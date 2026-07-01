#!/bin/bash
# traflight-hook.sh — Claude Code hook 脚本
# 通过 Python 守护进程控制红绿灯，避免串口并发冲突
#
# 用法:
#   PreToolUse[Bash]:           bash traflight-hook.sh before
#   PostToolUse[Bash]:          bash traflight-hook.sh success
#   PostToolUseFailure[Bash]:   bash traflight-hook.sh failure

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAEMON="$DIR/traflight-daemon.sh"

# 确保守护进程在运行
"$DAEMON" start 2>/dev/null

case "$1" in
    before)  "$DAEMON" before  >/dev/null 2>&1 ;;
    success) "$DAEMON" success >/dev/null 2>&1 ;;
    failure) "$DAEMON" failure >/dev/null 2>&1 ;;
esac
