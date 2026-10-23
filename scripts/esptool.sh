#!/usr/bin/env bash
# 运行 esptool（读/写 flash、查芯片信息）。
#
# 使用 requirements-dev.txt 固定的 esptool 版本，运行于 mise Python 环境。
#
# 用法：
#   ./scripts/esptool.sh --chip esp32s3 --port /dev/cu.usbmodem101 chip_id
#
# 参数原样透传给 esptool。
#
# 本脚本不依赖调用方的 cwd：它自己找到项目根目录并切进去，这样无论从
# 哪里调用，`mise exec` 都能读到项目的 mise.toml 并注入工具。

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

cd "${repo_root}"

# esptool 来自 mise 管理的 Python 环境（见 requirements-dev.txt）。
if ! mise exec -- python3 -c "import esptool" >/dev/null 2>&1; then
  echo "esptool 未安装，先执行：mise exec -- python3 -m pip install -r requirements-dev.txt" >&2
  exit 2
fi

exec mise exec -- python3 -m esptool "$@"
