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
struct SceneBackground {
  const uint16_t* pixels;
};
extern const SceneBackground kRouteScenes[3];
extern const SceneBackground kRouteScenesDim[3];

}  // namespace data
}  // namespace pokedex
