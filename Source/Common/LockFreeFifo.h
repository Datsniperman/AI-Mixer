#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cstring>

namespace aimixer {

// ==============================================================================
// Lock-free Single-Producer Single-Consumer (SPSC) Ring Buffer
// Thread-safe between Audio Thread (producer) and Analysis Thread (consumer).
// Zero heap allocations in push/pop once initialized.
// ==============================================================================
template <typename T, size_t Capacity>
class LockFreeFifo {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    LockFreeFifo() {
        buffer.resize(Capacity);
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }

    void reset() noexcept {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }

    // Number of elements available to read
    size_t getNumAvailableToRead() const noexcept {
        const size_t w = writeIndex.load(std::memory_order_acquire);
        const size_t r = readIndex.load(std::memory_order_relaxed);
        return (w > r) ? (w - r) : 0;
    }

    // Available space to write
    size_t getFreeSpace() const noexcept {
        const size_t avail = getNumAvailableToRead();
        return (avail < Capacity - 1) ? ((Capacity - 1) - avail) : 0;
    }

    // Producer (Audio Thread): write elements into ring buffer
    // Returns number of elements successfully written (drops if full to preserve real-time safety)
    size_t write(const T* data, size_t numElements) noexcept {
        if (numElements == 0 || data == nullptr) return 0;

        const size_t w = writeIndex.load(std::memory_order_relaxed);
        const size_t r = readIndex.load(std::memory_order_acquire);

        const size_t occupied = (w > r) ? (w - r) : 0;
        if (occupied >= Capacity - 1) return 0;

        const size_t available = (Capacity - 1) - occupied;
        const size_t toWrite = std::min(numElements, available);
        if (toWrite == 0) return 0;

        const size_t mask = Capacity - 1;
        const size_t wMask = w & mask;
        const size_t firstChunk = std::min(toWrite, Capacity - wMask);
        std::memcpy(&buffer[wMask], data, firstChunk * sizeof(T));

        if (toWrite > firstChunk) {
            std::memcpy(&buffer[0], data + firstChunk, (toWrite - firstChunk) * sizeof(T));
        }

        writeIndex.store(w + toWrite, std::memory_order_release);
        return toWrite;
    }

    // Consumer (Worker Thread): read elements out of ring buffer
    // Returns number of elements actually read
    size_t read(T* destination, size_t maxElements) noexcept {
        if (maxElements == 0 || destination == nullptr) return 0;

        const size_t w = writeIndex.load(std::memory_order_acquire);
        const size_t r = readIndex.load(std::memory_order_relaxed);

        if (w <= r) return 0;
        const size_t available = w - r;
        const size_t toRead = std::min(maxElements, available);
        if (toRead == 0) return 0;

        const size_t mask = Capacity - 1;
        const size_t rMask = r & mask;
        const size_t firstChunk = std::min(toRead, Capacity - rMask);
        std::memcpy(destination, &buffer[rMask], firstChunk * sizeof(T));

        if (toRead > firstChunk) {
            std::memcpy(destination + firstChunk, &buffer[0], (toRead - firstChunk) * sizeof(T));
        }

        readIndex.store(r + toRead, std::memory_order_release);
        return toRead;
    }

private:
    std::vector<T> buffer;
    alignas(64) std::atomic<size_t> writeIndex{0};
    alignas(64) std::atomic<size_t> readIndex{0};
};

} // namespace aimixer
