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
targets=()
for suite in adventure_queue battle adventure save_codec save_codec_modern save_schema6 save_language language adventure_regressions progression migration unique_encounters; do
  targets+=(--target "test_${suite}:test/test_${suite}.cpp")
done
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run --shared "${sources[@]}" "${targets[@]}" -- "${flags[@]}"
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run \
  --shared "${sources[@]}" firmware/apps/20_pokedex/test_mode.cpp firmware/apps/20_pokedex/test_mode_platform.cpp \
  --target "test_test_mode:test/test_test_mode.cpp" \
  -- "${flags[@]}" -Ifirmware/shared
