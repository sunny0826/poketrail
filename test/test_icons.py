import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image
from tools.build_icons import (
    CACHE_DIR, GEN1_COUNT, MANIFESTS, OUT_HEADER, POKEMON_COUNT, SPRITE_REVISION,
    TRANSPARENT_KEY, build_icon, generate_header, generate_source, load_icons,
)

ROOT = Path(__file__).resolve().parent.parent
SPRITE_HPP = ROOT / 'firmware/apps/20_pokedex/sprite.hpp'


def convert_png_independent(path):
    """独立从 PNG 计算 RGB565 像素，不复用生成器实现。"""
    image = Image.open(path)
    pixels = []
    for r, g, b, a in image.convert('RGBA').get_flattened_data():
        if a < 128:
            pixels.append(0xF81F)
        else:
            value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            assert value != 0xF81F, f'{path.name}: opaque pixel hits transparent key'
            pixels.append(value)
    return image.size, pixels


def parse_generated_arrays():
    """从生成物解析像素数组与图标表，作为被测对象。"""
    source = OUT_HEADER.with_suffix('.cpp').read_text(encoding='utf-8')
    arrays = {}
    for match in re.finditer(r'const uint16_t (\w+)\[\] = \{(.*?)\};', source, re.S):
        name, body = match.groups()
        arrays[name] = [int(token, 16) for token in re.findall(r'0x[0-9A-Fa-f]{4}', body)]
    table = re.search(r'const Icon kPokemonIcons\[251\] = \{(.*?)\};', source, re.S)
    entries = re.findall(r'\{(\d+), (\d+), (\w+)\}', table.group(1))
    ball = re.search(r'const Icon kPokeBallIcon = \{(\d+), (\d+), (\w+)\};', source)
    return arrays, entries, ball.groups()


def copy_icons_fixture(root):
    for relative in ('tools/build_icons.py', 'assets/icons-gen1.json',
                     'assets/icons-gen2.json'):
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, destination)
    (root / 'assets/sprites').mkdir(parents=True, exist_ok=True)
    (root / 'assets/sprites/icons').symlink_to(CACHE_DIR, target_is_directory=True)


class ManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifests = {g: json.loads(p.read_text()) for g, p in MANIFESTS.items()}

    def test_sources_pinned_to_fixed_revision(self):
        for generation, manifest in self.manifests.items():
            with self.subTest(generation=generation):
                self.assertEqual(manifest['revision'], SPRITE_REVISION)
                self.assertIn(SPRITE_REVISION, manifest['source'])
                self.assertIn('generation-vii/icons/', manifest['pokemon_path'])
        self.assertEqual(self.manifests[1]['poke_ball_path'], 'sprites/items/poke-ball.png')

    def test_all_251_ids_complete_and_unique(self):
        entries = [entry for manifest in self.manifests.values()
                   for entry in manifest['icons']]
        ids = [entry['id'] for entry in entries]
        self.assertEqual(sorted(ids), list(range(1, POKEMON_COUNT + 1)))
        self.assertEqual(len(ids), len(set(ids)))
        for entry in entries:
            with self.subTest(id=entry['id']):
                self.assertRegex(entry['sha256'], r'^[0-9a-f]{64}$')
                self.assertGreater(entry['width'], 0)
                self.assertGreater(entry['height'], 0)

    def test_generation_ranges_are_split(self):
        self.assertEqual([e['id'] for e in self.manifests[1]['icons']],
                         list(range(1, GEN1_COUNT + 1)))
        self.assertEqual([e['id'] for e in self.manifests[2]['icons']],
                         list(range(GEN1_COUNT + 1, POKEMON_COUNT + 1)))
        self.assertNotIn('poke_ball', self.manifests[2])

    def test_poke_ball_entry_present(self):
        ball = self.manifests[1]['poke_ball']
        self.assertRegex(ball['sha256'], r'^[0-9a-f]{64}$')
        self.assertGreater(ball['width'], 0)
        self.assertGreater(ball['height'], 0)


class SourceAssetTests(unittest.TestCase):
    """直接核对缓存 PNG 本身：哈希、尺寸、透明与颜色规则。"""

    def check_icon(self, path, entry):
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), entry['sha256'])
        image = Image.open(path)
        self.assertEqual(image.format, 'PNG')
        self.assertEqual(image.size, (entry['width'], entry['height']))
        transparent = 0
        for r, g, b, a in image.convert('RGBA').get_flattened_data():
            self.assertIn(a, (0, 255), f'{path.name}: partial alpha {a}')
            if a < 128:
                transparent += 1
            else:
                value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                self.assertNotEqual(value, 0xF81F, f'{path.name}: opaque pixel hits key')
        self.assertGreater(transparent, 0, f'{path.name}: no transparent pixels')

    def test_all_cached_pngs_match_manifests(self):
        for manifest in MANIFESTS.values():
            data = json.loads(manifest.read_text())
            for entry in data['icons']:
                with self.subTest(id=entry['id']):
                    path = CACHE_DIR / f'{entry["id"]:04d}.png'
                    self.assertTrue(path.exists(), f'missing {path}')
                    self.check_icon(path, entry)
        with self.subTest(id='poke-ball'):
            ball = json.loads(MANIFESTS[1].read_text())['poke_ball']
            self.check_icon(CACHE_DIR / 'poke-ball.png', ball)

    def test_pokemon_icons_expected_canvas(self):
        for number in range(1, POKEMON_COUNT + 1):
            with Image.open(CACHE_DIR / f'{number:04d}.png') as image:
                self.assertEqual(image.size, (40, 30), f'#{number}')


class GeneratedCodeTests(unittest.TestCase):
    """生成物与源 PNG 逐像素一致，并符合协调者接口约定。"""

    @classmethod
    def setUpClass(cls):
        cls.header = OUT_HEADER.read_text(encoding='utf-8')
        cls.arrays, cls.entries, cls.ball = parse_generated_arrays()

    def test_header_declares_contract(self):
        self.assertIn('#include <stdint.h>', self.header)
        self.assertIn('namespace pokedex {', self.header)
        self.assertIn('namespace data {', self.header)
        self.assertRegex(self.header,
                         r'struct Icon \{\s*uint16_t width;\s*uint16_t height;'
                         r'\s*const uint16_t\* pixels;\s*\};')
        self.assertIn('extern const Icon kPokemonIcons[251];', self.header)
        self.assertIn('extern const Icon kPokeBallIcon;', self.header)

    def test_transparent_key_matches_sprite_hpp(self):
        sprite = SPRITE_HPP.read_text(encoding='utf-8')
        self.assertRegex(sprite, r'kTransparentKey = 0xF81F')
        self.assertEqual(TRANSPARENT_KEY, 0xF81F)

    def test_table_has_251_entries_in_id_order(self):
        self.assertEqual(len(self.entries), POKEMON_COUNT)
        for index, (width, height, array) in enumerate(self.entries, start=1):
            with self.subTest(id=index):
                self.assertEqual(array, f'kIcon{index:04d}Pixels')
                self.assertIn(array, self.arrays)
                self.assertEqual(int(width) * int(height), len(self.arrays[array]))

    def test_every_pokemon_array_matches_source_png(self):
        for number in range(1, POKEMON_COUNT + 1):
            with self.subTest(id=number):
                size, pixels = convert_png_independent(CACHE_DIR / f'{number:04d}.png')
                width, height, array = self.entries[number - 1]
                self.assertEqual((int(width), int(height)), size)
                self.assertEqual(self.arrays[array], pixels)

    def test_poke_ball_array_matches_source_png(self):
        width, height, array = self.ball
        self.assertEqual(array, 'kPokeBallPixels')
        size, pixels = convert_png_independent(CACHE_DIR / 'poke-ball.png')
        self.assertEqual((int(width), int(height)), size)
        self.assertEqual(self.arrays[array], pixels)

    def test_generated_files_are_current(self):
        pokemon, poke_ball = load_icons(download=False, write_manifests=False)
        self.assertEqual(self.header, generate_header())
        self.assertEqual(OUT_HEADER.with_suffix('.cpp').read_text(encoding='utf-8'),
                         generate_source(pokemon, poke_ball))


class ManifestRequirementTests(unittest.TestCase):
    """普通生成与 --check 必须要求两代清单存在；只有 --write-manifests 可重建。"""

    def make_root(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        copy_icons_fixture(root)
        (root / 'firmware/apps/20_pokedex').mkdir(parents=True)
        return root

    def run_generator(self, root, *args):
        return subprocess.run([sys.executable, str(root / 'tools/build_icons.py'), *args],
                              cwd=root, text=True, capture_output=True)

    def test_missing_manifest_fails_for_each_generation(self):
        for removed in ((1,), (2,), (1, 2)):
            for args in ((), ('--check',), ('--download',)):
                with self.subTest(removed=removed, args=args):
                    root = self.make_root()
                    for generation in removed:
                        (root / f'assets/icons-gen{generation}.json').unlink()
                    result = self.run_generator(root, *args)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn(f'generation {removed[0]} icon manifest missing',
                                  result.stderr)
                    self.assertFalse(
                        (root / 'firmware/apps/20_pokedex/icons_generated.cpp').exists())

    def test_write_manifests_is_the_only_rebuild_bypass(self):
        root = self.make_root()
        originals = {path: path.read_bytes()
                     for path in (root / 'assets/icons-gen1.json',
                                  root / 'assets/icons-gen2.json')}
        (root / 'assets/icons-gen2.json').unlink()
        result = self.run_generator(root, '--write-manifests')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, original in originals.items():
            self.assertEqual(path.read_bytes(), original)
        self.assertTrue(
            (root / 'firmware/apps/20_pokedex/icons_generated.cpp').exists())

    def test_poke_ball_entry_and_hash_are_enforced(self):
        for mutate in ('missing_entry', 'wrong_hash'):
            with self.subTest(mutate=mutate):
                root = self.make_root()
                path = root / 'assets/icons-gen1.json'
                manifest = json.loads(path.read_text())
                if mutate == 'missing_entry':
                    manifest.pop('poke_ball')
                    expected = 'missing poke_ball entry'
                else:
                    manifest['poke_ball']['sha256'] = '0' * 64
                    expected = 'poke-ball.png: SHA256'
                path.write_text(json.dumps(manifest))
                result = self.run_generator(root)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(expected, result.stderr)
                self.assertFalse(
                    (root / 'firmware/apps/20_pokedex/icons_generated.cpp').exists())


class GeneratorBehaviorTests(unittest.TestCase):
    def test_rejects_opaque_pixel_on_transparent_key(self):
        import io
        output = io.BytesIO()
        Image.new('RGBA', (2, 2), (255, 0, 255, 255)).save(output, format='PNG')
        with self.assertRaisesRegex(ValueError, 'transparent key'):
            build_icon(output.getvalue(), 'fake.png')

    def test_rejects_icon_without_transparency(self):
        import io
        output = io.BytesIO()
        Image.new('RGB', (2, 2), (0, 0, 0)).save(output, format='PNG')
        with self.assertRaisesRegex(ValueError, 'no transparent'):
            build_icon(output.getvalue(), 'fake.png')

    def test_rejects_non_png(self):
        import io
        output = io.BytesIO()
        Image.new('RGB', (2, 2), (0, 0, 0)).save(output, format='GIF')
        with self.assertRaisesRegex(ValueError, 'not a PNG'):
            build_icon(output.getvalue(), 'fake.png')

    def test_hash_mismatch_fails_fast_offline(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        copy_icons_fixture(root)
        manifest_path = root / 'assets/icons-gen1.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['icons'][0]['sha256'] = '0' * 64
        manifest_path.write_text(json.dumps(manifest))
        result = subprocess.run([sys.executable, str(root / 'tools/build_icons.py')],
                                cwd=root, text=True, capture_output=True)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('SHA256', result.stderr)
        self.assertFalse((root / 'firmware').exists())

    def test_missing_cache_requires_explicit_download_flag(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        for relative in ('tools/build_icons.py', 'assets/icons-gen1.json',
                         'assets/icons-gen2.json'):
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        result = subprocess.run([sys.executable, str(root / 'tools/build_icons.py')],
                                cwd=root, text=True, capture_output=True)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('--download', result.stderr)
        self.assertFalse((root / 'firmware').exists())


if __name__ == '__main__':
    unittest.main()
