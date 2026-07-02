#pragma once
#include "wire.h"
#include <array>
#include <cstdint>
#include <cstddef>

// Bus<N> — N wires treated as an N-bit value (wire[0] = bit 0, wire[N-1] = MSB).
// N must be in [1, 64].
template<size_t N>
class Bus {
    static_assert(N >= 1 && N <= 64, "Bus width must be 1–64");
public:
    Bus() = default;

    Wire&       operator[](size_t i)       { return wires_[i]; }
    const Wire& operator[](size_t i) const { return wires_[i]; }

    static constexpr size_t width = N;

    // Drive all input wires from the low N bits of val.
    void set(uint64_t val) {
        for (size_t i = 0; i < N; ++i)
            wires_[i].set((val >> i) & 1);
    }

    // Non-blocking drain: read each wire and assemble into an integer.
    uint64_t get() {
        uint64_t v = 0;
        for (size_t i = 0; i < N; ++i)
            v |= (uint64_t)wires_[i].get() << i;
        return v;
    }

    // Return cached value without reading any pipes (safe after wait_for).
    uint64_t last() const {
        uint64_t v = 0;
        for (size_t i = 0; i < N; ++i)
            v |= (uint64_t)wires_[i].last() << i;
        return v;
    }

    // Block until each wire[i] holds (target >> i) & 1, in bit order.
    void wait_for(uint64_t target) {
        for (size_t i = 0; i < N; ++i)
            wires_[i].wait_for((target >> i) & 1);
    }

private:
    std::array<Wire, N> wires_;
};
