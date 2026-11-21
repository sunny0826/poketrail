#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/game-$(date -u +%Y%m%dT%H%M%SZ)}/game"

mkdir -p "${build_dir}"

"${CXX:-c++}" \
  -std=c++17 \
  -O1 \
  -Wall \
  -Wextra \
  -o "${build_dir}/test_game" \
  "${repo_root}/test/test_game.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/game.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/sprite.cpp" \
  -lz

"${build_dir}/test_game"

"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra \
  "${repo_root}/test/test_sprite_cache.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/sprite.cpp" \
  -lz -o "${build_dir}/test_sprite_cache"
"${build_dir}/test_sprite_cache"
