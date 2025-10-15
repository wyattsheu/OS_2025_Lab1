#!/bin/bash
# run_demo.sh - 測試 IPC 性能比較

echo "=== 清理舊資源 ==="
pkill -f receiver; pkill -f sender
rm -f /dev/shm/sem.oslab1_sem_* /dev/shm/oslab1_shm /dev/mqueue/oslab1_mq

# 建立臨時檔案儲存輸出
TEMP_OUTPUT=$(mktemp)

echo ""
echo "=== Test 1: Message Passing ==="
echo "Starting receiver (Message Passing)..."
./receiver 1 2>&1 | tee "${TEMP_OUTPUT}.receiver1" &
RECEIVER_PID=$!
sleep 1

echo "Starting sender (Message Passing)..."
./sender 1 input.txt 2>&1 | tee "${TEMP_OUTPUT}.sender1"

# 等待 receiver 完成
wait $RECEIVER_PID

echo ""
echo "=== 清理資源 ==="
pkill -f receiver; pkill -f sender
rm -f /dev/shm/sem.oslab1_sem_* /dev/shm/oslab1_shm /dev/mqueue/oslab1_mq
sleep 1

echo ""
echo "=== Test 2: Shared Memory ==="
echo "Starting receiver (Shared Memory)..."
./receiver 2 2>&1 | tee "${TEMP_OUTPUT}.receiver2" &
RECEIVER_PID=$!
sleep 1

echo "Starting sender (Shared Memory)..."
./sender 2 input.txt 2>&1 | tee "${TEMP_OUTPUT}.sender2"

# 等待 receiver 完成
wait $RECEIVER_PID

echo ""
echo "=== 測試完成 ==="
pkill -f receiver; pkill -f sender
rm -f /dev/shm/sem.oslab1_sem_* /dev/shm/oslab1_shm /dev/mqueue/oslab1_mq

# 抓取執行時間
MP_SEND_TIME=$(grep -oP "Total time taken in sending msg: \K[0-9.]+" "${TEMP_OUTPUT}.sender1" 2>/dev/null || echo "N/A")
MP_RECV_TIME=$(grep -oP "Total time taken in receiving msg: \K[0-9.]+" "${TEMP_OUTPUT}.receiver1" 2>/dev/null || echo "N/A")
SHM_SEND_TIME=$(grep -oP "Total time taken in sending msg: \K[0-9.]+" "${TEMP_OUTPUT}.sender2" 2>/dev/null || echo "N/A")
SHM_RECV_TIME=$(grep -oP "Total time taken in receiving msg: \K[0-9.]+" "${TEMP_OUTPUT}.receiver2" 2>/dev/null || echo "N/A")

echo ""
echo "============================================"
echo "           🎯 性能比較結果"
echo "============================================"
echo ""
printf "%-20s | %-15s | %-15s\n" "Method" "Send Time (s)" "Recv Time (s)"
echo "------------------------------------------------------------"
printf "%-20s | %-15s | %-15s\n" "Message Passing" "$MP_SEND_TIME" "$MP_RECV_TIME"
printf "%-20s | %-15s | %-15s\n" "Shared Memory" "$SHM_SEND_TIME" "$SHM_RECV_TIME"
echo "============================================"

# 計算差異百分比（只有在兩個時間都是數字時才計算）
if command -v bc &> /dev/null && [[ "$MP_SEND_TIME" =~ ^[0-9.]+$ ]] && [[ "$SHM_SEND_TIME" =~ ^[0-9.]+$ ]]; then
    echo ""
    echo "📊 差異分析："
    
    # 比較送訊時間
    SEND_DIFF=$(echo "scale=2; (($MP_SEND_TIME - $SHM_SEND_TIME) / $MP_SEND_TIME) * 100" | bc)
    ABS_SEND_DIFF=$(echo "scale=2; if ($SEND_DIFF < 0) -1 * $SEND_DIFF else $SEND_DIFF" | bc)
    
    if (( $(echo "$MP_SEND_TIME < $SHM_SEND_TIME" | bc -l) )); then
        echo "  • 送訊時間：Message Passing (${MP_SEND_TIME}s) 比 Shared Memory (${SHM_SEND_TIME}s) 快了 ${ABS_SEND_DIFF}%"
    else
        echo "  • 送訊時間：Shared Memory (${SHM_SEND_TIME}s) 比 Message Passing (${MP_SEND_TIME}s) 快了 ${ABS_SEND_DIFF}%"
    fi
    
    # 比較收訊時間
    if [[ "$MP_RECV_TIME" =~ ^[0-9.]+$ ]] && [[ "$SHM_RECV_TIME" =~ ^[0-9.]+$ ]]; then
        RECV_DIFF=$(echo "scale=2; (($MP_RECV_TIME - $SHM_RECV_TIME) / $MP_RECV_TIME) * 100" | bc)
        ABS_RECV_DIFF=$(echo "scale=2; if ($RECV_DIFF < 0) -1 * $RECV_DIFF else $RECV_DIFF" | bc)
        
        if (( $(echo "$MP_RECV_TIME < $SHM_RECV_TIME" | bc -l) )); then
            echo "  • 收訊時間：Message Passing (${MP_RECV_TIME}s) 比 Shared Memory (${SHM_RECV_TIME}s) 快了 ${ABS_RECV_DIFF}%"
        else
            echo "  • 收訊時間：Shared Memory (${SHM_RECV_TIME}s) 比 Message Passing (${MP_RECV_TIME}s) 快了 ${ABS_RECV_DIFF}%"
        fi
    fi
fi

# 清理臨時檔案
rm -f "${TEMP_OUTPUT}"*

echo ""
echo "✅ 測試與比較完成！"