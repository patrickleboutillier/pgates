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
    c.wait_for(1);          // blocks until the signal propagates and settles
    printf("c = %d\n", c.last());  // safe: returns value confirmed by wait_for

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
uint8_t v = w.get();     // non-blocking drain: returns latest byte seen
uint8_t v = w.last();    // cached value — safe to call after wait_for, no pipe read
w.wait_for(1);           // blocks until wire reaches and settles on this value
```

`last()` vs `get()`: after `wait_for(v)` returns, `last()` returns `v` without
touching the pipe. `get()` drains any bytes that have arrived since — if the
circuit is still in-flight, those bytes may not equal `v`. Prefer `last()` in tests
and anywhere you read a value immediately after `wait_for`.

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

### `Bus<N>`

`Bus<N>` groups N wires and treats them as an N-bit integer (bit 0 = wire[0], MSB =
wire[N-1]). N must be in [1, 64].

```cpp
#include "bus.h"

Bus<8> a, b, out;

a.set(0b10110011);       // drive all N wires from an integer
uint64_t v = a.get();    // non-blocking drain across all wires
uint64_t v = a.last();   // cached value (safe after wait_for)
out.wait_for(0b10110011);// block until all N wires settle on the expected bits
Wire& w = a[i];          // access individual wire by index
```

### Bus gates

Bus gates apply a single-bit gate to each pair of wires across two buses.
Include `buses.h` (which includes `bus.h` and `gates.h`).

```cpp
#include "buses.h"

Bus<4> a, b, out;
ANDer<4> g(a, b, out);   // out[i] = a[i] & b[i]
```

| Class | Inputs | Output |
|-------|--------|--------|
| `ANDer<N>(a, b, out)` | 2 buses | `out[i] = a[i] & b[i]` |
| `ORer<N>(a, b, out)` | 2 buses | `out[i] = a[i] \| b[i]` |
| `NANDer<N>(a, b, out)` | 2 buses | `out[i] = !(a[i] & b[i])` |
| `NORer<N>(a, b, out)` | 2 buses | `out[i] = !(a[i] \| b[i])` |
| `XORer<N>(a, b, out)` | 2 buses | `out[i] = a[i] ^ b[i]` |
| `NOTer<N>(in, out)` | 1 bus | `out[i] = !in[i]` |

### `Circuit`

```cpp
Circuit::start();       // fork all wire and gate processes
Circuit::stop();        // SIGTERM all children, reap, clear registry
Circuit::run_repl();    // interactive monitor (see below)
```

`Circuit::stop()` clears the internal registry, so a second `start()`/`stop()` cycle
in the same process works cleanly.

## REPL / monitor

`Circuit::run_repl()` drops into an interactive session. Run `examples/repl` for a
preloaded NOR SR-latch:

```
make
./examples/repl
```

```
pgates> list             # show all named wires
pgates> get *            # read all wire values
pgates> set s 1          # drive wire 's' high
pgates> set s 0          # release
pgates> get q            # read wire 'q'
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

Runs:
- `tests/truth_tables` — all 6 gate types against their complete truth tables (22 cases)
- `tests/bus_ops` — ANDer, ORer, NOTer, XORer at 4-bit width (14 cases)
