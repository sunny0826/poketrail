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
    CACHE_DIR, MANIFESTS, OUT_HEADER, POKEMON_COUNT,
    SPRITE_URL, TRANSPARENT_KEY, build_animation, frame_colors,
    generate_header, generate_source, generate_manifest, load_sprites,
)

ROOT = Path(__file__).resolve().parent.parent
SOURCES = (
    'tools/build_sprites.py',
    *(f'assets/pokedex-gen{generation}.json' for generation in range(1, 6)),
    'assets/sprite-scales.json',
    *(f'assets/animations-gen{generation}.json' for generation in range(1, 6)),
)


class GeneratorCliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for relative in (*SOURCES, 'assets/sprites/black-white/0025.gif'):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        self.header = self.root / OUT_HEADER.relative_to(ROOT)
        self.header.parent.mkdir(parents=True)
        self.protected = (self.header, self.header.with_suffix('.cpp'))
        for path in self.protected:
            path.write_text('full pokedex must remain intact\n')

    def run_generator(self, *args):
        return subprocess.run([sys.executable, str(self.root / 'tools/build_sprites.py'), *args],
                              cwd=self.root, text=True, capture_output=True)

    def generate(self, *args):
        return self.run_generator('--ids', '25', *args)

    def check_output(self, output, count):
        source = output.parent / 'check.cpp'
        source.write_text('#include "sprites_generated.hpp"\n'
                          '#include <cassert>\n'
                          f'int main() {{ assert(pokedex::data::kSpriteCount == {count}); }}\n')
        binary = output.parent / 'check'
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++11',
                        '-I' + str(ROOT / 'firmware/apps/20_pokedex'),
                        str(source), str(output.with_suffix('.cpp')), '-o', str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)

    def test_subset_cannot_overwrite_default_outputs(self):
        before = [path.read_bytes() for path in self.protected]
        for args in ((), ('--out', str(self.header)),
                     ('--out', str(self.header.relative_to(self.root)))):
            with self.subTest(args=args):
                result = self.generate(*args)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn('--out', result.stderr)
                self.assertEqual([path.read_bytes() for path in self.protected], before)

    def test_generation_one_cannot_overwrite_default_outputs(self):
        before = [path.read_bytes() for path in self.protected]
        manifests = [self.root / path.relative_to(ROOT) for path in MANIFESTS.values()]
        before_manifests = [path.read_bytes() for path in manifests]
        for args in (('--generation', '1'),
                     ('--generation', '1', '--out', str(self.header)),
                     ('--generation', '1', '--out', str(self.header.relative_to(self.root)))):
            with self.subTest(args=args):
                result = self.run_generator(*args)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn('--out', result.stderr)
                self.assertEqual([path.read_bytes() for path in self.protected], before)
                self.assertEqual([path.read_bytes() for path in manifests], before_manifests)

    def test_generation_one_writes_explicit_separate_outputs(self):
        shutil.copytree(ROOT / 'assets/sprites/black-white',
                        self.root / 'assets/sprites/black-white', dirs_exist_ok=True)
        before = [path.read_bytes() for path in self.protected]
        manifests = [self.root / path.relative_to(ROOT) for path in MANIFESTS.values()]
        before_manifests = [path.read_bytes() for path in manifests]
        output = self.root / 'gen1' / 'sprites_generated.hpp'
        output.parent.mkdir()
        result = self.run_generator('--generation', '1', '--out', str(output))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_output(output, 151)
        self.assertEqual([path.read_bytes() for path in self.protected], before)
        self.assertEqual([path.read_bytes() for path in manifests], before_manifests)

    def test_subset_writes_explicit_separate_outputs(self):
        before = [path.read_bytes() for path in self.protected]
        manifests = [self.root / path.relative_to(ROOT) for path in MANIFESTS.values()]
        before_manifests = [path.read_bytes() for path in manifests]
        output = self.root / 'sprites_generated.hpp'
        result = self.generate('--out', str(output))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_output(output, 1)
        self.assertEqual([path.read_bytes() for path in self.protected], before)
        self.assertEqual([path.read_bytes() for path in manifests], before_manifests)


class SpriteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sprites = load_sprites(list(range(1, POKEMON_COUNT + 1)))

    def test_transparent_black_white_and_reserved_key(self):
        image = Image.new('RGBA', (3, 1))
        image.putdata([(123,45,67,0), (0,0,0,255), (255,255,255,255)])
        self.assertEqual(frame_colors(image), (TRANSPARENT_KEY, 0, 0xFFFF))
        image = Image.new('RGBA', (1,1), (255,0,255,255))
        with self.assertRaisesRegex(ValueError, 'transparent key'):
            frame_colors(image)

    def test_static_asset_rejected(self):
        output = io.BytesIO()
        Image.new('RGBA', (10,10), (0,0,0,255)).save(output, format='GIF', duration=100)
        with self.assertRaisesRegex(ValueError, 'distinct frames'):
            build_animation(output.getvalue())

    def test_all_649_complete_animated_sources(self):
        self.assertIn('generation-v/black-white/animated/', SPRITE_URL)
        self.assertEqual([s['id'] for s in self.sprites], list(range(1, 650)))
        for number in range(1, 650):
            with self.subTest(id=number), Image.open(CACHE_DIR / f'{number:04d}.gif') as image:
                frames = set()
                for index in range(image.n_frames):
                    image.seek(index)
                    self.assertGreater(image.info['duration'], 0)
                    frames.add(image.convert('RGBA').tobytes())
                self.assertGreater(len(frames), 1)

    def test_generated_files_and_manifests_are_current(self):
        self.assertEqual(OUT_HEADER.read_text(), generate_header(self.sprites))
        self.assertEqual(OUT_HEADER.with_suffix('.cpp').read_text(), generate_source(self.sprites))
        for generation, path in MANIFESTS.items():
            ranges = {1: (0, 151), 2: (151, 251), 3: (251, 386), 4: (386, 493), 5: (493, 649)}
            first, last = ranges[generation]
            self.assertEqual(path.read_text(), generate_manifest(self.sprites[first:last]))
            for r in json.loads(path.read_text())['sprites']:
                actual = hashlib.sha256((CACHE_DIR / f'{r["id"]:04d}.gif').read_bytes()).hexdigest()
                self.assertEqual(r['sha256'], actual)

    def test_firmware_decoder_and_timing_match_every_gif_frame(self):
        build = ROOT / '.pio/animation-test'
        build.mkdir(parents=True, exist_ok=True)
        executable = build / 'test_decode'
        subprocess.run([os.environ.get('CXX','c++'), '-std=c++11', '-O2',
                        'test/test_decode.cpp', 'firmware/apps/20_pokedex/sprite.cpp',
                        'firmware/apps/20_pokedex/game.cpp',
                        'firmware/apps/20_pokedex/sprites_generated.cpp', '-lz',
                        '-o', str(executable)], cwd=ROOT, check=True)
        output = subprocess.check_output([str(executable)], text=True)
        records = iter(output.splitlines())
        checked = 0
        for number in range(1, POKEMON_COUNT + 1):
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
