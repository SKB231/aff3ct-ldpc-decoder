#!/bin/bash
# Quick performance test with smaller code for faster results

echo "========================================"
echo "Quick Batch Processing Test"
echo "========================================"
echo ""

CODE_PATH="conf/dec/LDPC/GSM_2112_4224.alist"
K=2112
N=4224
SNR=2.0
FRAMES=100
ITERATIONS=5

echo "Configuration: GSM_2112_4224 (K=$K, N=$N)"
echo "Frames: $FRAMES, Iterations: $ITERATIONS, SNR: $SNR dB"
echo ""

echo "1 Thread:"
./build/bin/aff3ct \
  -C "LDPC" \
  -K $K \
  -N $N \
  --dec-h-path $CODE_PATH \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite $ITERATIONS \
  -m $SNR \
  -M $SNR \
  -s 0.1 \
  -e $FRAMES \
  -t 1 \
  2>&1 | grep -E "(BER|FER|SIM_THR|ET/RT)" | tail -3

echo ""
echo "8 Threads:"
./build/bin/aff3ct \
  -C "LDPC" \
  -K $K \
  -N $N \
  --dec-h-path $CODE_PATH \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite $ITERATIONS \
  -m $SNR \
  -M $SNR \
  -s 0.1 \
  -e $FRAMES \
  -t 8 \
  2>&1 | grep -E "(BER|FER|SIM_THR|ET/RT)" | tail -3

