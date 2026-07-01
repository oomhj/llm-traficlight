#!/bin/bash
# traflight-daemon.sh — 常驻串口守护进程（FIFO 命令队列）
# 在后台保持串口连接，hooks 通过队列通信，顺序执行，无竞争
#
# 启动:   bash traflight-daemon.sh start
# 停止:   bash traflight-daemon.sh stop
# 发送:   bash traflight-daemon.sh before
#         bash traflight-daemon.sh success
#         bash traflight-daemon.sh failure

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI="$DIR/traflight.py"
PORT="/dev/cu.usbserial-210"
PIDFILE="/tmp/traflight-daemon.pid"
QUEUE_DIR="/tmp/traflight-queue"

daemon_loop() {
    mkdir -p "$QUEUE_DIR"
    # 初次连接测试
    python3 "$CLI" --port "$PORT" status >/dev/null 2>&1

    while true; do
        # 按文件名顺序取出命令（ls sort 保证 FIFO 顺序）
        for entry in $(ls "$QUEUE_DIR"/cmd_* 2>/dev/null | sort); do
            CMD=$(cat "$entry" 2>/dev/null)
            rm -f "$entry"
            if [ -n "$CMD" ]; then
                # $CMD 包含 like "yellow" 或 "green" 或 "blink red -n 3"
                # 注意 word splitting 是故意的
                # shellcheck disable=SC2086
                python3 "$CLI" --port "$PORT" $CMD >/dev/null 2>&1
            fi
        done
        sleep 0.05
    done
}

start() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE")
        if kill -0 "$PID" 2>/dev/null; then
            return 0
        fi
        rm -f "$PIDFILE"
    fi
    (
        # 捕获退出信号，清理资源
        trap 'rm -rf "$QUEUE_DIR" "$PIDFILE"; exit' INT TERM
        daemon_loop
    ) &
    echo $! > "$PIDFILE"
}

stop() {
    if [ -f "$PIDFILE" ]; then
        kill $(cat "$PIDFILE") 2>/dev/null
        wait $(cat "$PIDFILE") 2>/dev/null
        rm -f "$PIDFILE"
    fi
    rm -rf "$QUEUE_DIR"
}

# 入队一个命令（FIFO 队列，带序号）
send_cmd() {
    mkdir -p "$QUEUE_DIR"
    local seq
    # 高精度时间戳 + PID 确保唯一且有序
    seq=$(python3 -c "import time; print(int(time.time() * 1000000))")
    echo "$*" > "$QUEUE_DIR/cmd_${seq}_$$"
}

case "$1" in
    start)   start ;;
    stop)    stop ;;
    restart) stop; sleep 0.3; start ;;
    status)
        if [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE") 2>/dev/null; then
            echo "running (pid $(cat $PIDFILE))"
        else
            echo "stopped"
        fi
        ;;
    before)    send_cmd "yellow" ;;
    success)   send_cmd "green" ;;
    failure)   send_cmd "blink red -n 3" ;;
    notify|red) send_cmd "red" ;;
    health)    python3 "$DIR/traflight-health.py" --light ;;
    healthd)
        # 后台持续监控, 每 5 秒检查一次系统健康
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
    *)
        echo "Usage: $0 {start|stop|restart|status|before|success|failure|notify|health|healthd|health-stop}"
        ;;
esac
