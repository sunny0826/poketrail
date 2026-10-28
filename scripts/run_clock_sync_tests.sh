#!/usr/bin/env bash
# 定向验证烧录后主机校时：协议边界、Runtime 事务与主机脚本。
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MYM5_VERIFICATION_DIR:-${repo_root}/.pio/verification/clock-sync-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "${build_dir}"
cd "${repo_root}"

flags=(-Ifirmware/apps/20_pokedex -std=c++11 -O1 -g -Wall -Wextra -Werror)
if [[ "${SANITIZE:-1}" == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CXX:-clang++}" "${flags[@]}" test/test_clock_sync.cpp \
  firmware/apps/20_pokedex/clock_sync.cpp firmware/apps/20_pokedex/calendar.cpp \
  -o "${build_dir}/test_clock_sync"
"${build_dir}/test_clock_sync"

python3 -m unittest -v test/test_sync_clock.py
python3 test/test_clock_serial.py

MYM5_VERIFICATION_DIR="${build_dir}/render" \
  python3 scripts/run_render_tests.py test_clock_sync_render
