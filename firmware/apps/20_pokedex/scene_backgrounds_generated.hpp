#pragma once

// 由 tools/build_scene_backgrounds.py 自动生成，请勿手工编辑。
#include <stdint.h>

namespace pokedex {
namespace data {

constexpr uint16_t kSceneWidth = 228;
constexpr uint16_t kSceneHeight = 135;
constexpr uint16_t kSceneScale = 2;
constexpr uint16_t kSceneThumbTop = 18;
constexpr uint16_t kSceneThumbHeight = 90;
constexpr uint16_t kSceneTransparentKey = 0xF81F;
struct SceneBackground {
  const uint16_t* pixels;
};
struct SceneLayerSet {
  SceneBackground far;
  SceneBackground middle;
  SceneBackground near;
  uint8_t groundTop;
};
extern const SceneBackground kRouteScenes[3];
extern const SceneBackground kRouteScenesDim[3];
extern const SceneLayerSet kRouteSceneLayers[3];

}  // namespace data
}  // namespace pokedex
