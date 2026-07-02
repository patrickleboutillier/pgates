#pragma once
#include "gate.h"

class Wire;

class AND : public Gate {
public:
    AND(Wire& a, Wire& b, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};

class OR : public Gate {
public:
    OR(Wire& a, Wire& b, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};

class NOT : public Gate {
public:
    NOT(Wire& in, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};

class NAND : public Gate {
public:
    NAND(Wire& a, Wire& b, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};

class NOR : public Gate {
public:
    NOR(Wire& a, Wire& b, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};

class XOR : public Gate {
public:
    XOR(Wire& a, Wire& b, Wire& out);
protected:
    uint8_t evaluate(const std::vector<uint8_t>& in) const override;
};
