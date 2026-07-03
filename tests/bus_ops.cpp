#include "bus.h"
#include "buses.h"
#include "circuit.h"

#include <cstdio>

static int passed = 0, failed = 0;

static void expect(const char* label, uint64_t got, uint64_t want) {
    if (got == want) {
        printf("  pass  %-34s = 0x%llx\n", label, (unsigned long long)got);
        ++passed;
    } else {
        printf("  FAIL  %-34s = 0x%llx  (expected 0x%llx)\n",
               label, (unsigned long long)got, (unsigned long long)want);
        ++failed;
    }
}

static void test_ander() {
    printf("ANDer<4>:\n");
    Bus<4> a, b, out;
    ANDer<4> g(a, b, out);
    Circuit::start();
    a.set(0); b.set(0); out.wait_for(0);
    struct { uint64_t a, b, want; } rows[] = {
        {0b0000, 0b0000, 0b0000},
        {0b1111, 0b1111, 0b1111},
        {0b1010, 0b1100, 0b1000},
        {0b0101, 0b1111, 0b0101},
    };
    for (auto& r : rows) {
        a.set(r.a); b.set(r.b);
        out.wait_for(r.want);
        char label[64];
        snprintf(label, sizeof(label), "AND(0x%llx, 0x%llx)",
                 (unsigned long long)r.a, (unsigned long long)r.b);
        expect(label, out.last(), r.want);
    }
    Circuit::stop();
}

static void test_orer() {
    printf("ORer<4>:\n");
    Bus<4> a, b, out;
    ORer<4> g(a, b, out);
    Circuit::start();
    a.set(0); b.set(0); out.wait_for(0);
    struct { uint64_t a, b, want; } rows[] = {
        {0b0000, 0b0000, 0b0000},
        {0b1010, 0b0101, 0b1111},
        {0b1100, 0b0011, 0b1111},
        {0b1010, 0b1100, 0b1110},
    };
    for (auto& r : rows) {
        a.set(r.a); b.set(r.b);
        out.wait_for(r.want);
        char label[64];
        snprintf(label, sizeof(label), "OR(0x%llx, 0x%llx)",
                 (unsigned long long)r.a, (unsigned long long)r.b);
        expect(label, out.last(), r.want);
    }
    Circuit::stop();
}

static void test_noter() {
    printf("NOTer<4>:\n");
    Bus<4> in, out;
    NOTer<4> g(in, out);
    Circuit::start();
    out.wait_for(0b1111);  // NOT(0000) = 1111 at startup
    expect("NOT(0b0000)", out.last(), 0b1111);
    in.set(0b1010);
    out.wait_for(0b0101);
    expect("NOT(0b1010)", out.last(), 0b0101);
    in.set(0b1111);
    out.wait_for(0b0000);
    expect("NOT(0b1111)", out.last(), 0b0000);
    Circuit::stop();
}

static void test_xorer() {
    printf("XORer<4>:\n");
    Bus<4> a, b, out;
    XORer<4> g(a, b, out);
    Circuit::start();
    a.set(0); b.set(0); out.wait_for(0);
    struct { uint64_t a, b, want; } rows[] = {
        {0b1010, 0b1100, 0b0110},
        {0b1111, 0b0000, 0b1111},
        {0b1111, 0b1111, 0b0000},
    };
    for (auto& r : rows) {
        a.set(r.a); b.set(r.b);
        out.wait_for(r.want);
        char label[64];
        snprintf(label, sizeof(label), "XOR(0x%llx, 0x%llx)",
                 (unsigned long long)r.a, (unsigned long long)r.b);
        expect(label, out.last(), r.want);
    }
    Circuit::stop();
}

int main() {
    test_ander();
    test_orer();
    test_noter();
    test_xorer();

    printf("\n%d/%d passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n");
    return failed ? 1 : 0;
}
