#!/bin/bash
# Visual test script - shows one frame with clear before/after comparison

echo "========================================"
echo "LDPC Decoder - Single Frame Visualization"
echo "Code: DEBUG_6_3 (K=3 info bits, N=6 codeword)"
echo "========================================"
echo ""

# Run one frame and extract key information
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
  -e 1 \
  --sim-dbg 2>&1 | awk '
    /Channel_AWGN_LLR::add_noise/ { 
        getline; getline; 
        if ($0 ~ /Y_N =/) {
            gsub(/\[|\]/, "", $0);
            split($0, arr, "=");
            split(arr[2], y_n, ",");
            print "INPUT (Noisy LLRs from channel):";
            printf "  Y_N = [";
            for (i=1; i<=length(y_n); i++) {
                printf "%6.2f ", y_n[i];
            }
            printf "]\n";
            print "";
            print "Hard decision BEFORE decoding:";
            printf "  Bits = [";
            for (i=1; i<=length(y_n); i++) {
                bit = (y_n[i] < 0) ? 1 : 0;
                printf "%d ", bit;
            }
            printf "]\n";
            print "";
        }
    }
    /Decoder_LDPC_BP_horizontal_layered_SIMD::decode_siho/ {
        getline;
        if ($0 ~ /Y_N =/) {
            gsub(/\[|\]/, "", $0);
            split($0, arr, "=");
            split(arr[2], y_n, ",");
            print "Decoder input (LLRs):";
            printf "  Y_N = [";
            for (i=1; i<=length(y_n); i++) {
                printf "%6.2f ", y_n[i];
            }
            printf "]\n";
        }
        getline;
        if ($0 ~ /V_K =/) {
            gsub(/\[|\]/, "", $0);
            split($0, arr, "=");
            split(arr[2], v_k, ",");
            print "";
            print "OUTPUT (Decoded information bits):";
            printf "  V_K = [";
            for (i=1; i<=length(v_k); i++) {
                printf "%d ", v_k[i];
            }
            printf "]\n";
        }
        if ($0 ~ /CWD =/) {
            gsub(/\[|\]/, "", $0);
            split($0, arr, "=");
            cwd = arr[2];
            gsub(/ /, "", cwd);
            print "";
            print "Codeword status:";
            if (cwd == "0") {
                print "  CWD = 0 (Valid codeword - syndrome check passed!)";
            } else {
                print "  CWD = 1 (Invalid codeword - may have errors)";
            }
        }
    }
'

echo ""
echo "========================================"
echo "Interpretation:"
echo "  - Positive LLR values → likely bit 0"
echo "  - Negative LLR values → likely bit 1"
echo "  - Larger absolute value → more confident"
echo "  - CWD = 0 means the decoded codeword satisfies all parity checks"
echo "========================================"

