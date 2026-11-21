#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
port="${1:-}"
backup_dir="${repo_root}/backups"
stamp="$(date +%Y%m%d-%H%M%S)"
output="${backup_dir}/flash-${stamp}-${$}.bin"

if [[ -z "${port}" ]]; then
  echo "探测串口 ..."
  port="$(python3 "${repo_root}/scripts/detect_port.py" || true)"
  if [[ -z "${port}" ]]; then
    echo "未找到串口。请确认：数据线已连接，长按电源键 2 秒进下载模式。" >&2
    exit 2
  fi
fi

mkdir -p "${backup_dir}"
echo "读取 ${port} 的整片 Flash -> ${output}"

"${repo_root}/scripts/esptool.sh" \
  --chip esp32s3 \
  --port "${port}" \
  --baud 460800 \
  read-flash 0x0 0x1000000 "${output}"

actual="$(wc -c < "${output}" | tr -d '[:space:]')"
if [[ "${actual}" != "16777216" ]]; then
  echo "备份大小异常：期望 16777216，实际 ${actual}" >&2
  exit 1
fi

shasum -a 256 "${output}" | tee "${output}.sha256"
echo "备份完成：${output}"
