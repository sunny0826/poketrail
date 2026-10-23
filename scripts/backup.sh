#!/usr/bin/env bash
# 备份 StopWatch 当前整片 16MB Flash；仅首次刷机前的备份是出厂镜像。
#
# 首次烧录自定义固件前保存备份，并保留校验和用于恢复。
# 此脚本读取设备当前内容；已刷入自定义固件后运行不会得到出厂镜像。
#
# 用法：
#   rtk proxy mise exec -- ./scripts/backup.sh
#   rtk proxy mise exec -- ./scripts/backup.sh /dev/cu.usbmodemXXXX

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

# esptool 版本由 requirements-dev.txt 固定，入口为 scripts/esptool.sh。
rtk proxy mise exec -- "${repo_root}/scripts/esptool.sh" \
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
