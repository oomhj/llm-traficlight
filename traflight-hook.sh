#!/bin/bash
# traflight-hook.sh — Claude Code hook 脚本
# 自动在命令执行前后控制红绿灯
#
# 用法:
#   PreToolUse[Bash]:           bash traflight-hook.sh before
#   PostToolUse[Bash]:          bash traflight-hook.sh success
#   PostToolUseFailure[Bash]:   bash traflight-hook.sh failure

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI="$DIR/traflight.py"
PORT="/dev/cu.usbserial-210"

# 静默执行，不输出任何内容到终端
run_traflight() {
    python3 "$CLI" --port "$PORT" "$@" >/dev/null 2>&1
}

case "$1" in
    before)
        run_traflight yellow
        ;;
    success)
        run_traflight green
        ;;
    failure)
        run_traflight blink red -n 3
        ;;
esac
