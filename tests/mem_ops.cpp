#include "bus.h"
#include "circuit.h"
#include "mem.h"
#include "wire.h"

#include <cstdio>
#include <csignal>
#include <unistd.h>

static int passed = 0, failed = 0;

static volatile const char* g_last_step = "(none)";
static void on_alarm(int) {
    fprintf(stderr, "\nTIMEOUT: last step was: %s\n", g_last_step);
    _exit(2);
}
#define STEP(s) do { g_last_step = (s); } while(0)

static void expect(const char* label, uint64_t got, uint64_t want) {
    if (got == want) {
        ++passed;
    } else {
        printf("  FAIL  %-38s = 0x%llx  (expected 0x%llx)\n",
               label, (unsigned long long)got, (unsigned long long)want);
        ++failed;
    }
}

static void test_mem() {
    Wire i, s, o;
    MEM mem(i, s, o);
    Circuit::start();

    s.set(0); i.set(0);
    o.wait_for(0);
    i.set(1); s.set(1);
    o.wait_for(1);
    mem.qbar().wait_for(0);
    s.set(0);
    o.wait_for(1);
    i.set(0);
    o.get();
    s.set(1);
    o.wait_for(0);
    mem.qbar().wait_for(1);
    s.set(0);
    o.wait_for(0);
    i.set(1); s.set(1);
    o.wait_for(1);
    mem.qbar().wait_for(0);
    s.set(0);
    o.wait_for(1);

    Circuit::stop();
    ++passed;  // test_mem passed if we get here
}

static void test_byte4() {
    printf("BYTE<4>:\n");
    Bus<4> i, o;
    Wire s;
    BYTE<4> mem(i, s, o);
    Circuit::start();

    s.set(0); i.set(0);
    STEP("power-on wait_for(0)");
    o.wait_for(0);
    expect("power-on", o.last(), 0);

    struct { uint64_t val; const char* label; } cases[] = {
        {0b1010, "write 0b1010"},
        {0b0101, "write 0b0101"},
        {0b1111, "write 0b1111"},
        {0b0000, "write 0b0000"},
        {0b1100, "write 0b1100"},
    };

    for (auto& c : cases) {
        char step[128];

        snprintf(step, sizeof(step), "%s: wait_for(0x%llx)", c.label, (unsigned long long)c.val);
        STEP(step);
        i.set(c.val); s.set(1);
        o.wait_for(c.val);
        expect(c.label, o.last(), c.val);

        snprintf(step, sizeof(step), "%s: settle", c.label);
        STEP(step);
        mem.settle(c.val);

        s.set(0);

        for (size_t k = 0; k < 4; ++k) {
            uint8_t tgt = (c.val >> k) & 1;
            snprintf(step, sizeof(step), "%s: hold bit%zu wait_for(%u)", c.label, k, tgt);
            STEP(step);
            o[k].wait_for(tgt);
        }
        char label[64];
        snprintf(label, sizeof(label), "%s \xe2\x86\x92 latched", c.label);
        expect(label, o.last(), c.val);
    }

    Circuit::stop();
}

int main() {
    signal(SIGALRM, on_alarm);
    alarm(5);
    test_mem();
    test_byte4();

    printf("%d/%d passed\n", passed, passed + failed);
    return failed ? 1 : 0;
}
