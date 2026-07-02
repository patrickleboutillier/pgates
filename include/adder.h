#pragma once
#include "bus.h"
#include "gates.h"
#include "wire.h"
#include <cstddef>
#include <memory>
#include <vector>

// Full adder: sum = a ^ b ^ cin,  co = (a & b) | ((a ^ b) & cin).
// All five wire references are caller-owned; ADD must outlive Circuit::stop().
class ADD {
public:
    ADD(Wire& a, Wire& b, Wire& cin, Wire& sum, Wire& co);
private:
    Wire axb_, c1_, c2_;
    XOR  xor1_, xor2_;
    AND  and1_, and2_;
    OR   or_;
};

// N-bit ripple-carry adder.
// Chains N ADD stages bit 0 → bit N-1; internal carry wires are owned here.
// The ADDer object must outlive Circuit::stop().
template<size_t N>
class ADDer {
    static_assert(N >= 1, "ADDer width must be >= 1");
public:
    ADDer(Bus<N>& a, Bus<N>& b, Wire& cin, Bus<N>& sum, Wire& co) {
        for (size_t i = 0; i + 1 < N; ++i)
            carries_.push_back(std::make_unique<Wire>());
        for (size_t i = 0; i < N; ++i) {
            Wire& ci  = (i == 0)   ? cin : *carries_[i - 1];
            Wire& coi = (i == N-1) ? co  : *carries_[i];
            adders_.push_back(std::make_unique<ADD>(a[i], b[i], ci, sum[i], coi));
        }
    }
private:
    std::vector<std::unique_ptr<Wire>> carries_;
    std::vector<std::unique_ptr<ADD>>  adders_;
};
