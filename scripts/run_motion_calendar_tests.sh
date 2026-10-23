#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/motion-$(date -u +%Y%m%dT%H%M%SZ)}/hardware"
unified_src="$(python3 "${repo_root}/scripts/host_dependencies.py" M5Unified)"
mkdir -p "${build_dir}"
"${CXX:-c++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  "${repo_root}/test/test_motion_calendar.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/motion.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_motion_calendar"
"${build_dir}/test_motion_calendar" 2>&1 | tee "${build_dir}/test.log"
"${CXX:-c++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wno-unused-parameter -Wno-inconsistent-missing-override -Di2c_port_t=int \
  -I"${repo_root}/test/support/rtc_rx8130" \
  -I"${unified_src}" -I"${repo_root}/firmware/apps/20_pokedex" \
  "${repo_root}/test/test_device_clock.cpp" \
  "${unified_src}/utility/rtc/RX8130_Class.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/device_clock.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_device_clock"
"${build_dir}/test_device_clock" 2>&1 | tee "${build_dir}/rtc-test.log"
