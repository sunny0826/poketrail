#!/usr/bin/env bash
# 烧录宝可梦像素应用到 M5Stack StopWatch。
#
# 用法：
#   rtk proxy mise exec -- ./scripts/flash.sh              # 自动探测端口
#   rtk proxy mise exec -- ./scripts/flash.sh /dev/cu.usbmodemXXXX
#
# 烧录前请确认：
#   1. 已用数据线连接 USB Type-C
#   2. 长按电源键约 2 秒进入下载模式（内部绿灯亮起）
#   3. 已备份出厂固件（首次烧录前必做，见 scripts/backup.sh）

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
port="${1:-}"

if [[ -z "${port}" ]]; then
  echo "探测串口 ..."
  port="$(python3 "${repo_root}/scripts/detect_port.py" || true)"
  if [[ -z "${port}" ]]; then
    echo "未找到串口。请确认：数据线已连接，长按电源键 2 秒进下载模式。" >&2
    exit 2
  fi
fi

cd "${repo_root}"

echo "编译固件 ..."
rtk proxy mise exec -- pio run -e 20_pokedex

build_dir="${repo_root}/.pio/build/20_pokedex"
for image in bootloader.bin partitions.bin firmware.bin; do
  if [[ ! -f "${build_dir}/${image}" ]]; then
    echo "缺少构建产物：${build_dir}/${image}" >&2
    exit 1
  fi
done

echo "烧录到 ${port}"

# 使用 requirements-dev.txt 固定的 esptool 5.6.0 进行设备写入。
# 三个镜像的偏移与 PlatformIO 板级定义一致：
#   bootloader 0x0 / partitions 0x8000 / app 0x10000
# flash_mode、flash_freq、flash_size 从固件头读出来，不写死。
read -r flash_mode flash_freq flash_size < <(
  rtk proxy mise exec -- python3 -m esptool --chip esp32s3 image-info "${build_dir}/firmware.bin" \
    | awk -F': +' '
      /Flash mode/ {mode=$2}
      /Flash freq/ {freq=$2}
      /Flash size/ {size=$2}
      END {
        gsub(/MB/, "", size)
        print tolower(mode), tolower(freq), size
      }'
)

if [[ -z "${flash_mode}" || -z "${flash_freq}" || -z "${flash_size}" ]]; then
  echo "无法从固件头解析 flash 参数" >&2
  exit 1
fi

echo "flash 参数：mode=${flash_mode} freq=${flash_freq} size=${flash_size}MB"

rtk proxy mise exec -- "${repo_root}/scripts/esptool.sh" \
  --chip esp32s3 \
  --port "${port}" \
  --baud 460800 \
  --before default-reset \
  --after hard-reset \
  write-flash \
  --flash-mode "${flash_mode}" \
  --flash-freq "${flash_freq}" \
  --flash-size "${flash_size}MB" \
  0x0 "${build_dir}/bootloader.bin" \
  0x8000 "${build_dir}/partitions.bin" \
  0x10000 "${build_dir}/firmware.bin"

echo
echo "完成。串口输出用：rtk proxy mise exec -- pio device monitor -p ${port} --baud 115200"
