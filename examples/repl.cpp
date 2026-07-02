#include "circuit.h"
#include "gates.h"
#include "wire.h"
#include <cstdio>

int main() {
    Wire s("s"), r("r"), q("q"), nq("nq");
    // Standard NOR SR-latch: Q = NOR(R, /Q), /Q = NOR(S, Q)
    // S=1 → SET (q=1, nq=0);  R=1 → RESET (q=0, nq=1)
    NOR g1(r, nq, q);
    NOR g2(s,  q, nq);
    Circuit::start();

    puts("NOR SR-latch  (s, r = inputs  |  q, nq = outputs)");
    puts("Latch powers on to a random stable state.");
    puts("");
    puts("  set s 1 / set s 0   SET   q→1, nq→0");
    puts("  set r 1 / set r 0   RESET q→0, nq→1");
    puts("  get *               read all four wires");
    puts("  watch               stream every change  (Ctrl+C to stop)");
    puts("  quit                exit");
    puts("");

    Circuit::run_repl();
    Circuit::stop();
    return 0;
}
