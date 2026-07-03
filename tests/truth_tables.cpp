#include "circuit.h"
#include "gates.h"
#include "wire.h"

#include <array>
#include <cstdio>
#include <functional>

// ── Result tracking ───────────────────────────────────────────────────────────

static int passed = 0, failed = 0;

static void expect(const char* label, uint8_t got, uint8_t want) {
    if (got == want) {
        printf("  pass  %-22s = %d\n", label, got);
        passed++;
    } else {
        printf("  FAIL  %-22s = %d  (expected %d)\n", label, got, want);
        failed++;
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

using Row2 = std::array<int, 3>;  // {a, b, expected_out}

// Run a full truth table for a 2-input gate.
// 'make' constructs the gate given (in_a, in_b, out).
static void test_2input(
    const char* name,
    std::function<void(Wire&, Wire&, Wire&)> make,
    std::initializer_list<Row2> table)
{
    printf("%s:\n", name);
    Wire a("a"), b("b"), q("q");
    make(a, b, q);
    Circuit::start();

    // Settle the gate at (0,0) before the loop so we start from a known state
    a.set(0); b.set(0);
    q.wait_for((uint8_t)table.begin()->at(2));

    for (const auto& r : table) {
        a.set(r[0]); b.set(r[1]);
        q.wait_for((uint8_t)r[2]);
        char label[32];
        snprintf(label, sizeof(label), "%s(%d,%d)", name, r[0], r[1]);
        expect(label, q.last(), (uint8_t)r[2]);
    }
    Circuit::stop();
}

static void test_not() {
    printf("NOT:\n");
    Wire in("in"), out("out");
    NOT g(in, out);
    Circuit::start();

    // Wire process primes NOT's input with 0; NOT(0)=1 propagates at startup
    out.wait_for(1);
    expect("NOT(0)", out.last(), 1);

    in.set(1);
    out.wait_for(0);
    expect("NOT(1)", out.last(), 0);

    Circuit::stop();
}

// ── Tests ─────────────────────────────────────────────────────────────────────

int main() {
    test_2input("AND",
        [](Wire& a, Wire& b, Wire& q){ new AND(a, b, q); },
        { {0,0,0}, {0,1,0}, {1,0,0}, {1,1,1} });

    test_2input("OR",
        [](Wire& a, Wire& b, Wire& q){ new OR(a, b, q); },
        { {0,0,0}, {0,1,1}, {1,0,1}, {1,1,1} });

    test_not();

    test_2input("NAND",
        [](Wire& a, Wire& b, Wire& q){ new NAND(a, b, q); },
        { {0,0,1}, {0,1,1}, {1,0,1}, {1,1,0} });

    test_2input("NOR",
        [](Wire& a, Wire& b, Wire& q){ new NOR(a, b, q); },
        { {0,0,1}, {0,1,0}, {1,0,0}, {1,1,0} });

    test_2input("XOR",
        [](Wire& a, Wire& b, Wire& q){ new XOR(a, b, q); },
        { {0,0,0}, {0,1,1}, {1,0,1}, {1,1,0} });

    // ── Summary ───────────────────────────────────────────────────────────────
    printf("\n%d/%d passed", passed, passed + failed);
    if (failed) printf("  (%d FAILED)", failed);
    printf("\n");
    return failed ? 1 : 0;
}
