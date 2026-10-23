#!/usr/bin/env bash
# 在电脑上编译并运行动画时序、展示交互与解码器的逻辑测试。
#
# 这些测试不需要硬件。目的是在烧录到真机之前把状态机的问题暴露出来。
#
# 用法：
#   mise exec -- ./scripts/run_game_tests.sh

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/.pio/game-test"

mkdir -p "${build_dir}"

"${CXX:-c++}" \
  -std=c++17 \
  -O1 \
  -Wall \
  -Wextra \
  -o "${build_dir}/test_game" \
  "${repo_root}/test/test_game.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/game.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/sprite.cpp"

"${build_dir}/test_game"
