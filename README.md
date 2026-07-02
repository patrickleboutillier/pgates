# pgates

A logic gate simulator where every gate and every wire is its own OS process.

Signal propagation is purely event-driven — no clock. When an input changes, the
wire process broadcasts the new value to all connected gates. Each gate re-evaluates
and writes to its output wire only if the result changed. A gate has no idea who
receives its output; the wire handles fan-out, just like a real wire.

Inspired by [jcscpu](https://github.com/patrickleboutillier/jcscpu), a CPU built
from first principles.

## Requirements

- Linux (uses `epoll` and `/proc/self/fd`)
- g++ with C++17

## Build

```
make          # library + examples + tests
make test     # run the test suite
make clean
```

## Usage

```cpp
#include "circuit.h"
#include "gates.h"
#include "wire.h"

int main() {
    // 1. Declare wires (named for the REPL)
    Wire a("a"), b("b"), c("c");

    // 2. Declare gates, connecting them to wires
    AND g(a, b, c);

    // 3. Fork all processes and close unneeded fds in the parent
    Circuit::start();

    // 4. Drive inputs and observe outputs
    a.set(1); b.set(1);
    c.wait_for(1);          // blocks until the signal propagates
    printf("c = %d\n", c.get());

    // 5. Optional interactive monitor
    // Circuit::run_repl();

    Circuit::stop();
    return 0;
}
```

Compile against the library:

```
g++ -std=c++17 -Iinclude myprog.cpp libpgates.a -o myprog
```

## API

### `Wire`

```cpp
Wire w("name");          // named wire (name used by REPL)
Wire w;                  // anonymous wire

w.set(0);                // drive an input wire (no gate driver allowed)
uint8_t v = w.get();     // non-blocking: returns latest known value
w.wait_for(1);           // blocks until wire reaches this value
```

### Gates

All gates take wire references. Every gate is a separate OS process after
`Circuit::start()`.

| Class | Inputs | Output |
|-------|--------|--------|
| `AND(a, b, out)` | 2 | `a & b` |
| `OR(a, b, out)` | 2 | `a \| b` |
| `NOT(in, out)` | 1 | `!in` |
| `NAND(a, b, out)` | 2 | `!(a & b)` |
| `NOR(a, b, out)` | 2 | `!(a \| b)` |
| `XOR(a, b, out)` | 2 | `a ^ b` |

### `Circuit`

```cpp
Circuit::start();       // fork all wire and gate processes
Circuit::stop();        // SIGTERM all children, reap, clear registry
Circuit::run_repl();    // interactive monitor (see below)
```

`Circuit::stop()` clears the internal registry, so a second `start()`/`stop()` cycle
in the same process works cleanly.

## REPL / monitor

`Circuit::run_repl()` drops into an interactive session:

```
pgates> list             # show all named wires
pgates> get *            # read all wire values
pgates> set a 1          # drive wire 'a' high
pgates> get c            # read wire 'c'
pgates> watch            # print every value change (Ctrl+C to stop)
pgates> quit
```

## How it works

```
Parent process (REPL / test driver)
  │
  │  wire.set(v) ──► driver_pipe[1]
  │
  ▼
Wire process
  reads driver_pipe[0]
  on change → broadcasts to all listener_pipes
            → writes to tap_pipe[1]
  │
  │  listener_pipe[i]
  ▼
Gate process
  epoll on all input pipes
  re-evaluates on any change
  writes to output wire's driver_pipe ──► (next Wire process)

Parent ◄── tap_pipe[0] ── Wire process   (for get/wait_for/watch)
```

After `Circuit::start()`, the parent holds only:
- `tap_pipe[0]` for every wire (read-only monitoring)
- `driver_pipe[1]` for wires with no gate driver (circuit inputs)

All other file descriptors are closed in the parent.

## Fan-out

One wire can feed multiple gates with no changes to the driving gate. The wire
process holds one listener pipe per connected gate and writes to all of them on
every value change.

```cpp
Wire a("a"), b("b"), c("c"), d("d");
AND g1(a, b, c);
NOT g2(c, d);    // c fans out to g2 automatically
```

## Tests

```
make test
```

Runs `tests/truth_tables` which exercises all 6 gate types against their complete
truth tables (22 test cases).
