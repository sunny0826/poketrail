#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/storage-$(date -u +%Y%m%dT%H%M%SZ)}/storage"
mkdir -p "${build_dir}"
cd "${repo_root}"
flags=(-Ifirmware/apps/20_pokedex -std=c++11 -O1 -g -Wall -Wextra -Werror)
if [[ "${SANITIZE:-1}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=(firmware/apps/20_pokedex/file_store.cpp firmware/apps/20_pokedex/save_codec.cpp
  firmware/apps/20_pokedex/adventure_model.cpp firmware/apps/20_pokedex/adventure_model_activity.cpp
  firmware/apps/20_pokedex/adventure_model_save.cpp
  firmware/apps/20_pokedex/dragon_trial_catalog_generated.cpp
  firmware/apps/20_pokedex/calendar.cpp firmware/apps/20_pokedex/encounter_catalog.cpp)
"${CXX:-clang++}" "${flags[@]}" test/test_file_store.cpp "${sources[@]}" -o "${build_dir}/test_file_store"
"${build_dir}/test_file_store"
"${CXX:-clang++}" "${flags[@]}" -Itest/support/storage test/support/storage/device_tests.cpp \
  firmware/apps/20_pokedex/device_store.cpp "${sources[@]}" -o "${build_dir}/test_device_store"
"${build_dir}/test_device_store"
"${CXX:-clang++}" "${flags[@]}" test/support/storage/allocation_tests.cpp "${sources[@]}" -o "${build_dir}/test_storage_allocation"
"${build_dir}/test_storage_allocation"
