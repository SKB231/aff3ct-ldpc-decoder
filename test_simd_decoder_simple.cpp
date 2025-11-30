/*!
 * \file
 * \brief Simple test program to visualize LDPC decoding with small code
 * 
 * This program demonstrates the SIMD decoder with a very small code (6 bits, 3 check nodes)
 * so you can visually see the input noisy codeword and output corrected codeword.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include "aff3ct.hpp"
#include "Factory/Module/Decoder/LDPC/Decoder_LDPC.hpp"
#include "Tools/Code/LDPC/Matrix_handler/LDPC_matrix_handler.hpp"
#include "Module/Decoder/LDPC/BP/Horizontal_layered/Decoder_LDPC_BP_horizontal_layered_SIMD.hpp"

using namespace aff3ct;
using namespace aff3ct::factory;
using namespace aff3ct::module;
using namespace aff3ct::tools;

void print_vector(const std::string& name, const std::vector<float>& vec, int bits_per_line = 8)
{
    std::cout << name << " (" << vec.size() << " elements):" << std::endl;
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (i > 0 && i % bits_per_line == 0) std::cout << std::endl;
        std::cout << std::setw(8) << std::fixed << std::setprecision(3) << vec[i] << " ";
    }
    std::cout << std::endl << std::endl;
}

void print_bits(const std::string& name, const std::vector<int>& vec, int bits_per_line = 8)
{
    std::cout << name << " (" << vec.size() << " bits):" << std::endl;
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (i > 0 && i % bits_per_line == 0) std::cout << std::endl;
        std::cout << vec[i] << " ";
    }
    std::cout << std::endl << std::endl;
}

int main(int argc, char** argv)
{
    // Very small test code: 6 variable nodes, 3 check nodes
    const int K = 3;  // Information bits
    const int N = 6;  // Codeword size
    const int n_ite = 5;
    
    std::cout << "========================================" << std::endl;
    std::cout << "LDPC Decoder Visualization Test" << std::endl;
    std::cout << "Code: K=" << K << ", N=" << N << std::endl;
    std::cout << "========================================" << std::endl << std::endl;
    
    // Load the H matrix
    std::string H_path = "conf/dec/LDPC/DEBUG_6_3.alist";
    Sparse_matrix H = LDPC_matrix_handler::read(H_path);
    
    // Get info bits positions (for this small code, assume first K bits are info)
    std::vector<unsigned> info_bits_pos;
    for (int i = 0; i < K; i++)
        info_bits_pos.push_back(i);
    
    // Create decoder
    Decoder_LDPC dec_params("dec");
    dec_params.type = "BP_HORIZONTAL_LAYERED_SIMD";
    dec_params.n_ite = n_ite;
    dec_params.enable_syndrome = true;
    
    auto* decoder = dec_params.build<int, float>(H, info_bits_pos);
    
    std::cout << "Decoder created: " << decoder->get_name() << std::endl << std::endl;
    
    // Create a simple test case: original codeword (all zeros for simplicity)
    std::vector<int> original_codeword(N, 0);
    std::cout << "Original codeword (before encoding):" << std::endl;
    print_bits("  Original", original_codeword);
    
    // Simulate noisy channel: add some noise to create LLRs
    // Positive LLR = likely 0, Negative LLR = likely 1
    std::vector<float> Y_N(N);
    
    // Example: introduce errors at positions 1 and 4
    // Strong positive = 0, Strong negative = 1, Small values = uncertain
    Y_N[0] = +2.5f;  // Strong 0
    Y_N[1] = -1.8f;  // Error! Should be 0 but LLR suggests 1
    Y_N[2] = +1.2f;  // Weak 0
    Y_N[3] = +0.5f;  // Very weak 0
    Y_N[4] = -2.1f;  // Error! Should be 0 but LLR suggests 1
    Y_N[5] = +1.8f;  // Strong 0
    
    std::cout << "Noisy codeword (LLRs from channel):" << std::endl;
    print_vector("  Y_N (LLRs)", Y_N);
    
    // Hard decision before decoding
    std::vector<int> hard_before(N);
    for (int i = 0; i < N; i++)
        hard_before[i] = (Y_N[i] < 0) ? 1 : 0;
    
    std::cout << "Hard decision BEFORE decoding:" << std::endl;
    print_bits("  Hard Y_N", hard_before);
    std::cout << "  Errors: ";
    int errors_before = 0;
    for (int i = 0; i < N; i++)
    {
        if (hard_before[i] != original_codeword[i])
        {
            std::cout << "bit[" << i << "] ";
            errors_before++;
        }
    }
    std::cout << "(" << errors_before << " errors)" << std::endl << std::endl;
    
    // Decode
    std::vector<int8_t> CWD(1);
    std::vector<int> V_K(K);
    std::vector<int> V_N(N);
    
    // Cast decoder to our SIMD decoder type to access verify function
    auto* simd_decoder = dynamic_cast<Decoder_LDPC_BP_horizontal_layered_SIMD<int, float>*>(decoder);
    
    int status = decoder->decode_siho_cw(Y_N.data(), CWD.data(), V_N.data(), 0);
    
    std::cout << "Decoding result:" << std::endl;
    std::cout << "  Status (0=success): " << (int)status << std::endl;
    std::cout << "  CWD (0=valid codeword): " << (int)CWD[0] << std::endl << std::endl;
    
    // Hard decision after decoding
    std::cout << "Hard decision AFTER decoding:" << std::endl;
    print_bits("  Decoded V_N", V_N);
    
    std::cout << "Information bits extracted:" << std::endl;
    print_bits("  V_K", V_K);
    
    // Check for errors
    std::cout << "Errors after decoding: ";
    int errors_after = 0;
    for (int i = 0; i < N; i++)
    {
        if (V_N[i] != original_codeword[i])
        {
            std::cout << "bit[" << i << "] ";
            errors_after++;
        }
    }
    if (errors_after == 0)
        std::cout << "NONE - Decoding successful!" << std::endl;
    else
        std::cout << "(" << errors_after << " errors remain)" << std::endl;
    
    // Verify codeword
    bool is_valid = false;
    if (simd_decoder)
    {
        is_valid = simd_decoder->verify_codeword(V_N.data());
        std::cout << "Syndrome check: " << (is_valid ? "PASSED (valid codeword)" : "FAILED") << std::endl;
    }
    else
    {
        std::cout << "Syndrome check: (decoder type not available for verification)" << std::endl;
    }
    
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  Errors before: " << errors_before << std::endl;
    std::cout << "  Errors after:  " << errors_after << std::endl;
    std::cout << "  Correction:     " << (errors_before - errors_after) << " bits corrected" << std::endl;
    std::cout << "========================================" << std::endl;
    
    delete decoder;
    return 0;
}

