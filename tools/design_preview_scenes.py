from __future__ import annotations

from dataclasses import dataclass

from PIL import Image, ImageChops, ImageDraw

if __package__:
    from tools.scene_hoenn import hoenn
    from tools.scene_johto import johto
    from tools.scene_kanto import kanto
    from tools.scene_paint import H as HEIGHT, W as WIDTH, layer
    from tools.scene_sinnoh import sinnoh
    from tools.scene_unova import unova
else:
    from scene_hoenn import hoenn
    from scene_johto import johto
    from scene_kanto import kanto
    from scene_paint import H as HEIGHT, W as WIDTH, layer
    from scene_sinnoh import sinnoh
    from scene_unova import unova

MIDDLE_PIXELS_PER_STEP = 1
NEAR_PIXELS_PER_STEP = 2
# Firmware region order: Route::Kanto..Route::Unova.
REGIONS = ("kanto", "johto", "hoenn", "sinnoh", "unova")
GROUND_TOP = {"kanto": 84, "johto": 84, "hoenn": 90, "sinnoh": 84, "unova": 86}
_PAINTERS = {"kanto": kanto, "johto": johto, "hoenn": hoenn, "sinnoh": sinnoh, "unova": unova}


def ground_height(route: str) -> int:
    return HEIGHT - GROUND_TOP[route]


@dataclass(frozen=True)
class SceneLayers:
    far: Image.Image
    middle: Image.Image
    near: Image.Image


def pixel_scene_layers(route: str) -> SceneLayers:
    if route not in _PAINTERS:
        raise ValueError(f"unknown route {route}")
    far, middle, near = layer(), layer(), layer()
    top = GROUND_TOP[route]
    _PAINTERS[route](far, middle, near, top)
    if far.getchannel("A").getextrema() != (255, 255):
        raise ValueError(f"{route} far layer leaves transparent pixels")
    # Only the ground band loops; anything above it would never be shown.
    for moving in (middle, near):
        ImageDraw.Draw(moving).rectangle((0, 0, WIDTH - 1, top - 1), fill=(0, 0, 0, 0))
    return SceneLayers(far.convert("RGB"), middle, near)


def ground_motion(layer: Image.Image, route: str, offset: int) -> Image.Image:
    top = GROUND_TOP[route]
    band = ImageChops.offset(layer.crop((0, top, WIDTH, HEIGHT)), 0, offset)
    shifted = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    shifted.paste(band, (0, top))
    return shifted


def pixel_scene_motion(route: str, visual_steps: int = 0) -> Image.Image:
    layers = pixel_scene_layers(route)
    middle = ground_motion(layers.middle, route, visual_steps * MIDDLE_PIXELS_PER_STEP)
    near = ground_motion(layers.near, route, visual_steps * NEAR_PIXELS_PER_STEP)
    image = layers.far.convert("RGBA")
    image.alpha_composite(middle)
    image.alpha_composite(near)
    return image.convert("RGB")


def pixel_scene_source(route: str) -> Image.Image:
    return pixel_scene_motion(route)


def pixel_scene(route: str, visual_steps: int = 0) -> Image.Image:
    return pixel_scene_motion(route, visual_steps).resize((456, 270), Image.Resampling.NEAREST)
