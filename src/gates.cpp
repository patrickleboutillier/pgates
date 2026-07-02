#include "gates.h"
#include "wire.h"

AND::AND(Wire& a, Wire& b, Wire& out)
    : Gate({&a, &b}, &out) {}
uint8_t AND::evaluate(const std::vector<uint8_t>& in) const {
    return in[0] & in[1];
}

OR::OR(Wire& a, Wire& b, Wire& out)
    : Gate({&a, &b}, &out) {}
uint8_t OR::evaluate(const std::vector<uint8_t>& in) const {
    return in[0] | in[1];
}

NOT::NOT(Wire& in, Wire& out)
    : Gate({&in}, &out) {}
uint8_t NOT::evaluate(const std::vector<uint8_t>& in) const {
    return !in[0];
}

NAND::NAND(Wire& a, Wire& b, Wire& out)
    : Gate({&a, &b}, &out) {}
uint8_t NAND::evaluate(const std::vector<uint8_t>& in) const {
    return !(in[0] & in[1]);
}

NOR::NOR(Wire& a, Wire& b, Wire& out)
    : Gate({&a, &b}, &out) {}
uint8_t NOR::evaluate(const std::vector<uint8_t>& in) const {
    return !(in[0] | in[1]);
}

XOR::XOR(Wire& a, Wire& b, Wire& out)
    : Gate({&a, &b}, &out) {}
uint8_t XOR::evaluate(const std::vector<uint8_t>& in) const {
    return in[0] ^ in[1];
}
