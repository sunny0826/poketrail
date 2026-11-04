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
    CACHE_DIR, MANIFESTS, OUT_HEADER, POKEMON_COUNT, SPRITE_REVISION,
    build_icon,
)

ROOT = Path(__file__).resolve().parent.parent


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
    table = re.search(r'const Icon kPokemonIcons\[649\] = \{(.*?)\};', source, re.S)
    entries = re.findall(r'\{(\d+), (\d+), (\w+)\}', table.group(1))
    ball = re.search(r'const Icon kPokeBallIcon = \{(\d+), (\d+), (\w+)\};', source)
    return arrays, entries, ball.groups()


def copy_icons_fixture(root):
    for relative in ('tools/build_icons.py', *(f'assets/icons-gen{generation}.json'
                                                 for generation in range(1, 6))):
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

    def test_all_649_ids_complete_and_unique(self):
        entries = [entry for manifest in self.manifests.values()
                   for entry in manifest['icons']]
        ids = [entry['id'] for entry in entries]
        self.assertEqual(sorted(ids), list(range(1, POKEMON_COUNT + 1)))

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
    """生成物与固定来源 PNG 逐像素一致。"""

    @classmethod
    def setUpClass(cls):
        cls.arrays, cls.entries, cls.ball = parse_generated_arrays()

    def test_every_pokemon_array_matches_source_png(self):
        self.assertEqual(len(self.entries), 649)
        for number in range(1, POKEMON_COUNT + 1):
            with self.subTest(id=number):
                size, pixels = convert_png_independent(CACHE_DIR / f'{number:04d}.png')
                width, height, array = self.entries[number - 1]
                self.assertEqual((int(width), int(height)), size)
                self.assertEqual(self.arrays[array], pixels)

    def test_poke_ball_array_matches_source_png(self):
        width, height, array = self.ball
        size, pixels = convert_png_independent(CACHE_DIR / 'poke-ball.png')
        self.assertEqual((int(width), int(height)), size)
        self.assertEqual(self.arrays[array], pixels)

class ManifestRequirementTests(unittest.TestCase):
    """发布生成必须有来源清单；只有显式 --write-manifests 可重建。"""

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
        for relative in ('tools/build_icons.py', *(f'assets/icons-gen{generation}.json'
                                                     for generation in range(1, 6))):
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
