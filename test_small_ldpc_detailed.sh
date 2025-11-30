#!/bin/bash
# Detailed test script to visualize LDPC decoding with a very small code
# Shows input noisy codeword and output corrected codeword

echo "========================================"
echo "LDPC Decoder Visualization Test (Detailed)"
echo "Code: DEBUG_6_3 (K=3, N=6)"
echo "========================================"
echo ""
echo "Running simulation with debug output..."
echo ""

# Run with debug mode - show full codeword decoding
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
  -e 5 \
  --sim-dbg \
  2>&1

echo ""
echo "========================================"
echo "To see the full codeword (V_N), look for:"
echo "  decode_siho_cw - shows full decoded codeword"
echo ""
echo "Example interpretation:"
echo "  Input Y_N[0] = +4.46  -> Hard decision: 0 (positive = bit 0)"
echo "  Input Y_N[1] = -0.65  -> Hard decision: 1 (negative = bit 1)"
echo "  Output V_N[0] = 0     -> Decoded bit 0"
echo "  Output V_N[1] = 0     -> Decoded bit 1 (corrected!)"
echo "========================================"

