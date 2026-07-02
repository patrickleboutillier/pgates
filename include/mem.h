#pragma once
#include "bus.h"
#include "gates.h"
#include "wire.h"
#include <cstddef>
#include <memory>
#include <vector>

// Gated D-latch (single-bit memory cell).
//   s=1: output follows input  (write / transparent mode)
//   s=0: output holds last value  (hold mode)
// Initialised to store 0 at power-on (internal wc_ starts HIGH).
//
// Circuit (mirrors jcscpu memory.go):
//   wa = NAND(i,  s)
//   wb = NAND(wa, s)
//   o  = NAND(wa, wc)
//   wc = NAND(o,  wb)
class MEM {
public:
    MEM(Wire& i, Wire& s, Wire& o);
private:
    Wire wa_, wb_;
    Wire wc_;              // starts HIGH so o powers on as 0
    NAND nand1_, nand2_, nand3_, nand4_;
};

// N-bit memory register: N independent D-latches sharing one set wire.
// MEMer object must outlive Circuit::stop().
template<size_t N>
class MEMer {
    static_assert(N >= 1, "MEMer width must be >= 1");
public:
    MEMer(Bus<N>& i, Wire& s, Bus<N>& o) {
        for (size_t k = 0; k < N; ++k)
            cells_.push_back(std::make_unique<MEM>(i[k], s, o[k]));
    }
private:
    std::vector<std::unique_ptr<MEM>> cells_;
};
