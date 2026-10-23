import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image
from tools.build_sprites import (
    CACHE_DIR, OUT_HEADER, MANIFEST, TRANSPARENT_KEY, SPRITE_URL,
    build_animation, encode_runs, frame_colors, generate_header,
    generate_source, generate_manifest, load_sprites, load_evolved_species,
)

ROOT = Path(__file__).resolve().parent.parent


class GeneratorCliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for relative in ('tools/build_sprites.py', 'assets/pokedex-gen1.json',
                         'assets/evolution-gen1.json', 'assets/animations-gen1.json',
                         'assets/sprites/black-white/0025.gif'):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        self.header = self.root / OUT_HEADER.relative_to(ROOT)
        self.header.parent.mkdir(parents=True)
        self.protected = (self.header, self.header.with_suffix('.cpp'))
        for path in self.protected:
            path.write_text('full pokedex must remain intact\n')

    def generate(self, *args):
        return subprocess.run([sys.executable, str(self.root / 'tools/build_sprites.py'),
                               '--ids', '25', *args], cwd=self.root,
                              text=True, capture_output=True)

    def test_subset_cannot_overwrite_default_outputs(self):
        before = [path.read_bytes() for path in self.protected]
        for args in ((), ('--out', str(self.header)),
                     ('--out', str(self.header.relative_to(self.root)))):
            with self.subTest(args=args):
                result = self.generate(*args)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn('--out', result.stderr)
                self.assertEqual([path.read_bytes() for path in self.protected], before)

    def test_subset_writes_explicit_separate_outputs(self):
        before = [path.read_bytes() for path in self.protected]
        manifest = self.root / 'assets/animations-gen1.json'
        original_manifest = manifest.read_bytes()
        output = self.root / 'sprites_generated.hpp'
        result = self.generate('--out', str(output))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('extern const Sprite kSprites[]', output.read_text())
        self.assertIn('kSpriteCount = 1;', output.with_suffix('.cpp').read_text())
        self.assertEqual([path.read_bytes() for path in self.protected], before)
        self.assertEqual(manifest.read_bytes(), original_manifest)


class SpriteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sprites = load_sprites(list(range(1, 152)))

    def test_transparent_black_white_and_reserved_key(self):
        image = Image.new('RGBA', (3, 1))
        image.putdata([(123,45,67,0), (0,0,0,255), (255,255,255,255)])
        self.assertEqual(frame_colors(image), (TRANSPARENT_KEY, 0, 0xFFFF))
        image = Image.new('RGBA', (1,1), (255,0,255,255))
        with self.assertRaisesRegex(ValueError, 'transparent key'):
            frame_colors(image)

    def test_runs_preserve_long_spans_and_reject_invalid_index(self):
        original = bytes([0]*33 + [15]*17 + [1,2,3])
        decoded = bytearray()
        for run in encode_runs(original):
            decoded.extend([run & 15] * ((run >> 4)+1))
        self.assertEqual(decoded, original)
        with self.assertRaises(ValueError):
            encode_runs(bytes([16]))

    def test_static_asset_rejected(self):
        output = io.BytesIO()
        Image.new('RGBA', (10,10), (0,0,0,255)).save(output, format='GIF', duration=100)
        with self.assertRaisesRegex(ValueError, 'distinct frames'):
            build_animation(output.getvalue())

    def test_all_151_complete_animated_sources(self):
        self.assertIn('generation-v/black-white/animated/', SPRITE_URL)
        self.assertEqual([s['id'] for s in self.sprites], list(range(1,152)))
        for s in self.sprites:
            with self.subTest(id=s['id']):
                self.assertGreater(s['unique'], 1)
                self.assertGreaterEqual(len(s['frames']), s['unique'])
                self.assertLessEqual(len(s['palette']),16)
                self.assertGreater(s['duration'], 0)

    def test_intermediate_and_final_forms_use_first_generation_relationships(self):
        evolved = {s['id'] for s in self.sprites if s['has_evolved']}
        self.assertEqual(len(evolved), 72)
        self.assertTrue({2, 5, 8, 11, 14, 17, 30, 33, 44, 61, 64, 67, 70, 75, 93, 148} <= evolved)
        self.assertTrue({3, 6, 9, 26, 42, 130, 134, 135, 136, 149} <= evolved)
        self.assertTrue(evolved.isdisjoint({1, 4, 7, 25, 106, 113, 122, 124, 143, 150, 151}))

    def test_evolution_metadata_rejects_invalid_relationships(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'evolution.json'
            original = json.loads((ROOT / 'assets/evolution-gen1.json').read_text())
            for invalid in (999, 3, '2', True):
                broken = json.loads(json.dumps(original))
                broken['parents']['3'] = invalid
                path.write_text(json.dumps(broken))
                with self.subTest(parent=invalid), self.assertRaises(ValueError):
                    load_evolved_species(path)
            original['parents']['1'] = 3
            path.write_text(json.dumps(original))
            with self.assertRaisesRegex(ValueError, 'cycle'):
                load_evolved_species(path)
            del original['parents']['1']
            path.write_text(json.dumps(original))
            with self.assertRaisesRegex(ValueError, '151'):
                load_evolved_species(path)

    def test_generated_files_and_manifest_are_current(self):
        self.assertEqual(OUT_HEADER.read_text(), generate_header(self.sprites))
        self.assertEqual(OUT_HEADER.with_suffix('.cpp').read_text(), generate_source(self.sprites))
        self.assertEqual(MANIFEST.read_text(), generate_manifest(self.sprites))
        records = json.loads(MANIFEST.read_text())['sprites']
        for r in records:
            actual = hashlib.sha256((CACHE_DIR / f'{r["id"]:04d}.gif').read_bytes()).hexdigest()
            self.assertEqual(r['sha256'], actual)

    def test_firmware_decoder_and_timing_match_every_gif_frame(self):
        build = ROOT / '.pio/animation-test'
        build.mkdir(parents=True, exist_ok=True)
        executable = build / 'test_decode'
        subprocess.run([os.environ.get('CXX','c++'), '-std=c++11', '-O2',
                        'test/test_decode.cpp', 'firmware/apps/20_pokedex/sprite.cpp',
                        'firmware/apps/20_pokedex/game.cpp',
                        'firmware/apps/20_pokedex/sprites_generated.cpp',
                        '-o', str(executable)], cwd=ROOT, check=True)
        output = subprocess.check_output([str(executable)], text=True)
        records = iter(output.splitlines())
        checked = 0
        for number in range(1,152):
            image = Image.open(CACHE_DIR / f'{number:04d}.gif')
            for index in range(image.n_frames):
                image.seek(index)
                # 独立从原 GIF 合成帧计算，不读取生成器的RLE结果。
                fingerprint = 14695981039346656037
                for r,g,b,a in image.convert('RGBA').get_flattened_data():
                    value = 0xF81F if a < 128 else ((r//8)<<11)|((g//4)<<5)|(b//8)
                    for byte in (value & 255, value >> 8):
                        fingerprint = ((fingerprint ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
                record = next(records).split()
                self.assertEqual(list(map(int,record[:5])),
                                 [number,index,*image.size,image.info['duration']])
                self.assertEqual(int(record[5],16),fingerprint, f'#{number} frame {index}')
                checked += 1
            image.close()
        self.assertEqual(list(records), [])
        print(f'Firmware decoding and timing match {checked} original GIF frames')


if __name__ == '__main__':
    unittest.main()
