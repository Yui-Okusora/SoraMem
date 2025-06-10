# SoraMem

A Memory Management System based on memory-mapped files allows fast access and reduces RAM usage for random/sequential reading/writing of large files.

*** Basically, trading disk storage for more RAM. ***

---

# SoraMem Feature Roadmap: Performance and Reliability

## Performance Enhancements

### 1. SIMD-Accelerated Memory Copy ✅
- Utilize AVX2 or AVX-512 instructions to boost data copy speed.
- Optimized for large memory-mapped file segments.

### 2. Multi-threaded Chunked Copying ✅
- Divide data into system-aligned chunks (e.g., 64 KB).
- Process chunks in parallel using up to 4 threads.

### 3. Memory/File Pooling ✅
- Reuse `MMFile` objects and associated file handles.
- Reduces system call overhead and improves runtime efficiency.

### 4. Alignment Optimization ✅
- Align file sizes to system granularity (e.g., 64 KB).
- Align memory allocations to 32/64/512 bytes for SIMD compatibility.

### 5. Branchless Logic ✅
- Replace `if/else` with ternary or arithmetic-based logic in performance-critical sections.

### 6. (Optional) Copy-on-Write Views
- Enable shared views with isolation on write for memory efficiency.

### 7. Memory View Pooling
- Reuse `MemView` objects to reduce mapping overhead.
- Avoid unnecessary `MapViewOfFile`/`UnmapViewOfFile` cycles.

### 8. Async I/O with Overlapped I/O
- Use Windows `OVERLAPPED` structure for asynchronous file operations.
- Improves responsiveness and concurrency in heavy-load scenarios.

### 9. Memory Prefetching
- Implement hardware prefetch hints or software lookahead.
- Reduce latency in sequential and semi-random access patterns.

### 10. Cross-Process Shared Memory
- Allow MMFs to be shared across different processes.
- Use named mappings and security attributes.

### 11. Custom Allocator Interface
- Provide an abstract interface for plug-and-play memory allocators.
- Allow integration with external memory management strategies.

### 12. LRU Cache for Views
- Manage mapped views using a Least Recently Used (LRU) strategy.
- Optimize usage under view mapping limits.

### 13. Memory Snapshot and Restore
- Enable save/load of current mapped state and memory contents.
- Useful for checkpointing and debugging.

---

## Reliability and Robustness

### 1. File Integrity Checking ✅
- Add CRC32/64 for fast validation.
- Use SHA-256 for secure and tamper-proof verification.

### 2. Crash-Safe RAII Wrapper
- Use RAII and smart pointers for exception safety.
- Prevent memory leaks and undefined states.

### 3. Memory Pressure Awareness
- Monitor system state via:
  - `VirtualQuery`
  - `GlobalMemoryStatusEx`
  - `GetProcessMemoryInfo`
- Adapt behavior based on available virtual/physical memory.

### 4. Safe File Flushing
- Use `FlushViewOfFile` and `FlushFileBuffers` to ensure memory-mapped changes are committed.

### 5. Optional Compression Layer
- Use `Zstd` (high compression, fast decompression) or `LZ4` (ultra-fast).
- Apply to archival or non-active save files.

### 6. Metadata & Signature Blocks ✅
- Add magic numbers, file IDs, and hashes to file headers.
- Verify during load to detect corruption or format mismatch.

### 7. Namespacing and Modularization ✅
- Use a universal namespace (e.g., `okusora::`, `soramem::`).
- Split independent features (e.g., hashing, memory pooling, compression) into separate files/modules.

