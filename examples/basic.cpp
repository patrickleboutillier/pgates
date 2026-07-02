#include "circuit.h"
#include "gates.h"
#include "wire.h"
#include <cstdio>

int main() {
    // AND gate truth table
    Wire a("a"), b("b"), c("c");
    Wire ni("ni"), no("no");
    AND and_gate(a, b, c);
    NOT not_gate(ni, no);
    Circuit::start();

    printf("AND truth table:\n");
    // Reset both to 0 first so the wire processes have a known baseline
    a.set(0); b.set(0);
    c.wait_for(0);

    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j <= 1; j++) {
            a.set(i); b.set(j);
            c.wait_for((uint8_t)(i & j));
            printf("  %d AND %d = %d\n", i, j, c.get());
        }
    }

    printf("NOT gate:\n");
    // NOT(0)=1 should already be set from the initial wire broadcast
    no.wait_for(1);
    printf("  NOT 0 = %d\n", no.get());
    ni.set(1);
    no.wait_for(0);
    printf("  NOT 1 = %d\n", no.get());

    Circuit::stop();
    return 0;
}
