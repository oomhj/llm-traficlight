#!/bin/bash
# traflight-daemon.sh — 串口守护进程 CLI (委托给 Python 实现)
# 保持串口连接，所有命令走 FIFO 队列，统一管理串口
# 健康监控已集成到 daemon 内部 (自动每秒采集)

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDAEMON="$DIR/traflight-daemon.py"

case "$1" in
    start|stop|restart|status)
        python3 "$PYDAEMON" "$1"
        ;;
    before)  python3 "$PYDAEMON" send "yellow" ;;
    success) python3 "$PYDAEMON" send "green" ;;
    failure) python3 "$PYDAEMON" send "red" ;;
    notify)  python3 "$PYDAEMON" send "blink_red" ;;
    red)     python3 "$PYDAEMON" send "red" ;;
    send_raw)
        shift
        python3 "$PYDAEMON" send "$*"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|before|success|failure|notify|red}"
        ;;
esac
