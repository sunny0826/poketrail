#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/motion-$(date -u +%Y%m%dT%H%M%SZ)}/hardware"
mkdir -p "${build_dir}"
"${CXX:-c++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${repo_root}/test" \
  "${repo_root}/test/test_motion_detector.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/motion.cpp" \
  -o "${build_dir}/test_motion_detector"
"${build_dir}/test_motion_detector" 2>&1 | tee "${build_dir}/motion.log"
"${CXX:-c++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"${repo_root}/test" \
  "${repo_root}/test/test_motion_calendar.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/motion.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_motion_calendar"
"${build_dir}/test_motion_calendar" 2>&1 | tee "${build_dir}/calendar.log"
