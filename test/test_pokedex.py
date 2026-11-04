import csv
import hashlib
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.build_pokedex import EN_SOURCE, FILES, REVISION, SOURCE, build, load_sources


class SourceManifestTests(unittest.TestCase):
    def test_pinned_revision_hashes_and_urls(self):
        manifest, _ = load_sources()
        self.assertEqual(manifest['revision'], REVISION)
        for name in FILES:
            raw = (SOURCE / name).read_bytes()
            meta = manifest['sources'][name]
            self.assertEqual(hashlib.sha256(raw).hexdigest(), meta['sha256'])
            self.assertEqual(len(raw), meta['bytes'])
            self.assertIn(f'/pokeapi/{REVISION}/data/v2/csv/{name}', meta['url'])

    def test_tampered_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            for name in (*FILES, 'manifest.json'):
                shutil.copyfile(SOURCE / name, source / name)
            path = source / 'pokemon.csv'
            path.write_bytes(path.read_bytes() + b'\n')
            with self.assertRaisesRegex(ValueError, 'hash/size'):
                build(source)


class EnglishMetadataTests(unittest.TestCase):
    def test_all_649_names_and_types_match_pinned_csv(self):
        def rows(name):
            with (SOURCE / name).open(encoding='utf-8') as handle:
                return list(csv.DictReader(handle))

        names = {int(row['pokemon_species_id']): row['name']
                 for row in rows('pokemon_species_names.csv') if row['local_language_id'] == '9'}
        types = {row['name'] for row in rows('type_names.csv') if row['local_language_id'] == '9'}
        lines = EN_SOURCE.read_text().splitlines()
        entries = [json.loads('[' + line.strip()[1:-2] + ']')
                   for line in lines if line.startswith('  {')]
        self.assertEqual(len(entries), 649)
        for number, (name, labels) in enumerate(entries, 1):
            with self.subTest(id=number):
                self.assertEqual(name, names[number])
                self.assertTrue(labels)
                self.assertTrue(set(labels.split(' / ')) <= types)


if __name__ == '__main__':
    unittest.main()
