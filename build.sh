#!/bin/bash

GUEST_XV6_DIR="guest/xv6"

show_help() {
    cat << EOF
Usage: ./build [COMMAND]

COMMAND:
  (no command)    Build both xv6 guest OS and hypervisor
  guest           Build xv6 guest OS only
  -h, --help      Show this help message
  debug           Build hypervisor in debug mode (with DEBUG_MODE=1)

Note: If build.sh failed at first time, run it again!

EOF
}

case "$1" in
    "guest")
        # 构建xv6
        echo ">>>>>> Building xv6 guest OS..."
        (cd "$GUEST_XV6_DIR" && make clean && make)
        echo ">>>>>> xv6 guest OS build completed"
        ;;

    "-h"|"--help")
        show_help
        ;;

    "debug")
        # 以调试模式构建hypervisor
        echo ">>>>>> Building hypervisor in debug mode..."
        make clean
        make VERBOSE=1 DEBUG_MODE=1
        echo ">>>>>> Debug mode hypervisor build completed"
        ;;

    "")
        # 仅构建hypervisor
        echo ">>>>>> Building hypervisor..."
        make clean
        make VERBOSE=1
        echo ">>>>>> Hypervisor build completed"
        ;;

    *)
        # 处理未知命令
        echo "Error: Unknown command '$1'"
        show_help
        exit 1
        ;;
esac
