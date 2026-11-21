#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/storage-$(date -u +%Y%m%dT%H%M%SZ)}/storage"
mkdir -p "${build_dir}"
cd "${repo_root}"
flags=(-Ifirmware/apps/20_pokedex -std=c++11 -O1 -g -Wall -Wextra -Werror)
if [[ "${SANITIZE:-1}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=(firmware/apps/20_pokedex/file_store.cpp firmware/apps/20_pokedex/save_codec.cpp
  firmware/apps/20_pokedex/adventure_migration.cpp
  firmware/apps/20_pokedex/adventure_model.cpp firmware/apps/20_pokedex/calendar.cpp firmware/apps/20_pokedex/encounter_catalog.cpp)
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run --shared "${sources[@]}" \
  --target test_file_store:test/test_file_store.cpp \
  --target test_storage_allocation:test/support/storage/allocation_tests.cpp \
  -- "${flags[@]}"
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run --shared "${sources[@]}" firmware/apps/20_pokedex/device_store.cpp \
  --target test_device_store:test/support/storage/device_tests.cpp \
  --target test_mode_storage:test/support/storage/test_mode_storage.cpp \
  -- "${flags[@]}" -Itest/support/storage
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run --shared "${sources[@]}" \
  --target test_storage_psram:test/support/storage/allocation_tests.cpp \
  -- "${flags[@]}" -DARDUINO_ARCH_ESP32 -Itest/support/psram
python3 scripts/host_build.py unit --cxx "${CXX:-clang++}" --run --shared "${sources[@]}" firmware/apps/20_pokedex/device_store.cpp firmware/apps/20_pokedex/test_console.cpp \
  --target test_large_state:test/support/storage/large_state_tests.cpp \
  -- "${flags[@]}" -DARDUINO_ARCH_ESP32 -Itest/support/psram -Itest/support/storage
