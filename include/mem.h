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
    Wire& qbar() { return wc_; }  // /Q — must be 0 (1) after storing 1 (0)
    void settle(uint8_t written_val) { wc_.wait_for(written_val ? 0 : 1); }
private:
    Wire wa_, wb_;
    Wire wc_;              // starts HIGH so o powers on as 0
    NAND nand1_, nand2_, nand3_, nand4_;
};

// N-bit memory register: N independent D-latches sharing one set wire.
// BYTE object must outlive Circuit::stop().
template<size_t N>
class BYTE {
    static_assert(N >= 1, "BYTE width must be >= 1");
public:
    BYTE(Bus<N>& i, Wire& s, Bus<N>& o) {
        for (size_t k = 0; k < N; ++k)
            cells_.push_back(std::make_unique<MEM>(i[k], s, o[k]));
    }
    Wire& qbar(size_t k) { return cells_[k]->qbar(); }
    void settle(uint64_t written_val) {
        for (size_t k = 0; k < N; ++k)
            cells_[k]->settle((written_val >> k) & 1);
    }
private:
    std::vector<std::unique_ptr<MEM>> cells_;
};
