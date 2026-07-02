#include "adder.h"
#include "bus.h"
#include "circuit.h"
#include "wire.h"

#include <cstdio>
#include <cstdlib>

static int passed = 0, failed = 0;

static void expect(const char* label, uint64_t got, uint64_t want) {
    if (got == want) {
        printf("  pass  %-42s = 0x%llx\n", label, (unsigned long long)got);
        ++passed;
    } else {
        printf("  FAIL  %-42s = 0x%llx  (expected 0x%llx)\n",
               label, (unsigned long long)got, (unsigned long long)want);
        ++failed;
    }
}

static void test_add() {
    printf("ADD (full adder):\n");
    Wire a, b, cin, sum, co;
    ADD g(a, b, cin, sum, co);
    Circuit::start();
    a.set(0); b.set(0); cin.set(0);
    sum.wait_for(0); co.wait_for(0);

    struct { uint8_t a, b, cin, sum, co; } rows[] = {
        {0, 0, 0,  0, 0},
        {0, 0, 1,  1, 0},
        {0, 1, 0,  1, 0},
        {0, 1, 1,  0, 1},
        {1, 0, 0,  1, 0},
        {1, 0, 1,  0, 1},
        {1, 1, 0,  0, 1},
        {1, 1, 1,  1, 1},
    };
    for (auto& r : rows) {
        a.set(r.a); b.set(r.b); cin.set(r.cin);
        sum.wait_for(r.sum);
        co.wait_for(r.co);
        char label[64];
        snprintf(label, sizeof(label), "ADD(%d, %d, cin=%d)", r.a, r.b, r.cin);
        // Encode as 2-bit result: bit1=carry, bit0=sum.  Equals a+b+cin.
        uint64_t got  = (uint64_t)sum.last() | ((uint64_t)co.last() << 1);
        uint64_t want = r.a + r.b + r.cin;
        expect(label, got, want);
    }
    Circuit::stop();
}

static void test_adder4() {
    printf("ADDer<4>:\n");
    Bus<4> a, b, sum;
    Wire cin, co;
    ADDer<4> g(a, b, cin, sum, co);
    Circuit::start();
    a.set(0); b.set(0); cin.set(0);
    sum.wait_for(0); co.wait_for(0);

    struct { uint64_t a, b; uint8_t cin; uint64_t want_sum; uint8_t want_co; } rows[] = {
        {0b0000, 0b0000, 0,  0b0000, 0},  //  0 +  0      =  0
        {0b0101, 0b0011, 0,  0b1000, 0},  //  5 +  3      =  8
        {0b0111, 0b0001, 0,  0b1000, 0},  //  7 +  1      =  8
        {0b0111, 0b0111, 0,  0b1110, 0},  //  7 +  7      = 14
        {0b1111, 0b0001, 0,  0b0000, 1},  // 15 +  1      = 16 (overflow)
        {0b0101, 0b0011, 1,  0b1001, 0},  //  5 +  3 + 1  =  9
        {0b1111, 0b1111, 1,  0b1111, 1},  // 15 + 15 + 1  = 31 = 0b1_1111
    };
    for (auto& r : rows) {
        a.set(r.a); b.set(r.b); cin.set(r.cin);
        sum.wait_for(r.want_sum);
        co.wait_for(r.want_co);
        char label[64];
        snprintf(label, sizeof(label), "ADD(0x%llx, 0x%llx, cin=%d)",
                 (unsigned long long)r.a, (unsigned long long)r.b, r.cin);
        // Encode as 5-bit result: bit4=carry, bits3:0=sum.
        uint64_t got  = sum.last() | ((uint64_t)co.last() << 4);
        uint64_t want = r.want_sum | ((uint64_t)r.want_co << 4);
        expect(label, got, want);
    }
    Circuit::stop();
}

int main() {
    test_add();
    test_adder4();

    printf("\n%d/%d passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n");
    return failed ? 1 : 0;
}
