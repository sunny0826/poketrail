#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

cd "${repo_root}"

if ! mise exec -- python3 -c "import esptool" >/dev/null 2>&1; then
  echo "esptool 未安装，先执行：mise exec -- python3 -m pip install -r requirements-dev.txt" >&2
  exit 2
fi

exec mise exec -- python3 -m esptool "$@"
