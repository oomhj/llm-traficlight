#!/bin/bash
# traflight-hook.sh — Claude Code hook 脚本
# 通过 Python 守护进程队列控制红绿灯
#
# 用法:
#   PreToolUse[Bash]:           bash traflight-hook.sh before
#   PostToolUse[Bash]:          bash traflight-hook.sh success
#   PostToolUseFailure[Bash]:   bash traflight-hook.sh failure

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDAEMON="$DIR/traflight-daemon.py"

# 确保守护进程在运行
python3 "$PYDAEMON" start 2>/dev/null

case "$1" in
    before)  python3 "$PYDAEMON" send "yellow" >/dev/null 2>&1 ;;
    success) python3 "$PYDAEMON" send "green" >/dev/null 2>&1 ;;
    failure) python3 "$PYDAEMON" send "red" >/dev/null 2>&1 ;;
    notify)  python3 "$PYDAEMON" send "blink_all" >/dev/null 2>&1 ;;
esac
