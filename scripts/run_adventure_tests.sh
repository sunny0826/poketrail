#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/adventure-$(date -u +%Y%m%dT%H%M%SZ)}/domain"
mkdir -p "${build_dir}"
cd "${repo_root}"
python3 tools/build_adventure_catalog.py --check
sources=(firmware/apps/20_pokedex/adventure_progression.cpp
  firmware/apps/20_pokedex/adventure_migration.cpp
  firmware/apps/20_pokedex/adventure.cpp firmware/apps/20_pokedex/adventure_steps.cpp
  firmware/apps/20_pokedex/adventure_clock.cpp firmware/apps/20_pokedex/adventure_capture.cpp
  firmware/apps/20_pokedex/adventure_model.cpp firmware/apps/20_pokedex/battle.cpp
  firmware/apps/20_pokedex/calendar.cpp firmware/apps/20_pokedex/encounter_catalog.cpp
  firmware/apps/20_pokedex/save_codec.cpp firmware/apps/20_pokedex/file_store.cpp)
flags=(-Ifirmware/apps/20_pokedex -std=c++11 -O1 -g -Wall -Wextra -Werror)
if [[ "${SANITIZE:-1}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for suite in adventure save_codec save_codec_modern adventure_regressions progression migration unique_encounters; do
  "${CXX:-clang++}" "${flags[@]}" -Ifirmware/apps/20_pokedex "test/test_${suite}.cpp" "${sources[@]}" -o "${build_dir}/test_${suite}"
  "${build_dir}/test_${suite}"
done
"${CXX:-clang++}" "${flags[@]}" -Ifirmware/apps/20_pokedex -Ifirmware/shared test/test_test_mode.cpp \
  firmware/apps/20_pokedex/test_mode.cpp firmware/apps/20_pokedex/test_mode_platform.cpp \
  "${sources[@]}" -o "${build_dir}/test_test_mode"
"${build_dir}/test_test_mode"
# Sanitizers alter stack overhead; measure startup separately with guard pages.
"${CXX:-clang++}" -std=c++11 -Os -Wall -Wextra -Werror -pthread \
  -Ifirmware/apps/20_pokedex test/test_adventure_startup.cpp "${sources[@]}" \
  -o "${build_dir}/test_adventure_startup"
"${build_dir}/test_adventure_startup"
