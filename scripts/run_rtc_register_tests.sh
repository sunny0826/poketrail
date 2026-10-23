#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/rtc-$(date -u +%Y%m%dT%H%M%SZ)}/rtc"
unified_src="$(python3 "${repo_root}/scripts/host_dependencies.py" M5Unified)"
mkdir -p "${build_dir}"
"${CXX:-clang++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wno-unused-parameter -Wno-inconsistent-missing-override -Di2c_port_t=int \
  -I"${repo_root}/test/support/rtc_rx8130" \
  -I"${unified_src}" \
  -I"${repo_root}/firmware/apps/20_pokedex" \
  "${repo_root}/test/test_device_clock_rx8130.cpp" \
  "${unified_src}/utility/rtc/RX8130_Class.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/device_clock.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_device_clock_rx8130"
UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_device_clock_rx8130"

"${CXX:-clang++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wno-unused-parameter -Wno-inconsistent-missing-override -Di2c_port_t=int \
  -I"${repo_root}/test/support/rtc_rx8130" \
  -I"${unified_src}" \
  -I"${repo_root}/firmware/apps/20_pokedex" \
  "${repo_root}/test/test_device_clock_lifecycle.cpp" \
  "${unified_src}/utility/rtc/RX8130_Class.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/device_clock.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_device_clock_lifecycle"
UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_device_clock_lifecycle"

python3 "${repo_root}/test/support/rtc_startup/extract_startup.py" \
  "${unified_src}/M5Unified.cpp" \
  "${build_dir}/m5_startup.inc"
"${CXX:-clang++}" -std=c++11 -O1 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Wno-unused-parameter -Wno-inconsistent-missing-override -Di2c_port_t=int \
  -I"${repo_root}/test/support/rtc_startup" -I"${repo_root}/test/support" \
  -I"${unified_src}" \
  -I"${repo_root}/firmware/apps/20_pokedex" -I"${repo_root}/firmware/shared" \
  -I"${build_dir}" \
  "${repo_root}/test/test_board_rtc_startup.cpp" \
  "${repo_root}/firmware/shared/board.cpp" \
  "${unified_src}/utility/rtc/RX8130_Class.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/device_clock.cpp" \
  "${repo_root}/firmware/apps/20_pokedex/calendar.cpp" \
  -o "${build_dir}/test_board_rtc_startup"
UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_board_rtc_startup" weekday-zero
UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_board_rtc_startup" irq-race
