#!/bin/bash
# Example: Running default LDPC decoder (BP_FLOODING)

echo "========================================"
echo "Default LDPC Decoder (BP_FLOODING)"
echo "========================================"
echo ""

# Method 1: Don't specify --dec-type (uses default)
echo "Method 1: Using default decoder (no --dec-type specified)"
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  -m 2.0 \
  -M 2.0 \
  -s 0.1 \
  -e 5 2>&1 | grep -E "(Type \(D\)|FER|BER)" | head -5

echo ""
echo "Method 2: Explicitly specify BP_FLOODING"
./build/bin/aff3ct \
  -C "LDPC" \
  -K 3 \
  -N 6 \
  --dec-h-path conf/dec/LDPC/DEBUG_6_3.alist \
  --dec-type "BP_FLOODING" \
  -m 2.0 \
  -M 2.0 \
  -s 0.1 \
  -e 5 2>&1 | grep -E "(Type \(D\)|FER|BER)" | head -5

echo ""
echo "========================================"
echo "Available LDPC decoder types:"
echo "  - BP_FLOODING (default)"
echo "  - BP_HORIZONTAL_LAYERED"
echo "  - BP_HORIZONTAL_LAYERED_SIMD (your custom decoder)"
echo "  - BP_VERTICAL_LAYERED"
echo "  - BP_PEELING"
echo "  - BIT_FLIPPING"
echo "  - CHASE"
echo "  - ML"
echo "========================================"

