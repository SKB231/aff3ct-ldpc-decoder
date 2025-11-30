#!/bin/bash
# Clear visual test - shows input noisy codeword and output decoded codeword with interpretation

echo "========================================"
echo "LDPC Decoder Visualization"
echo "Code: DEBUG_6_3 (6 bits, 3 check nodes)"
echo "========================================"
echo ""

# Run with debug
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite 75 \
  -m 2.0 \
  -M 2.0 \
  -s 0.1 \
  -e 1 \
  --sim-dbg 2>&1 | tee /tmp/aff3ct_debug.txt > /dev/null

# Extract first frame
Y_N_LINE=$(grep -A 1 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "Y_N = " | head -1)
V_K_LINE=$(grep -A 3 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "V_K = " | head -1)
CWD_LINE=$(grep -A 2 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "CWD = " | head -1)

# Extract values
Y_N_VALS=$(echo "$Y_N_LINE" | sed 's/.*\[\(.*\)\].*/\1/')
V_K_VALS=$(echo "$V_K_LINE" | sed 's/.*\[\(.*\)\].*/\1/')
CWD_VAL=$(echo "$CWD_LINE" | sed 's/.*\[\(.*\)\].*/\1/' | tr -d ' ')

echo "INPUT (Noisy LLRs from channel):"
echo "  Y_N = [$Y_N_VALS]"
echo ""

# Convert to hard decisions
echo "Hard decision BEFORE decoding:"
echo -n "  Bits = ["
for val in $Y_N_VALS; do
    # Remove leading/trailing spaces and compare
    val_clean=$(echo "$val" | xargs)
    if (( $(echo "$val_clean < 0" | bc -l 2>/dev/null || echo "0") )); then
        echo -n "1 "
    else
        echo -n "0 "
    fi
done
echo "]"
echo ""

echo "OUTPUT (Decoded information bits):"
echo "  V_K = [$V_K_VALS]"
echo ""

echo "Codeword Status:"
if [ "$CWD_VAL" = "0" ]; then
    echo "  CWD = 0 ✓ Valid codeword (syndrome check passed!)"
else
    echo "  CWD = 1 ✗ Invalid codeword (may have errors)"
fi
echo ""

echo "========================================"
echo "Interpretation:"
echo "  • Y_N values are Log-Likelihood Ratios (LLRs)"
echo "    - Positive → likely bit 0"
echo "    - Negative → likely bit 1"
echo "    - Larger absolute value → more confident"
echo ""
echo "  • V_K are the 3 decoded information bits"
echo ""
echo "  • CWD = 0 means the decoded codeword satisfies"
echo "    all parity-check equations (H × codeword^T = 0)"
echo "========================================"

