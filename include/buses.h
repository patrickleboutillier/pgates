#pragma once
#include "bus.h"
#include "gates.h"
#include <cstddef>
#include <memory>
#include <vector>

// Internal base for all N-wide 2-input bus gates.
// G must be a Gate subclass taking (Wire&, Wire&, Wire&).
template<size_t N, typename G>
class BusGate2 {
public:
    BusGate2(Bus<N>& a, Bus<N>& b, Bus<N>& out) {
        for (size_t i = 0; i < N; ++i)
            gates_.push_back(std::make_unique<G>(a[i], b[i], out[i]));
    }
private:
    std::vector<std::unique_ptr<G>> gates_;
};

template<size_t N> using ANDer  = BusGate2<N, AND>;
template<size_t N> using ORer   = BusGate2<N, OR>;
template<size_t N> using NANDer = BusGate2<N, NAND>;
template<size_t N> using NORer  = BusGate2<N, NOR>;
template<size_t N> using XORer  = BusGate2<N, XOR>;

// N-wide inverter: out[i] = NOT in[i].
template<size_t N>
class NOTer {
public:
    NOTer(Bus<N>& in, Bus<N>& out) {
        for (size_t i = 0; i < N; ++i)
            gates_.push_back(std::make_unique<NOT>(in[i], out[i]));
    }
private:
    std::vector<std::unique_ptr<NOT>> gates_;
};
