#include "bus.h"
#include "circuit.h"
#include "mem.h"
#include "wire.h"

#include <cstdio>
#include <cstdlib>

static int passed = 0, failed = 0;

static void expect(const char* label, uint64_t got, uint64_t want) {
    if (got == want) {
        printf("  pass  %-38s = 0x%llx\n", label, (unsigned long long)got);
        ++passed;
    } else {
        printf("  FAIL  %-38s = 0x%llx  (expected 0x%llx)\n",
               label, (unsigned long long)got, (unsigned long long)want);
        ++failed;
    }
}

static void test_mem() {
    printf("MEM (D-latch):\n");
    Wire i, s, o;
    MEM mem(i, s, o);
    Circuit::start();

    // Power-on state: wc_ starts HIGH, so o should settle to 0
    s.set(0); i.set(0);
    o.wait_for(0);
    expect("power-on stores 0", o.last(), 0);

    // Write 1: enable, drive input, wait for output, disable
    i.set(1); s.set(1);
    o.wait_for(1);
    expect("write 1 (s=1, i=1)", o.last(), 1);
    s.set(0);
    // wait_for after s=0: drains any glitch+recovery bytes from the s→0
    // transition (internal wc may not have settled when the write wait_for
    // returned) and confirms the circuit is truly stable in hold mode.
    o.wait_for(1);
    expect("latch holds 1 after s=0", o.last(), 1);

    // Input change with s=0 must not affect output (circuit is now stable)
    i.set(0);
    o.get();  // drain any bytes — should be empty since hold mode blocks writes
    expect("i=0 with s=0 does not change o", o.last(), 1);

    // Write 0
    s.set(1);
    o.wait_for(0);
    expect("write 0 (s=1, i=0)", o.last(), 0);
    s.set(0);
    o.wait_for(0);
    expect("latch holds 0 after s=0", o.last(), 0);

    // Write 1 again to confirm reuse
    i.set(1); s.set(1);
    o.wait_for(1);
    expect("write 1 again", o.last(), 1);
    s.set(0);
    o.wait_for(1);
    expect("latch holds 1 again", o.last(), 1);

    Circuit::stop();
}

static void test_memer4() {
    printf("MEMer<4>:\n");
    Bus<4> i, o;
    Wire s;
    MEMer<4> mem(i, s, o);
    Circuit::start();

    // Power-on: all bits should be 0
    s.set(0); i.set(0);
    o.wait_for(0);
    expect("power-on stores 0b0000", o.last(), 0b0000);

    struct { uint64_t val; const char* label; } cases[] = {
        {0b1010, "write 0b1010"},
        {0b0101, "write 0b0101"},
        {0b1111, "write 0b1111"},
        {0b0000, "write 0b0000"},
        {0b1100, "write 0b1100"},
    };

    for (auto& c : cases) {
        i.set(c.val); s.set(1);
        o.wait_for(c.val);
        char label[64];
        snprintf(label, sizeof(label), "%s → stored", c.label);
        expect(label, o.last(), c.val);
        s.set(0);
        o.wait_for(c.val);  // settle hold mode before asserting
        snprintf(label, sizeof(label), "%s → latched", c.label);
        expect(label, o.last(), c.val);
    }

    // Hold-mode isolation is verified at the single-bit level in test_mem.

    Circuit::stop();
}

int main() {
    test_mem();
    test_memer4();

    printf("\n%d/%d passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n");
    return failed ? 1 : 0;
}
