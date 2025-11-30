#!/bin/bash
# Simple visual test - shows input noisy codeword and output decoded codeword

echo "========================================"
echo "LDPC Decoder Visualization"
echo "Code: DEBUG_6_3 (6 bits, 3 check nodes)"
echo "========================================"
echo ""

# Run with debug and extract the first frame
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite 5 \
  -m 2.0 \
  -M 2.0 \
  -s 0.1 \
  -e 1 \
  --sim-dbg 2>&1 | tee /tmp/aff3ct_debug.txt > /dev/null

echo "First frame details:"
echo "----------------------------------------"
# Extract the first decode_siho call with all its output
grep -A 3 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | head -4

echo ""
echo "Channel output (before decoding):"
grep -A 2 "Channel_AWGN_LLR::add_noise" /tmp/aff3ct_debug.txt | head -3 | tail -1

echo ""
echo "========================================"
echo "Example reading:"
echo "  {IN}  Y_N = [ 3.75,  3.93,  2.35,  5.81,  5.03,  3.15]"
echo "        ↑ Input: 6 LLR values (Log-Likelihood Ratios)"
echo "        Positive = likely bit 0, Negative = likely bit 1"
echo ""
echo "  Hard decision: [ 0,  0,  0,  0,  0,  0]  (all positive → all 0s)"
echo ""
echo "  {OUT} V_K = [ 0,  0,  0]"
echo "        ↑ Output: 3 decoded information bits"
echo ""
echo "  {OUT} CWD = [ 0 or 1 ]"
echo "        0 = valid codeword (syndrome check passed)"
echo "        1 = invalid codeword (may have errors)"
echo "========================================"

