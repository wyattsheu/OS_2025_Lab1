#!/bin/bash
# run_demo.sh - 測試 IPC 性能比較

echo "=== 清理舊資源 ==="
pkill -f receiver; pkill -f sender
rm -f /dev/shm/sem.oslab1_sem_* /dev/shm/oslab1_shm /dev/mqueue/oslab1_mq

echo ""
echo "=== Test 1: Message Passing ==="
echo "Starting receiver (Message Passing)..."
./receiver 1 &
RECEIVER_PID=$!
sleep 1

echo "Starting sender (Message Passing)..."
./sender 1 input.txt

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
./receiver 2 &
RECEIVER_PID=$!
sleep 1

echo "Starting sender (Shared Memory)..."
./sender 2 input.txt

# 等待 receiver 完成
wait $RECEIVER_PID

echo ""
echo "=== 測試完成 ==="
pkill -f receiver; pkill -f sender
rm -f /dev/shm/sem.oslab1_sem_* /dev/shm/oslab1_shm /dev/mqueue/oslab1_mq