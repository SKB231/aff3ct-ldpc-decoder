#!/bin/bash
# Test with visible errors - shows error correction in action

echo "========================================"
echo "LDPC Decoder - Error Correction Example"
echo "Code: DEBUG_6_3 (6 bits, 3 check nodes)"
echo "========================================"
echo ""

# Run multiple frames to find one with errors
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  --dec-type "BP_HORIZONTAL_LAYERED_SIMD" \
  --dec-ite 5 \
  -m 1.5 \
  -M 1.5 \
  -s 0.1 \
  -e 10 \
  --sim-dbg 2>&1 | tee /tmp/aff3ct_debug.txt > /dev/null

# Find a frame with negative LLRs (potential errors)
frame_num=0
for i in {1..10}; do
    Y_N_LINE=$(grep -A 1 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "Y_N = " | sed -n "${i}p")
    if [ -n "$Y_N_LINE" ]; then
        # Check if there are negative values (potential errors)
        if echo "$Y_N_LINE" | grep -q "-"; then
            frame_num=$i
            break
        fi
    fi
done

if [ $frame_num -eq 0 ]; then
    frame_num=1
fi

# Extract the frame
Y_N_LINE=$(grep -A 1 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "Y_N = " | sed -n "${frame_num}p")
V_K_LINE=$(grep -A 3 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "V_K = " | sed -n "${frame_num}p")
CWD_LINE=$(grep -A 2 "Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho" /tmp/aff3ct_debug.txt | grep "CWD = " | sed -n "${frame_num}p")

# Extract values
Y_N_VALS=$(echo "$Y_N_LINE" | sed 's/.*\[\(.*\)\].*/\1/')
V_K_VALS=$(echo "$V_K_LINE" | sed 's/.*\[\(.*\)\].*/\1/')
CWD_VAL=$(echo "$CWD_LINE" | sed 's/.*\[\(.*\)\].*/\1/' | tr -d ' ')

echo "Frame #$frame_num:"
echo "----------------------------------------"
echo "INPUT (Noisy LLRs from channel):"
echo "  Y_N = [$Y_N_VALS]"
echo ""

# Convert to hard decisions and highlight potential errors
echo "Hard decision BEFORE decoding:"
echo -n "  Bits = ["
error_positions=""
pos=0
for val in $Y_N_VALS; do
    val_clean=$(echo "$val" | xargs | sed 's/^ *//;s/ *$//')
    # Use awk for floating point comparison
    bit=$(echo "$val_clean" | awk '{if ($1 < 0) print "1"; else print "0"}')
    echo -n "$bit "
    if [ "$bit" = "1" ]; then
        error_positions="$error_positions $pos"
    fi
    pos=$((pos+1))
done
echo "]"

if [ -n "$error_positions" ]; then
    echo "  ⚠ Potential errors at positions:$error_positions (negative LLRs → bit 1)"
else
    echo "  ✓ All LLRs positive (likely all bits are 0)"
fi
echo ""

echo "OUTPUT (Decoded information bits):"
echo "  V_K = [$V_K_VALS]"
echo ""

echo "Codeword Status:"
if [ "$CWD_VAL" = "0" ]; then
    echo "  CWD = 0 ✓ Valid codeword (syndrome check passed!)"
    echo "  ✓ Decoder successfully corrected errors!"
else
    echo "  CWD = 1 ✗ Invalid codeword"
    echo "  (Decoder may need more iterations or noise is too high)"
fi
echo ""

echo "========================================"
echo "What happened:"
echo "  1. Channel added noise → some LLRs became negative"
echo "  2. Decoder processed LLRs through 5 iterations"
echo "  3. Decoder output: V_K = [$V_K_VALS]"
echo "  4. Syndrome check: CWD = $CWD_VAL"
echo "========================================"

