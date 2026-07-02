#include "adder.h"

// Member init order follows declaration order (Wire members first, then gates).
// xor1_ computes axb_ = a ^ b; xor2_ completes sum = axb_ ^ cin.
// and1_ computes the (a & b) carry term; and2_ the ((a^b) & cin) term.
// or_ combines both carry terms into co.
ADD::ADD(Wire& a, Wire& b, Wire& cin, Wire& sum, Wire& co)
    : xor1_(a, b, axb_),
      xor2_(axb_, cin, sum),
      and1_(a, b, c1_),
      and2_(axb_, cin, c2_),
      or_(c1_, c2_, co)
{}
