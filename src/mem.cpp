#include "mem.h"

// Internal wires are pre-set to the stable "stores 0" state:
//   wa_=1  (NAND(i=0, s=0) = 1)
//   wb_=1  (NAND(wa=1, s=0) = 1)
//   wc_=1  (NAND(o=0, wb=1) = 1  →  the /Q side of the SR latch)
//
// Combined with gate.cpp's n_pending startup fence, every gate's first
// evaluation sees consistent inputs and produces no output change, giving
// a deterministic power-on state of o=0.
MEM::MEM(Wire& i, Wire& s, Wire& o)
    : wa_("", 1),
      wb_("", 1),
      wc_("", 1),
      nand1_(i,   s,   wa_),
      nand2_(wa_, s,   wb_),
      nand3_(wa_, wc_, o),
      nand4_(o,   wb_, wc_)
{}
