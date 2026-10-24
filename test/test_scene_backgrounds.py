import json
import re
import unittest
from pathlib import Path

from PIL import Image, ImageEnhance

from tools.build_scene_backgrounds import (
    DIM_FACTOR, HEIGHT, MANIFEST, OUT_HEADER, ROUTES, WIDTH, generate_all,
    image_pixels, pixel_digest, to_rgb565,
)
from tools.design_preview import pixel_scene, pixel_scene_source, rgb565

ROOT = Path(__file__).resolve().parent.parent


class SceneBackgroundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header, cls.source, cls.manifest = generate_all()
        cls.generated = (OUT_HEADER.read_text(encoding="utf-8"),
                         OUT_HEADER.with_suffix(".cpp").read_text(encoding="utf-8"),
                         MANIFEST.read_text(encoding="utf-8"))

    def test_generated_files_and_manifest_are_current_and_deterministic(self):
        self.assertEqual((self.header, self.source, self.manifest), self.generated)
        self.assertEqual(generate_all(), self.generated)
        manifest = json.loads(self.manifest)
        self.assertEqual(manifest["source"], "tools/design_preview_scenes.py#pixel_scene_source")
        self.assertEqual(manifest["dimFactor"], DIM_FACTOR)
        self.assertEqual([entry["route"] for entry in manifest["routes"]], list(ROUTES))
        for route, entry in zip(ROUTES, manifest["routes"]):
            self.assertEqual(entry["normalSha256"], pixel_digest(image_pixels(route)))
            self.assertEqual(entry["dimSha256"], pixel_digest(image_pixels(route, True)))

    def test_canvas_palette_and_route_tables(self):
        self.assertIn(f"kSceneWidth = {WIDTH}", self.header)
        self.assertIn(f"kSceneHeight = {HEIGHT}", self.header)
        for route in ROUTES:
            normal = image_pixels(route)
            dim = image_pixels(route, True)
            self.assertEqual(len(normal), WIDTH * HEIGHT)
            self.assertEqual(len(dim), WIDTH * HEIGHT)
            self.assertGreater(len(set(normal)), 20)
            self.assertNotEqual(normal, dim)
            self.assertIn(f"{{kScene{route.title()}}},", self.source)
            self.assertIn(f"{{kScene{route.title()}Dim}},", self.source)

    def test_generated_pixels_match_independent_rgb565_conversion(self):
        for route in ROUTES:
            source = pixel_scene_source(route)
            expected = [to_rgb565(r, g, b) for r, g, b in source.get_flattened_data()]
            self.assertEqual(image_pixels(route), expected)
            dimmed = ImageEnhance.Brightness(source).enhance(DIM_FACTOR)
            expected_dim = [to_rgb565(r, g, b) for r, g, b in dimmed.get_flattened_data()]
            self.assertEqual(image_pixels(route, True), expected_dim)
            name = f"kScene{route.title()}"
            body = re.search(rf"const uint16_t {name}\[\] = \{{(.*?)\}};", self.source, re.S).group(1)
            actual = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]{4}", body)]
            self.assertEqual(actual, expected)

    def test_thumbnail_is_the_middle_source_crop(self):
        for route in ROUTES:
            source = pixel_scene_source(route)
            thumbnail = source.crop((0, 18, WIDTH, 108))
            reference = pixel_scene(route).crop((0, 36, 456, 216)).resize(
                (WIDTH, 90), Image.Resampling.NEAREST)
            self.assertEqual(thumbnail.tobytes(), reference.tobytes())

    def test_row_expansion_preserves_nearest_neighbor_pixels(self):
        for route in ROUTES:
            source = image_pixels(route)
            expanded = []
            for y in range(HEIGHT):
                row = source[y * WIDTH:(y + 1) * WIDTH]
                expanded.extend([value for value in row for _ in range(2)] * 2)
            self.assertEqual(len(expanded), 456 * 270)
            for x, y in ((0, 0), (17, 18), (113, 67), (227, 134), (79, 101)):
                source_value = source[y * WIDTH + x]
                self.assertEqual(expanded[(y * 2) * 456 + x * 2], source_value)
                self.assertEqual(expanded[(y * 2 + 1) * 456 + x * 2 + 1], source_value)
                self.assertIsInstance(rgb565(source_value), tuple)


if __name__ == "__main__":
    unittest.main()
