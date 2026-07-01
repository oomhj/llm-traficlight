#!/bin/bash
# traflight-daemon.sh — 串口守护进程 CLI (委托给 Python 实现)
# 保持串口连接，所有命令走 FIFO 队列，统一管理串口

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYDAEMON="$DIR/traflight-daemon.py"

case "$1" in
    start|stop|status)
        python3 "$PYDAEMON" "$1"
        ;;
    restart)
        python3 "$PYDAEMON" stop 2>/dev/null
        sleep 0.5
        python3 "$PYDAEMON" start
        ;;
    before)    python3 "$PYDAEMON" send "yellow" ;;
    success)   python3 "$PYDAEMON" send "green" ;;
    failure)   python3 "$PYDAEMON" send "blink_red" ;;
    notify|red) python3 "$PYDAEMON" send "notify" ;;
    health)
        # 采集系统数据并通过 daemon 队列发送
        python3 "$DIR/traflight-health.py"
        ;;
    healthd)
        nohup python3 "$DIR/traflight-health.py" --watch > /dev/null 2>&1 &
        echo $! > /tmp/traflight-healthd.pid
        echo "healthd started (pid $(cat /tmp/traflight-healthd.pid))"
        ;;
    health-stop)
        if [ -f /tmp/traflight-healthd.pid ]; then
            kill $(cat /tmp/traflight-healthd.pid) 2>/dev/null
            rm -f /tmp/traflight-healthd.pid
            echo "healthd stopped"
        fi
        ;;
    send_raw)
        shift
        python3 "$PYDAEMON" send "$*"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|before|success|failure|notify|health|healthd|health-stop}"
        ;;
esac
