#!/bin/bash

GUEST_IMG="guest/xv6/kernel/xv6"
QEMU="qemu-system-aarch64"
COMMON_ARGS=(
  -cpu cortex-a72
  -machine virt,gic-version=3
  -smp 4
  -m 512
  -nographic
)

print_help() {
  cat << EOF
Usage: ${0##*/} [COMMAND]

COMMAND:
  (no command)   Build both xv6 guest OS and hypervisor
  gdb            Run with GDB debugging hypervisor (starts GDB server on tcp::7758)
  guest          Run guest image directly
  help           Show this help message

If no command is provided, runs default configuration with hypervisor

EOF
}

case "$1" in
  gdb)
    exec "$QEMU" \
      "${COMMON_ARGS[@]}" \
      -machine virtualization=on \
      -kernel hyper \
      -S -gdb tcp::7758
    ;;
  guest)
    exec "$QEMU" \
      "${COMMON_ARGS[@]}" \
      -kernel "$GUEST_IMG" \
      -drive file=guest/xv6/fs.img,if=none,format=raw,id=x0 \
      -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
    ;;
  help|--help)
    print_help
    exit 0
    ;;
  "")
    exec "$QEMU" \
      "${COMMON_ARGS[@]}" \
      -machine virtualization=on \
      -kernel hyper
    ;;
  *)
    echo "Error: Unknown command '$1'" >&2
    print_help >&2
    exit 1
    ;;
esac