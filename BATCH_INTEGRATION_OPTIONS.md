# Options for Integrating Batch Processor

## Overview
The `decode_batch_parallel` function exists but is not integrated into AFF3CT's normal decode path. Here are the integration options:

## Option 1: Use `n_frames_per_wave` (Recommended for SIMD)
**How it works:** AFF3CT has a concept of "frames per wave" where multiple frames are processed together in a single call. This is how SIMD-optimized decoders work.

**Pros:**
- Native AFF3CT support
- Works with existing task system
- Used by other SIMD decoders (e.g., `BP_FLOODING` with SIMD)

**Cons:**
- Requires reordering data (interleaving frames)
- More complex implementation

**Implementation:**
```cpp
// In constructor, set frames per wave
this->set_n_frames_per_wave(8); // Process 8 frames at once

// In _decode_siho, process multiple frames
int _decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id)
{
    const auto cur_wave = frame_id / this->get_n_frames_per_wave();
    const auto n_frames = this->get_n_frames_per_wave();
    
    // Reorder input frames for batch processing
    std::vector<const R*> frames_in(n_frames);
    for (auto f = 0; f < n_frames; f++)
        frames_in[f] = Y_N + f * this->N;
    tools::Reorderer<R>::apply(frames_in, reordered_input.data(), this->N);
    
    // Process batch in parallel
    decode_batch_parallel(reordered_input.data(), CWD, V_K, n_frames, this->N);
    
    // Reorder output back
    // ...
}
```

## Option 2: Override the Codelet (Direct Integration)
**How it works:** Override the task codelet to call `decode_batch_parallel` instead of `_decode_siho`.

**Pros:**
- Direct control over batch processing
- No need to change existing `_decode_siho` logic
- Can process arbitrary batch sizes

**Cons:**
- Bypasses normal AFF3CT flow
- May conflict with frame management

**Implementation:**
```cpp
// In constructor, override the codelet
auto& p1 = this->create_task("decode_siho", (int)dec::tsk::decode_siho);
// ... create sockets ...

this->create_codelet(
    p1,
    [this](spu::module::Module& m, spu::runtime::Task& t, const size_t frame_id) -> int
    {
        auto& dec = static_cast<Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>&>(m);
        
        // Get batch size from available frames
        const size_t batch_size = std::min(dec.get_n_frames(), 8u);
        
        // Collect frames for batch processing
        std::vector<R> Y_N_batch;
        std::vector<int8_t> CWD_batch;
        std::vector<B> V_K_batch;
        
        // ... collect data from multiple frames ...
        
        // Process batch
        dec.decode_batch_parallel(
            Y_N_batch.data(),
            CWD_batch.data(),
            V_K_batch.data(),
            batch_size,
            dec.get_N()
        );
        
        return 0;
    });
```

## Option 3: Async Decode with Multiple frame_ids
**How it works:** Modify `_decode_siho` to accept and process multiple frame_ids asynchronously.

**Pros:**
- Natural extension of existing interface
- Can use async/await or futures
- Flexible batch size

**Cons:**
- Requires changing method signatures
- May need to modify task system

**Implementation:**
```cpp
// New method signature
int _decode_siho_batch(const R* Y_N_batch, 
                       int8_t* CWD_batch, 
                       B* V_K_batch,
                       const std::vector<size_t>& frame_ids)
{
    // Use decode_batch_parallel internally
    decode_batch_parallel(Y_N_batch, CWD_batch, V_K_batch, 
                         frame_ids.size(), this->N);
    return 0;
}

// In _decode_siho, collect frames and call batch version
int _decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id)
{
    // Collect nearby frames for batch processing
    std::vector<size_t> frame_ids = {frame_id, frame_id+1, ...};
    return _decode_siho_batch(Y_N, CWD, V_K, frame_ids);
}
```

## Option 4: Custom Task with Batch Processing
**How it works:** Create a new task specifically for batch processing, separate from `decode_siho`.

**Pros:**
- Doesn't interfere with existing decode path
- Can be called explicitly when needed
- Clean separation of concerns

**Cons:**
- Requires custom simulation code
- Not integrated into standard AFF3CT flow

**Implementation:**
```cpp
// In constructor
auto& p_batch = this->create_task("decode_batch", custom_task_id);
auto p_batch_Y_N = this->template create_socket_in<R>(p_batch, "Y_N_batch", this->N * max_batch_size);
auto p_batch_CWD = this->template create_socket_out<int8_t>(p_batch, "CWD_batch", max_batch_size);
auto p_batch_V_K = this->template create_socket_out<B>(p_batch, "V_K_batch", this->K * max_batch_size);

this->create_codelet(
    p_batch,
    [this](spu::module::Module& m, spu::runtime::Task& t, const size_t frame_id) -> int
    {
        auto& dec = static_cast<Decoder_LDPC_BP_horizontal_layered_SIMD<B, R>&>(m);
        // Call decode_batch_parallel
        return 0;
    });
```

## Option 5: Thread Pool in _decode_siho
**How it works:** Use a thread pool inside `_decode_siho` to process multiple frames concurrently.

**Pros:**
- No changes to AFF3CT interface
- Can process frames as they arrive
- Flexible implementation

**Cons:**
- Requires thread pool management
- May create too many threads
- Synchronization overhead

**Implementation:**
```cpp
// Thread pool member
std::vector<std::thread> thread_pool;
std::queue<std::function<void()>> work_queue;
std::mutex queue_mutex;

int _decode_siho(const R* Y_N, int8_t* CWD, B* V_K, const size_t frame_id)
{
    // Add to work queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        work_queue.push([=]() {
            this->_decode_siho_impl(Y_N, CWD, V_K, frame_id);
        });
    }
    
    // Process batch when queue is full
    if (work_queue.size() >= batch_size) {
        process_batch();
    }
}
```

## Recommendation

**For SIMD optimization:** Use **Option 1** (n_frames_per_wave) - this is the standard AFF3CT approach and works well with SIMD instructions.

**For maximum flexibility:** Use **Option 2** (override codelet) - gives you full control while staying within AFF3CT's framework.

**For quick integration:** Use **Option 3** (async decode) - minimal changes to existing code.

