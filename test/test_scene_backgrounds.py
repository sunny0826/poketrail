import json
import re
import unittest
from pathlib import Path

from PIL import Image, ImageEnhance

from tools.build_scene_backgrounds import (
    DIM_FACTOR, HEIGHT, LAYERS, MANIFEST, OUT_HEADER, ROUTES, TRANSPARENT_KEY, WIDTH,
    generate_all, image_pixels, layer_pixels, pixel_digest, to_rgb565,
)
from tools.design_preview import pixel_scene, pixel_scene_source, rgb565
from tools.design_preview_scenes import (
    GROUND_TOP, HEIGHT as SCENE_HEIGHT, MIDDLE_PIXELS_PER_STEP, NEAR_PIXELS_PER_STEP,
    WIDTH as SCENE_WIDTH, ground_height, pixel_scene_layers, pixel_scene_motion,
)

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
            self.assertEqual(entry["layers"]["groundTop"], GROUND_TOP[route])
            for layer in LAYERS:
                self.assertEqual(entry["layers"][f"{layer}Sha256"],
                                 pixel_digest(layer_pixels(route, layer)))
        self.assertEqual(manifest["transparentKey"], TRANSPARENT_KEY)

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

    def test_generated_motion_layers_match_rgba_transparency(self):
        self.assertIn(f"kSceneTransparentKey = 0x{TRANSPARENT_KEY:04X}", self.header)
        for route in ROUTES:
            for layer in LAYERS:
                with self.subTest(route=route, layer=layer):
                    source = getattr(pixel_scene_layers(route), layer).convert("RGBA")
                    expected = [TRANSPARENT_KEY if alpha == 0 else to_rgb565(r, g, b)
                                for r, g, b, alpha in source.get_flattened_data()]
                    self.assertEqual(layer_pixels(route, layer), expected)
                    name = f"kScene{route.title()}{layer.title()}"
                    body = re.search(rf"const uint16_t {name}\[\] = \{{(.*?)\}};", self.source, re.S).group(1)
                    actual = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]{4}", body)]
                    self.assertEqual(actual, expected)
                    if layer == "far":
                        self.assertNotIn(TRANSPARENT_KEY, actual)
                    else:
                        self.assertIn(TRANSPARENT_KEY, actual)
            self.assertIn(f"{GROUND_TOP[route]}}},", self.source)

    def test_thumbnail_is_the_middle_source_crop(self):
        for route in ROUTES:
            source = pixel_scene_source(route)
            thumbnail = source.crop((0, 18, WIDTH, 108))
            reference = pixel_scene(route).crop((0, 36, 456, 216)).resize(
                (WIDTH, 90), Image.Resampling.NEAREST)
            self.assertEqual(thumbnail.tobytes(), reference.tobytes())

    def test_motion_layers_preserve_static_source_and_repeat_seamlessly(self):
        for route in ROUTES:
            with self.subTest(route=route):
                layers = pixel_scene_layers(route)
                for layer in (layers.far, layers.middle, layers.near):
                    self.assertEqual(layer.size, (SCENE_WIDTH, SCENE_HEIGHT))
                self.assertEqual(pixel_scene_motion(route).tobytes(), pixel_scene_source(route).tobytes())
                self.assertEqual(pixel_scene_motion(route, ground_height(route)).tobytes(),
                                 pixel_scene_source(route).tobytes())
                self.assertEqual(pixel_scene_motion(route, 17).tobytes(),
                                 pixel_scene_motion(route, 17).tobytes())
                self.assertNotEqual(pixel_scene_motion(route, 1).tobytes(),
                                    pixel_scene_source(route).tobytes())

    def test_motion_offsets_move_ground_down_without_moving_far_scene(self):
        for route in ROUTES:
            layers = pixel_scene_layers(route)
            band_height = ground_height(route)
            for offset in (-5, 0, 1, 17, 48, 49, 113, 227):
                with self.subTest(route=route, offset=offset):
                    expected = layers.far.convert("RGBA")
                    for layer, speed in ((layers.middle, MIDDLE_PIXELS_PER_STEP),
                                         (layers.near, NEAR_PIXELS_PER_STEP)):
                        band = layer.crop((0, GROUND_TOP[route], SCENE_WIDTH, SCENE_HEIGHT))
                        tiled = Image.new("RGBA", (SCENE_WIDTH, band_height * 3))
                        for index in range(3):
                            tiled.paste(band, (0, index * band_height))
                        start = band_height - (offset * speed) % band_height
                        expected.alpha_composite(tiled.crop((0, start, SCENE_WIDTH, start + band_height)),
                                                 (0, GROUND_TOP[route]))
                    actual = pixel_scene_motion(route, offset)
                    self.assertEqual(actual.mode, "RGB")
                    self.assertEqual(actual.size, (SCENE_WIDTH, SCENE_HEIGHT))
                    self.assertEqual(actual.tobytes(), expected.convert("RGB").tobytes())
                    self.assertEqual(actual.crop((0, 0, SCENE_WIDTH, GROUND_TOP[route])).tobytes(),
                                     pixel_scene_source(route).crop((0, 0, SCENE_WIDTH, GROUND_TOP[route])).tobytes())

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
