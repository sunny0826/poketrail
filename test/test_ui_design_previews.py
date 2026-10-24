import json
import re
import unittest
from pathlib import Path

from PIL import Image, ImageChops, ImageSequence

from tools.design_preview import GeneratedAssets, pixel_scene_source
from tools.design_preview_battle import (
    ANIMATIONS, DURATION_MS, FRAME_MS, battle_state, source_frame,
)

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/design/ui-scene-redesign"


class DesignPreviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((OUT / "manifest.json").read_text(encoding="utf-8"))
        cls.frames = cls.manifest["frames"]
        cls.by_id = {frame["id"]: frame for frame in cls.frames}
        cls.animations = cls.manifest["animations"]
        cls.assets = GeneratedAssets()

    def test_every_firmware_page_is_covered(self):
        source = (ROOT / "firmware/apps/20_pokedex/screens.hpp").read_text(encoding="utf-8")
        body = re.search(r"enum class Page[^{]*\{(.*?)\};", source, re.S).group(1)
        expected = {item.strip() for item in body.replace("\n", " ").split(",") if item.strip()}
        actual = {frame["page"] for frame in self.frames}
        self.assertEqual(actual, expected)
        self.assertEqual(len(expected), 25)

    def test_ids_groups_and_references_are_complete(self):
        self.assertEqual(len(self.by_id), len(self.frames))
        self.assertEqual({frame["group"] for frame in self.frames},
                         {"exploration", "capture", "partners", "records", "system"})
        for frame in self.frames:
            with self.subTest(frame=frame["id"]):
                self.assertTrue((OUT / "frames" / f"{frame['id']}.png").is_file())

    def test_routes_and_capture_keyframes_are_covered(self):
        scenes = [frame for frame in self.frames if frame.get("scene_only")]
        self.assertEqual({frame["route"] for frame in scenes}, {"forest", "coast", "cave"})
        stages = {frame.get("stage") for frame in self.frames if frame.get("stage")}
        self.assertEqual(stages, {"flight", "flash", "drop", "wiggle",
                                  "reveal-success", "reveal-failure"})

    def test_native_images_are_nonempty_and_clipped_to_safe_circle(self):
        outside = Image.new("L", (466, 466), 255)
        from PIL import ImageDraw
        ImageDraw.Draw(outside).ellipse((5, 5, 461, 461), fill=0)
        self.assertEqual(self.manifest["nativeSize"], [466, 466])
        self.assertEqual(self.manifest["safeRadius"], 228)
        for frame in self.frames:
            with self.subTest(frame=frame["id"]), Image.open(OUT / "frames" / f"{frame['id']}.png") as image:
                self.assertEqual(image.format, "PNG")
                self.assertEqual(image.size, (466, 466))
                self.assertEqual(image.mode, "RGB")
                self.assertIsNotNone(image.getbbox())
                self.assertIsNone(ImageChops.multiply(image, outside.convert("RGB")).getbbox())

    def test_home_uses_exact_generated_sprite_pixels(self):
        expected = self.assets.sprite(4)
        with Image.open(OUT / "frames/home-forest.png") as image:
            left, top = 233 - expected.width // 2, 248 - expected.height // 2
            actual = image.crop((left, top, left + expected.width, top + expected.height))
            for source, target in zip(expected.get_flattened_data(), actual.get_flattened_data()):
                if source[3]:
                    self.assertEqual(source[:3], target)

    def test_assets_use_integer_nearest_neighbor_scaling(self):
        icon = self.assets.icon(1, 2)
        self.assertEqual(icon.size, (80, 60))
        meta = self.assets.meta[4]
        sprite = self.assets.sprite(4)
        self.assertEqual(sprite.size, (meta["width"] * 2, meta["height"] * 2))

    def test_refined_scene_source_canvas(self):
        for route in ("forest", "coast", "cave"):
            with self.subTest(route=route):
                self.assertEqual(pixel_scene_source(route).size, (228, 135))

    def test_battle_animation_files_and_timing(self):
        from PIL import ImageDraw
        outside = Image.new("L", (466, 466), 255)
        ImageDraw.Draw(outside).ellipse((5, 5, 461, 461), fill=0)
        self.assertEqual({item["id"] for item in self.animations},
                         {"battle-attack", "battle-dodge", "battle-perfect"})
        for spec in self.animations:
            with self.subTest(animation=spec["id"]), Image.open(OUT / spec["file"]) as image:
                self.assertEqual(image.format, "GIF")
                self.assertEqual(image.size, (466, 466))
                self.assertEqual(image.n_frames, 30)
                self.assertEqual(image.info.get("loop"), 0)
                durations = []
                unique = set()
                for frame in ImageSequence.Iterator(image):
                    durations.append(frame.info.get("duration"))
                    rgb = frame.convert("RGB")
                    unique.add(rgb.tobytes())
                    self.assertIsNone(ImageChops.multiply(rgb, outside.convert("RGB")).getbbox())
                self.assertEqual(durations, [50] * 30)
                self.assertEqual(sum(durations), DURATION_MS)
                self.assertEqual(spec["frameMs"], FRAME_MS)
                self.assertEqual(spec["frameCount"], 30)
                self.assertEqual(spec["durationMs"], DURATION_MS)
                self.assertTrue(spec["loop"])
                self.assertGreater(len(unique), 20)

    def test_battle_keyframe_states_and_hp(self):
        manifest = {item["id"]: item for item in self.animations}
        for spec in ANIMATIONS:
            with self.subTest(animation=spec["id"]):
                early = battle_state(spec, 650)
                late = battle_state(spec, 900)
                self.assertEqual(early["enemyHp"], 3)
                self.assertEqual(early["playerHp"], 3)
                self.assertEqual(late["message"], manifest[spec["id"]]["judgement"])
                self.assertEqual(late["playerHp"], manifest[spec["id"]]["playerHp"])
                self.assertEqual(late["enemyHp"], manifest[spec["id"]]["enemyHp"])
                if spec["action"] == "attack":
                    self.assertGreater(battle_state(spec, 800)["playerOffset"], 0)
                    self.assertGreater(late["enemyOffset"], 0)
                    self.assertEqual(late["enemyHp"], 1 if spec["variant"] == "perfect" else 2)
                else:
                    self.assertLess(battle_state(spec, 850)["playerOffset"], 0)
                    self.assertLess(late["enemyOffset"], 0)
                    self.assertEqual(late["playerHp"], 3)
        perfect = ANIMATIONS[2]
        self.assertFalse(battle_state(perfect, 600)["perfectWindow"])
        self.assertTrue(battle_state(perfect, 750)["perfectWindow"])
        self.assertFalse(battle_state(perfect, 900)["perfectWindow"])

    def test_original_sprite_timing_does_not_restart(self):
        animations = {4: self.assets.sprite_frames(4, 2)}
        total = sum(duration for _, duration in animations[4])
        for time_ms in (0, 50, 750, 850, 1600, 4000):
            position = time_ms % total
            index = 0
            while position >= animations[4][index][1]:
                position -= animations[4][index][1]
                index += 1
            expected = self.assets.sprite(4, index, 2)
            self.assertEqual(source_frame(animations, 4, time_ms).tobytes(), expected.tobytes())

    def test_contacts_and_offline_gallery_cover_the_manifest(self):
        for name in ("overview", "exploration", "capture", "partners", "records", "system", "battle-animation"):
            with self.subTest(contact=name), Image.open(OUT / "contacts" / f"{name}.png") as image:
                self.assertGreater(image.width, 900)
                self.assertGreater(image.height, 700)
        gallery = (OUT / "index.html").read_text(encoding="utf-8")
        gallery += "\n".join((OUT / f"{name}.html").read_text(encoding="utf-8")
                             for name in ("exploration", "capture", "battle-animation",
                                          "partners", "records", "system"))
        self.assertNotIn("<script", gallery)
        self.assertNotRegex(gallery, r"(?:src|href)=\"https?://")
        self.assertIn('href="battle-animation.html"', gallery)
        for frame in self.frames:
            self.assertIn(f'src="frames/{frame["id"]}.png"', gallery)
            self.assertIn(f'href="frames/{frame["id"]}.png"', gallery)
        for animation in self.animations:
            self.assertIn(f'src="{animation["file"]}"', gallery)
            self.assertIn(f'href="{animation["file"]}"', gallery)

    def test_design_document_links_and_frame_count(self):
        document = OUT.with_suffix(".md")
        text = document.read_text(encoding="utf-8")
        self.assertIn(f"查看 {len(self.frames)} 帧与 {len(self.animations)} 条动画", text)
        for name in ("pages", "motion", "implementation"):
            text += (OUT.parent / f"ui-scene-redesign-{name}.md").read_text(encoding="utf-8")
        links = re.findall(r"!?\[[^]]*\]\(([^)]+)\)", text)
        self.assertGreater(len(links), 50)
        for link in links:
            with self.subTest(link=link):
                self.assertTrue((document.parent / link).is_file())


if __name__ == "__main__":
    unittest.main()
