#!/bin/bash
# Performance test: Compare 1 thread vs 8 threads with batch processing

echo "========================================"
echo "Batch Processing Performance Test"
echo "Decoder: BP_HORIZONTAL_LAYERED_SIMD"
echo "========================================"
echo ""

# Test parameters
CODE_PATH="conf/dec/LDPC/MACKAY_504_1008.alist"
K=504
N=1008
SNR=2.0
FRAMES=1000
ITERATIONS=10

echo "Test Configuration:"
echo "  Code: MACKAY_504_1008 (K=$K, N=$N)"
echo "  SNR: $SNR dB"
echo "  Frames: $FRAMES"
echo "  Iterations: $ITERATIONS"
echo ""

echo "========================================"
echo "Test 1: Single Thread (t=1)"
echo "========================================"
time ./build/bin/aff3ct \
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
  2>&1 | tee /tmp/aff3ct_1thread.txt | tail -10

echo ""
echo "========================================"
echo "Test 2: 8 Threads (t=8)"
echo "========================================"
time ./build/bin/aff3ct \
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
  2>&1 | tee /tmp/aff3ct_8threads.txt | tail -10

echo ""
echo "========================================"
echo "Performance Comparison"
echo "========================================"

# Extract metrics
echo ""
echo "1 Thread Results:"
echo "------------------"
grep -E "(BER|FER|SIM_THR)" /tmp/aff3ct_1thread.txt | tail -1

echo ""
echo "8 Threads Results:"
echo "------------------"
grep -E "(BER|FER|SIM_THR)" /tmp/aff3ct_8threads.txt | tail -1

echo ""
echo "========================================"
echo "Speedup Calculation"
echo "========================================"

# Extract throughput (SIM_THR)
THR_1=$(grep -E "SIM_THR" /tmp/aff3ct_1thread.txt | tail -1 | awk '{print $NF}')
THR_8=$(grep -E "SIM_THR" /tmp/aff3ct_8threads.txt | tail -1 | awk '{print $NF}')

if [ -n "$THR_1" ] && [ -n "$THR_8" ]; then
    echo "Throughput (1 thread):  $THR_1 Mb/s"
    echo "Throughput (8 threads): $THR_8 Mb/s"
    
    # Calculate speedup (simple comparison, may need more sophisticated parsing)
    echo ""
    echo "Note: Check the actual throughput values above for speedup calculation"
fi

echo ""
echo "========================================"
echo "Test Complete"
echo "========================================"
echo ""
echo "Full results saved to:"
echo "  /tmp/aff3ct_1thread.txt"
echo "  /tmp/aff3ct_8threads.txt"

