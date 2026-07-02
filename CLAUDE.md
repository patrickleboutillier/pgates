# pgates — Claude context

## What this is

A logic gate simulator where every gate and every wire is its own OS process.
Inspired by the author's Go CPU simulator at github.com/patrickleboutillier/jcscpu.
Written in C++17, targets Linux (uses `/proc/self/fd` and `epoll`).

## Build

```
make          # builds libpgates.a + examples/basic + examples/repl + tests/truth_tables + tests/bus_ops
make test     # runs all tests
make clean
```

## Core design decisions (don't revisit without good reason)

**Two-phase model.** Topology is declared in C++ (constructors, stack objects), then
`Circuit::start()` forks everything. This avoids the old approach of sending control
messages to already-running child processes to add inputs dynamically.

**Wire-as-process.** Each `Wire` forks its own process. Its main loop reads one byte
from a single driver pipe, then broadcasts to N listener pipes (one per connected gate
input). This means gates write to their output wire and are completely unaware of
downstream consumers — exactly like a real wire.

**Gate-as-process.** Each `Gate` forks its own process. It uses `epoll` to block on
all its input pipes simultaneously, re-evaluates on any change, and writes to its
output wire only when the result changes.

**Tap pipe.** Every wire process also writes to a `tap_pipe` on every value change.
The parent holds the read end; `Wire::get()` (non-blocking drain), `Wire::last()`
(cached, no pipe read), and `Wire::wait_for(v)` (blocking) use this for the
REPL/test interface.

**`wait_for` stability.** After blocking until `target` is seen, `wait_for` loops
back and drains the pipe non-blocking, then re-enters blocking if a post-target byte
arrived. This repeats until the pipe is empty with `cached_val_ == target`, guarding
against deferred glitches from lagging gate evaluations. Tests use `wire.last()`
(not `wire.get()`) after `wait_for` to read the confirmed cached value without
opening a new race window.

**Wire initial value.** `Wire(name, initial_val)` lets a wire prime its gate listeners
with a value other than 0. MEM uses this to start internal wire `wc_` at 1, which
guarantees the latch powers on storing 0 (matching jcscpu behaviour).

**fd cleanup.** After all forks, `Circuit::start()` calls `close_all_except()` (which
iterates `/proc/self/fd`) to leave the parent with only tap read-ends and drive
write-ends for input wires. Each child process does the same immediately after fork,
keeping only the fds it actually owns.

**Circuit singleton** (`Circuit::wires_`, `Circuit::gates_`, `Circuit::wire_map_`)
auto-registers Wire/Gate objects at construction. `Circuit::stop()` kills children,
reaps them, then **clears the registry** so a second start/stop cycle works cleanly.

## File map

```
include/wire.h        Wire class (public API)
include/gate.h        Gate base class
include/gates.h       AND OR NOT NAND NOR XOR (single-wire gates)
include/bus.h         Bus<N> template (N-bit wire group, N ≤ 64)
include/buses.h       ANDer ORer NOTer NANDer NORer XORer (bus-level gates)
include/adder.h       ADD (full adder) + ADDer<N> (N-bit ripple-carry adder)
include/mem.h         MEM (gated D-latch) + MEMer<N> (N-bit memory register)
include/circuit.h     Circuit singleton (start/stop/run_repl)
src/util.h            close_all_except, internal only
src/wire.cpp
src/gate.cpp
src/gates.cpp
src/circuit.cpp
src/adder.cpp
src/mem.cpp
tests/truth_tables.cpp  truth-table tests for all 6 single-wire gates
tests/bus_ops.cpp       truth-table tests for bus gate types
tests/adder_ops.cpp     truth-table tests for ADD and ADDer<4>
tests/mem_ops.cpp       write/hold tests for MEM and MEMer<4>
examples/basic.cpp      AND + NOT demo
examples/repl.cpp       NOR SR-latch interactive REPL demo
```

## Known rough edges

- **Gate ownership for individual gates.** `test_2input()` in the test suite allocates
  gates with `new` (intentional leak) because a stack-allocated gate would be destroyed
  before `Circuit::start()`. Bus gates (`ANDer` etc.) avoid this by owning their gates
  via `std::unique_ptr`; bus gate objects must outlive `Circuit::stop()`. The right
  general fix is to have `Circuit` take ownership of gates, or introduce a `Simulation`
  RAII object.

- **Glitches are real.** Setting two inputs in sequence (not atomically) can produce a
  brief intermediate output. This is intentional (matches real circuit behaviour).
  `wait_for` handles it via the stability loop; always use `last()` rather than `get()`
  immediately after `wait_for` in tests.

## Process cleanup — current state and remaining gaps

**What's in place:**
- `prctl(PR_SET_PDEATHSIG, SIGTERM)` is called in every child (wire and gate) immediately
  after `fork()`, before `run()`. If the parent dies for any reason — crash, SIGKILL,
  `return` without `Circuit::stop()` — all children receive SIGTERM unconditionally.
  This is the primary safety net.
- The pipe cascade also provides cleanup: parent fds closing causes wire processes to
  get EOF, which propagates to gate processes, then to output wire processes.
- `Circuit::stop()` sends SIGTERM explicitly and reaps with `while (wait(...) > 0)`.

**Remaining gap — crashed child → zombie:**
If a child process crashes *before* `Circuit::stop()` is called, it becomes a zombie
until the parent reaps it. During a long REPL session this could accumulate.

Fix: add `signal(SIGCHLD, SIG_IGN)` in `Circuit::start()` (Linux-specific: tells the
kernel to auto-reap children). Consequence: `Circuit::stop()`'s `wait()` loop must
switch to `waitpid(pid, ..., 0)` per child, since a blanket `wait()` would return
ECHILD immediately.

**Remaining gap — `wait_for` hangs if the producing gate crashes:**
If the child process that would write to a wire's tap dies mid-simulation, any
`wait_for()` call on that wire blocks forever.

Fix: add an optional timeout parameter:
```cpp
bool Wire::wait_for(uint8_t target, int timeout_ms = -1);
// returns false on timeout
```
Implement with `poll()` on `tap_pipe_[0]` with a `timeval`.

## Next steps (in rough priority order)

### 1. Assembly / module pattern

A class that owns its internal gates and exposes `Wire&` members as ports:

```cpp
class HalfAdder {
public:
    Wire &a, &b;      // inputs  — references into internal wires
    Wire &sum, &carry;
    HalfAdder();
private:
    Wire sum_, carry_;
    XOR xor_g;
    AND and_g;
};
```

The challenge: `Wire&` members must bind at construction time, before
`Circuit::start()`. Initialiser-list ordering and self-referential connections
(feedback paths) need care.

### 2. Feedback / SR latch

The first real test of the wire-as-process model under a feedback loop.
Two cross-coupled NOR gates share output wires as each other's inputs.
The circuit should settle to a stable state; verify with `wait_for` after setting S or R.
The REPL demo in `examples/repl.cpp` sets this up interactively.

```cpp
Wire s("s"), r("r"), q("q"), nq("nq");
NOR g1(r, nq, q);   // Q  = NOR(R, /Q)
NOR g2(s,  q, nq);  // /Q = NOR(S,  Q)
Circuit::start();
s.set(1); s.set(0);  // set latch (S=1 forces Q=1)
q.wait_for(1);
```

### 3. Fix gate lifetime / ownership

Options:
- Have `Circuit` hold `std::unique_ptr<Gate>` and expose a factory:
  `Circuit::add<AND>(a, b, c)`
- Or wrap everything in a `Simulation` RAII object that owns all entities.

Either way, stack-allocated gates in examples should keep working.

### 4. More standard components

`ADD` (full adder) and `ADDer<N>` (N-bit ripple-carry) are in place. Next: D flip-flop,
8-bit register. These mirror the progression in jcscpu.

### 5. Propagation timing / tracing (optional)

Add timestamps (CLOCK_MONOTONIC) to tap writes so the REPL can show how long signals
took to propagate through a circuit. Useful for comparing gate-count depth.
