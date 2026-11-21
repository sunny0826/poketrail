#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
port=""
skip_sync=0
for arg in "$@"; do
  case "${arg}" in
    --no-sync) skip_sync=1 ;;
    -*)
      echo "未知参数：${arg}" >&2
      exit 2
      ;;
    *)
      if [[ -n "${port}" ]]; then
        echo "只能指定一个串口" >&2
        exit 2
      fi
      port="${arg}"
      ;;
  esac
done

if [[ -z "${port}" ]]; then
  echo "探测串口 ..."
  port="$(python3 "${repo_root}/scripts/detect_port.py" || true)"
  if [[ -z "${port}" ]]; then
    echo "未找到串口。请确认：数据线已连接，长按电源键 2 秒进下载模式。" >&2
    exit 2
  fi
fi

cd "${repo_root}"

run_tool() {
  if command -v "$1" >/dev/null 2>&1; then
    "$@"
  else
    mise exec -- "$@"
  fi
}

echo "编译固件 ..."
run_tool pio run -e 20_pokedex

build_dir="${repo_root}/.pio/build/20_pokedex"
for image in bootloader.bin partitions.bin firmware.bin; do
  if [[ ! -f "${build_dir}/${image}" ]]; then
    echo "缺少构建产物：${build_dir}/${image}" >&2
    exit 1
  fi
done

echo "烧录到 ${port}"

read -r flash_mode flash_freq flash_size < <(
  "${repo_root}/scripts/esptool.sh" --chip esp32s3 image-info "${build_dir}/firmware.bin" \
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

"${repo_root}/scripts/esptool.sh" \
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
if [[ "${skip_sync}" == 0 ]]; then
  echo "等待设备启动并同步时钟 ..."
  # Do not silently switch to another visible serial device: the clock must be
  # set on the board that was just flashed. Allow the same CDC path to return.
  for _ in $(seq 1 50); do
    [[ -e "${port}" ]] && break
    sleep 0.2
  done
  sync_cmd=(mise exec -- python3)
  if python3 -c "import serial" >/dev/null 2>&1; then
    sync_cmd=(python3)
  fi
  if ! "${sync_cmd[@]}" "${repo_root}/scripts/sync_clock.py" --port "${port}" --timeout 30; then
    echo >&2
    echo "固件已写入 ${port}，但校时失败。" >&2
    echo "设备可能仍在使用旧日期。确认 USB 串口后重试：" >&2
    echo "  mise exec -- python3 scripts/sync_clock.py --port ${port}" >&2
    exit 3
  fi
  echo
  echo "完成。串口输出用：mise exec -- pio device monitor -p ${port} --baud 115200"
else
  echo "已跳过校时。串口输出用：mise exec -- pio device monitor -p ${port} --baud 115200"
fi
