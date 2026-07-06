#!/bin/bash
# traflight-cli.sh — 红绿灯 CLI 封装
# 调用 Python 守护进程队列控制 ESP8266 红绿灯
#
# 用法:
#   bash traflight-cli.sh yellow     🟡
#   bash traflight-cli.sh green      🟢
#   bash traflight-cli.sh red        🔴
#   bash traflight-cli.sh status     查询
#
# skill 调用示例:
#   traflight yellow
#   traflight green
#   traflight red

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDAEMON="$DIR/traflight-daemon.py"
PIDFILE="/tmp/traflight-daemon.pid"

# 确保 daemon 在运行
if [ ! -f "$PIDFILE" ] || ! kill -0 "$(cat "$PIDFILE" 2>/dev/null)" 2>/dev/null; then
    python3 "$PYDAEMON" start 2>/dev/null
fi

case "$1" in
    yellow|green|red|off|status|blink|health|blink_all)
        python3 "$PYDAEMON" send "$@" >/dev/null 2>&1
        ;;
    scan)
        python3 "$DIR/traflight.py" scan
        ;;
    *)
        echo "用法: traflight-cli.sh yellow|green|red|status|blink|scan"
        exit 1
        ;;
esac
