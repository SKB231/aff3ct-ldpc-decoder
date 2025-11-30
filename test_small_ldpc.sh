#!/bin/bash
# Simple test script to visualize LDPC decoding with a very small code
# This uses the DEBUG_6_3 code (6 bits, 3 check nodes) so you can see everything

echo "========================================"
echo "LDPC Decoder Visualization Test"
echo "Code: DEBUG_6_3 (K=3, N=6)"
echo "========================================"
echo ""
echo "Running simulation with debug output..."
echo ""

# Run with debug mode to see input/output
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite 5 \
  -m 1.0 \
  -M 1.0 \
  -s 0.1 \
  -e 3 \
  --sim-dbg \
  2>&1 | grep -A 5 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho"

echo ""
echo "========================================"
echo "Explanation:"
echo "  {IN}  Y_N = [values]  <- Noisy LLRs from channel (input)"
echo "  {OUT} V_K = [values]  <- Decoded information bits (output)"
echo "  {OUT} CWD = [0 or 1]  <- 0 = valid codeword, 1 = invalid"
echo ""
echo "Positive LLR = likely bit 0"
echo "Negative LLR = likely bit 1"
echo "========================================"

