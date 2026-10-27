import csv
import hashlib
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.build_pokedex import (
    DATA, FILES, LANGUAGE, REVISION, SOURCE, build, load_sources, validate,
)

ROOT = Path(__file__).resolve().parent.parent
GEN1 = ROOT / 'assets/pokedex-gen1.json'


def read_independent(source: Path) -> dict:
    def rows(name):
        with (source / name).open(encoding='utf-8') as handle:
            return list(csv.DictReader(handle))

    names = {int(r['pokemon_species_id']): r['name']
             for r in rows('pokemon_species_names.csv')
             if r['local_language_id'] == str(LANGUAGE)}
    types = {int(r['type_id']): r['name']
             for r in rows('type_names.csv')
             if r['local_language_id'] == str(LANGUAGE)}
    defaults = {int(r['species_id']): r for r in rows('pokemon.csv')
                if r['is_default'] == '1'}
    slots = {}
    for row in rows('pokemon_types.csv'):
        slots.setdefault(int(row['pokemon_id']), []).append(
            (int(row['slot']), int(row['type_id'])))
    result = {}
    for number in range(1, 252):
        base = defaults[number]
        result[number] = {
            'pokemon_name': names[number],
            'pokemon_type_name': ','.join(types[t] for _, t in sorted(slots[number])),
            'height': f"{int(base['height']) / 10:.1f} m",
            'weight': f"{int(base['weight']) / 10:.1f} kg",
        }
    return result


class SourceManifestTests(unittest.TestCase):
    def test_pinned_revision_hashes_and_urls(self):
        manifest, tables = load_sources()
        self.assertEqual(manifest['revision'], REVISION)
        self.assertEqual(manifest['language'], {'id': LANGUAGE, 'identifier': 'zh-hans'})
        self.assertEqual(set(manifest['sources']), set(FILES))
        for name in FILES:
            raw = (SOURCE / name).read_bytes()
            meta = manifest['sources'][name]
            self.assertEqual(hashlib.sha256(raw).hexdigest(), meta['sha256'])
            self.assertEqual(len(raw), meta['bytes'])
            self.assertIn(f'/pokeapi/{REVISION}/data/v2/csv/{name}', meta['url'])
            self.assertTrue(tables[name])

    def test_tampered_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            for name in (*FILES, 'manifest.json'):
                shutil.copyfile(SOURCE / name, source / name)
            path = source / 'pokemon.csv'
            path.write_bytes(path.read_bytes() + b'\n')
            with self.assertRaisesRegex(ValueError, 'hash/size'):
                build(source)


class Gen2MetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(DATA.read_text())
        cls.by_id = {int(e['zukan_id']): e for e in cls.data['pokemons']}

    def test_100_default_species_in_order(self):
        ids = [int(e['zukan_id']) for e in self.data['pokemons']]
        self.assertEqual(ids, list(range(152, 252)))
        self.assertEqual(len(ids), len(set(ids)))
        self.assertTrue(all(e['zukan_sub_id'] == 0 for e in self.data['pokemons']))
        self.assertEqual(self.by_id[201]['pokemon_name'], '未知图腾')

    def test_values_match_independent_csv_parse(self):
        expected = read_independent(SOURCE)
        for number, entry in self.by_id.items():
            with self.subTest(id=number):
                self.assertEqual(entry['pokemon_name'], expected[number]['pokemon_name'])
                self.assertEqual(entry['pokemon_type_name'],
                                 expected[number]['pokemon_type_name'])
                self.assertEqual(entry['height'], expected[number]['height'])
                self.assertEqual(entry['weight'], expected[number]['weight'])

    def test_known_samples_and_type_format(self):
        self.assertEqual(self.by_id[152]['pokemon_name'], '菊草叶')
        self.assertEqual(self.by_id[157]['pokemon_type_name'], '火')
        self.assertEqual(self.by_id[249]['height'], '5.2 m')
        self.assertEqual(self.by_id[249]['weight'], '216.0 kg')
        self.assertEqual(self.by_id[233]['pokemon_name'], '多边兽２型')
        for entry in self.by_id.values():
            self.assertGreaterEqual(len(entry['pokemon_type_name'].split(',')), 1)
            self.assertLessEqual(len(entry['pokemon_type_name'].split(',')), 2)
            self.assertRegex(entry['height'], r'^\d+\.\d m$')
            self.assertRegex(entry['weight'], r'^\d+\.\d kg$')

    def test_generation_one_snapshot_matches_same_source(self):
        expected = read_independent(SOURCE)
        gen1 = {int(e['zukan_id']): e for e in json.loads(GEN1.read_text())['pokemons']}
        self.assertEqual(set(gen1), set(range(1, 152)))
        for number, entry in gen1.items():
            with self.subTest(id=number):
                self.assertEqual(entry['pokemon_name'], expected[number]['pokemon_name'])
                self.assertEqual(entry['pokemon_type_name'],
                                 expected[number]['pokemon_type_name'])
                self.assertEqual(entry['height'], expected[number]['height'])
                self.assertEqual(entry['weight'], expected[number]['weight'])

    def test_generator_output_is_current(self):
        self.assertEqual(self.data, build())
        self.assertEqual(self.data, validate(self.data))


if __name__ == '__main__':
    unittest.main()
