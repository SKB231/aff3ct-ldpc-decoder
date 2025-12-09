/*!
 * \file
 * \brief Class module::Decoder_LDPC_BP_horizontal_layered_SIMD.
 */
#ifndef DECODER_LDPC_BP_HORIZONTAL_LAYERED_SIMD_HPP_
#define DECODER_LDPC_BP_HORIZONTAL_LAYERED_SIMD_HPP_

#include <cstdint>
#include <vector>

#include "Module/Decoder/Decoder_SISO.hpp"
#include "Module/Decoder/LDPC/BP/Decoder_LDPC_BP.hpp"
#include "Tools/Algo/Matrix/Sparse_matrix/Sparse_matrix.hpp"

// Intel SIMD intrinsics
#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace aff3ct
{
namespace module
{
/*!
 * \brief SIMD-optimized horizontal layered LDPC BP decoder
 *
 * This decoder uses Intel SIMD instructions (AVX2/AVX-512) to accelerate
 * LDPC decoding operations. Currently implements basic SIMD load/store
 * operations as a foundation for future optimizations.
 */
template<typename B = int, typename R = double>
class Decoder_LDPC_BP_horizontal_layered_SIMD
  : public Decoder_SISO<B, R>
  , public Decoder_LDPC_BP
{
  protected:
    const std::vector<uint32_t> info_bits_pos;

    // data structures for iterative decoding
    std::vector<std::vector<R>> var_nodes;
    std::vector<std::vector<R>> messages;
    std::vector<R> contributions;

// SIMD parameters
#ifdef __AVX2__
    static constexpr int SIMD_WIDTH = sizeof(R) == 4 ? 8 : 4; // 8 floats or 4 doubles for AVX2
#else
    static constexpr int SIMD_WIDTH = 1; // Scalar fallback
#endif

  public:
    Decoder_LDPC_BP_horizontal_layered_SIMD(const int K,
                                            const int N,
                                            const int n_ite,
                                            const tools::Sparse_matrix& H,
                                            const std::vector<unsigned>& info_bits_pos,
                                            const bool enable_syndrome = true,
                                            const int syndrome_depth = 1);
    virtual ~Decoder_LDPC_BP_horizontal_layered_SIMD() = default;

    virtual Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>* clone() const;

    virtual void set_n_frames(const size_t n_frames);

    /*!
     * \brief High-level function to decode multiple codewords in parallel across cores
     *
     * This function splits the workload across different CPU cores, where each core
     * processes a subset of codewords. This is the high-level parallel implementation
     * before SIMD optimizations are added.
     *
     * Example usage:
     * \code
     *   const size_t n_codewords = 100;
     *   std::vector<R> Y_N_batch(n_codewords * codeword_size);
     *   std::vector<int8_t> CWD_batch(n_codewords);
     *   std::vector<B> V_K_batch(n_codewords * decoder.get_K());
     *
     *   // Fill Y_N_batch with noisy codewords...
     *
     *   decoder.decode_batch_parallel(
     *       Y_N_batch.data(),
     *       CWD_batch.data(),
     *       V_K_batch.data(),
     *       n_codewords,
     *       codeword_size
     *   );
     * \endcode
     *
     * \param Y_N_batch: Pointer to batch of noisy codewords (n_codewords * codeword_size elements)
     * \param CWD_batch: Pointer to batch of codeword status flags (n_codewords elements)
     * \param V_K_batch: Pointer to batch of decoded information bits (n_codewords * K elements)
     * \param n_codewords: Number of codewords to decode
     * \param codeword_size: Size of each codeword (should equal N)
     */
    void decode_batch_parallel(const R* Y_N_batch,
                               int8_t* CWD_batch,
                               B* V_K_batch,
                               const size_t n_codewords,
                               const size_t codeword_size);

    /*!
     * \brief Verify that decoded codewords satisfy the parity check (syndrome check)
     *
     * This function verifies that the decoded codewords are valid by checking
     * if they satisfy the parity check equations (H * codeword^T = 0).
     *
     * \param V_N_batch: Pointer to batch of decoded full codewords (n_codewords * N elements)
     * \param n_codewords: Number of codewords to verify
     * \return Number of valid codewords (codewords that pass syndrome check)
     */
    size_t verify_batch(const B* V_N_batch, const size_t n_codewords) const;

    /*!
     * \brief Verify a single decoded codeword using syndrome check
     *
     * \param V_N: Pointer to decoded full codeword (N elements)
     * \return true if codeword is valid (passes syndrome check), false otherwise
     */
    bool verify_codeword(const B* V_N) const;

    /*!
     * \brief Verify decoded information bits by reconstructing full codeword and checking syndrome
     *
     * This function takes decoded information bits, reconstructs the full codeword
     * (if encoder is available), and verifies it using syndrome check.
     *
     * \param V_K_batch: Pointer to batch of decoded information bits (n_codewords * K elements)
     * \param n_codewords: Number of codewords to verify
     * \return Number of valid codewords
     */
    size_t verify_info_bits_batch(const B* V_K_batch, const size_t n_codewords) const;

  protected:
    void _reset(const size_t frame_id);

    int _decode_siso(const R* Y_N1, int8_t* CWD, R* Y_N2, const size_t frame_id);
    int _decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id);
    int _decode_siho_cw(const R* Y_N, int8_t* CWD, B* V_N, const size_t frame_id);

    void _load(const R* Y_N, const size_t frame_id);
    int _decode(const size_t frame_id);
    void _decode_single_ite(std::vector<R>& var_nodes, std::vector<R>& messages);

    void _decode_codeword_range(const R* Y_N_batch,
                                int8_t* CWD_batch,
                                B* V_K_batch,
                                const size_t start_idx,
                                const size_t end_idx,
                                const size_t codeword_size);

    void _load_simd(const R* src, R* dst, const size_t len);
    void _store_simd(const R* src, R* dst, const size_t len);
};
}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define DECODER_LDPC_BP_HORIZONTAL_LAYERED_SIMD_HXX_INCLUDED
#include "Module/Decoder/LDPC/BP/Horizontal_layered/Decoder_LDPC_BP_horizontal_layered_SIMD.hxx"
#endif

#endif
