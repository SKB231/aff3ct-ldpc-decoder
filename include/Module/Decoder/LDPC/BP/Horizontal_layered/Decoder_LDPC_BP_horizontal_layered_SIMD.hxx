#include <algorithm>
#include <cstring>
#include <string>
#include <thread>

#include "Module/Decoder/LDPC/BP/Horizontal_layered/Decoder_LDPC_BP_horizontal_layered_SIMD.hpp"
#include "Tools/Code/LDPC/Syndrome/LDPC_syndrome.hpp"
#include "Tools/Perf/common/hard_decide.h"
#include "Tools/general_utils.h"

using std::cout;
using std::endl;
namespace aff3ct
{
namespace module
{
template<typename B, typename R>
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::Decoder_LDPC_BP_horizontal_layered_SIMD(
  const int K,
  const int N,
  const int n_ite,
  const tools::Sparse_matrix& _H,
  const std::vector<unsigned>& info_bits_pos,
  const bool enable_syndrome,
  const int syndrome_depth)
  : Decoder_SISO<B, R>(K, N)
  , Decoder_LDPC_BP(K, N, n_ite, _H, enable_syndrome, syndrome_depth)
  , info_bits_pos(info_bits_pos.begin(), info_bits_pos.end())
  , var_nodes(this->n_frames, std::vector<R>(N))
  , messages(this->n_frames, std::vector<R>(this->H.get_n_connections()))
  , contributions(this->H.get_cols_max_degree())
{
    const std::string name = "Decoder_LDPC_BP_horizontal_layered_SIMD";
    this->set_name(name);
    for (auto& t : this->tasks)
        t->set_replicability(true);

    tools::check_LUT(info_bits_pos, "info_bits_pos", (size_t)K);

    // Set n_frames_per_wave for batch processing
    // Use hardware concurrency or a reasonable default (e.g., 8)
    const size_t default_wave_size = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    this->set_n_frames_per_wave(default_wave_size);

    this->reset();
}

template<typename B, typename R>
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>*
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::clone() const
{
    auto m = new Decoder_LDPC_BP_horizontal_layered_SIMD(*this);
    m->deep_copy(*this);
    return m;
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_reset(const size_t frame_id)
{
    std::fill(this->messages[frame_id].begin(), this->messages[frame_id].end(), (R)0);
    std::fill(this->var_nodes[frame_id].begin(), this->var_nodes[frame_id].end(), (R)0);
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_load(const R* Y_N, const size_t frame_id)
{
// Use SIMD for loading if available, otherwise fall back to scalar
#ifdef __AVX2__
    if constexpr (std::is_same_v<R, float>)
    {
        _load_simd(Y_N, this->var_nodes[frame_id].data(), this->N);
        return;
    }
#endif
    // Scalar fallback for non-float types or when AVX2 not available
    for (auto v = 0; v < (int)var_nodes[frame_id].size(); v++)
        this->var_nodes[frame_id][v] += Y_N[v];
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_load_simd(const R* src, R* dst, const size_t len)
{
    // Scalar fallback
    for (size_t i = 0; i < len; i++)
    {
        dst[i] += src[i];
    }
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_store_simd(const R* src, R* dst, const size_t len)
{
    std::memcpy(dst, src, len * sizeof(R));
}

template<typename B, typename R>
int
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_siso(const R* Y_N1, int8_t* CWD, R* Y_N2, const size_t frame_id)
{
    this->_load(Y_N1, frame_id);

    auto status = this->_decode(frame_id);

    for (auto v = 0; v < this->N; v++)
        Y_N2[v] = this->var_nodes[frame_id][v] - Y_N1[v];

    std::copy(Y_N2, Y_N2 + this->N, this->var_nodes[frame_id].begin());

    CWD[0] = !status;
    return status;
}

template<typename B, typename R>
int
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id)
{
    const auto n_frames_per_wave = this->get_n_frames_per_wave();
    const auto cur_wave = frame_id / n_frames_per_wave;
    const auto wave_start_frame = cur_wave * n_frames_per_wave;

    // If we're processing multiple frames per wave, use batch processing
    // The task system calls this with frame_id = wave_start_frame when n_frames_per_wave > 1
    // Data is laid out sequentially: [frame0_data, frame1_data, ..., frameN_data]
    if (n_frames_per_wave > 1)
    {
        // Calculate actual number of frames in this wave
        const auto actual_wave_size = std::min(n_frames_per_wave, this->get_n_frames() - wave_start_frame);

        std::vector<int8_t> CWD_batch(actual_wave_size);
        std::vector<B> V_K_batch(actual_wave_size * this->K);

        this->decode_batch_parallel(Y_N, // Already sequential: frame0, frame1, ...
                                    CWD_batch.data(),
                                    V_K_batch.data(),
                                    actual_wave_size,
                                    this->N);

        // Copy output back - V_K also expects sequential layout
        std::copy(V_K_batch.begin(), V_K_batch.end(), V_K);

        CWD[0] = CWD_batch[0];

        return CWD_batch[0] ? 1 : 0;
    }
    else
    {

        this->_load(Y_N, frame_id);

        auto status = this->_decode(frame_id);

        for (auto i = 0; i < this->K; i++)
        {
            const auto k = this->info_bits_pos[i];
            V_K[i] = !(this->var_nodes[frame_id][k] >= 0);
        }

        CWD[0] = !status;
        return status;
    }
}

template<typename B, typename R>
int
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_siho_cw(const R* Y_N, int8_t* CWD, B* V_N, const size_t frame_id)
{
    // LOAD
    this->_load(Y_N, frame_id);

    auto status = this->_decode(frame_id);

    tools::hard_decide(this->var_nodes[frame_id].data(), V_N, this->N);

    CWD[0] = !status;
    return status;
}

template<typename B, typename R>
int
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode(const size_t frame_id)
{
    bool valid_synd = true;
    for (auto ite = 0; ite < this->n_ite; ite++)
    {
        this->_decode_single_ite(this->var_nodes[frame_id], this->messages[frame_id]);

        valid_synd = this->check_syndrome_soft(this->var_nodes[frame_id].data());
        if (valid_synd) break;
    }

    return !valid_synd && this->enable_syndrome;
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_single_ite(std::vector<R>& var_nodes, std::vector<R>& messages)
{
    auto kr = 0;
    auto kw = 0;

    const auto n_chk_nodes = (int)this->H.get_n_cols();
    for (auto c = 0; c < n_chk_nodes; c++)
    {
        const auto chk_degree = (int)this->H[c].size();

        for (auto v = 0; v < chk_degree; v++)
        {
            this->contributions[v] = var_nodes[this->H[c][v]] - messages[kr++];
        }

        R min_val = std::abs(this->contributions[0]);
        R second_min = min_val;
        int min_idx = 0;
        int sign = (this->contributions[0] >= 0) ? 0 : -1; // Use XOR-based sign like standard MS

        for (auto v = 1; v < chk_degree; v++)
        {
            R abs_val = std::abs(this->contributions[v]);
            int var_sign = (this->contributions[v] >= 0) ? 0 : -1;

            sign ^= var_sign; // XOR for sign computation (standard MS approach)

            if (abs_val < min_val)
            {
                second_min = min_val;
                min_val = abs_val;
                min_idx = v;
            }
            else if (abs_val < second_min)
            {
                second_min = abs_val;
            }
        }

        R cst1 = std::max((R)0, second_min); // For variable with min1
        R cst2 = std::max((R)0, min_val);    // For other variables

        for (auto v = 0; v < chk_degree; v++)
        {
            R var_abs = std::abs(this->contributions[v]);
            R res_abs = (var_abs == min_val) ? cst1 : cst2;
            int var_sign = (this->contributions[v] >= 0) ? 0 : -1;
            int res_sng = sign ^ var_sign;

            R msg_val = (R)std::copysign(res_abs, res_sng);

            messages[kw] = msg_val;
            var_nodes[this->H[c][v]] = this->contributions[v] + messages[kw++];
        }
    }
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::set_n_frames(const size_t n_frames)
{
    const auto old_n_frames = this->get_n_frames();
    if (old_n_frames != n_frames)
    {
        Decoder_SISO<B, R>::set_n_frames(n_frames);

        const auto vec_size = this->var_nodes[0].size();
        const auto old_var_nodes_size = this->var_nodes.size();
        const auto new_var_nodes_size = (old_var_nodes_size / old_n_frames) * n_frames;
        this->var_nodes.resize(new_var_nodes_size, std::vector<R>(vec_size));

        const auto vec_size2 = this->messages[0].size();
        const auto old_messages_size = this->messages.size();
        const auto new_messages_size = (old_messages_size / old_n_frames) * n_frames;
        this->messages.resize(new_messages_size, std::vector<R>(vec_size2));
    }
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::decode_batch_parallel(const R* Y_N_batch,
                                                                     int8_t* CWD_batch,
                                                                     B* V_K_batch,
                                                                     const size_t n_codewords,
                                                                     const size_t codeword_size)
{
    // Get the number of available CPU cores
    const size_t n_threads = std::max(1u, std::thread::hardware_concurrency());
    const size_t codewords_per_thread = (n_codewords + n_threads - 1) / n_threads;

    std::vector<std::thread> threads;
    threads.reserve(n_threads);

    // Launch threads to process codewords in parallel
    // Each thread processes a range of codewords, distributing the workload across cores
    for (size_t t = 0; t < n_threads; t++)
    {
        const size_t start_idx = t * codewords_per_thread;
        const size_t end_idx = std::min(start_idx + codewords_per_thread, n_codewords);

        if (start_idx < n_codewords)
        {
            threads.emplace_back(&Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_codeword_range,
                                 this,
                                 Y_N_batch,
                                 CWD_batch,
                                 V_K_batch,
                                 start_idx,
                                 end_idx,
                                 codeword_size);
        }
    }

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }
}

template<typename B, typename R>
void
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::_decode_codeword_range(const R* Y_N_batch,
                                                                      int8_t* CWD_batch,
                                                                      B* V_K_batch,
                                                                      const size_t start_idx,
                                                                      const size_t end_idx,
                                                                      const size_t codeword_size)
{
    // Process each codeword in the assigned range
    for (size_t cw_idx = start_idx; cw_idx < end_idx; cw_idx++)
    {
        // Calculate offsets for this codeword in the batch
        const R* Y_N = Y_N_batch + (cw_idx * codeword_size);
        int8_t* CWD = CWD_batch + cw_idx;
        B* V_K = V_K_batch + (cw_idx * this->K);

        // Use a local frame_id that maps to our internal storage
        // For simplicity, we'll use modulo to map to available frame slots
        const size_t frame_id = cw_idx % this->get_n_frames();

        // Load the input
        this->_load(Y_N, frame_id);

        // Decode
        auto status = this->_decode(frame_id);

        // Store the output (hard decision)
        for (auto i = 0; i < this->K; i++)
        {
            const auto k = this->info_bits_pos[i];
            V_K[i] = !(this->var_nodes[frame_id][k] >= 0);
        }

        CWD[0] = !status;
    }
}

template<typename B, typename R>
bool
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::verify_codeword(const B* V_N) const
{
    // Use the syndrome check to verify the codeword
    return tools::LDPC_syndrome::check_hard(V_N, this->H);
}

template<typename B, typename R>
size_t
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::verify_batch(const B* V_N_batch, const size_t n_codewords) const
{
    size_t valid_count = 0;

    // Verify each codeword in the batch
    for (size_t cw_idx = 0; cw_idx < n_codewords; cw_idx++)
    {
        const B* V_N = V_N_batch + (cw_idx * this->N);
        if (verify_codeword(V_N))
        {
            valid_count++;
        }
    }

    return valid_count;
}

template<typename B, typename R>
size_t
Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>::verify_info_bits_batch(const B* V_K_batch,
                                                                      const size_t n_codewords) const
{
    size_t valid_count = 0;
    for (size_t cw_idx = 0; cw_idx < n_codewords && cw_idx < this->n_frames; cw_idx++)
    {
        const size_t frame_id = cw_idx % this->n_frames;
        if (this->check_syndrome_soft(this->var_nodes[frame_id].data()))
        {
            valid_count++;
        }
    }

    return valid_count;
}

}
}
