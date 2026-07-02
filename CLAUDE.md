# pgates — Claude context

## What this is

A logic gate simulator where every gate and every wire is its own OS process.
Inspired by the author's Go CPU simulator at github.com/patrickleboutillier/jcscpu.
Written in C++17, targets Linux (uses `/proc/self/fd` and `epoll`).

## Build

```
make          # builds libpgates.a + examples/basic + tests/truth_tables
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
The parent holds the read end; `Wire::get()` (non-blocking drain) and
`Wire::wait_for(v)` (blocking) use this for the REPL/test interface.

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
include/gates.h       AND OR NOT NAND NOR XOR
include/circuit.h     Circuit singleton (start/stop/run_repl)
src/util.h            close_all_except, internal only
src/wire.cpp
src/gate.cpp
src/gates.cpp
src/circuit.cpp
tests/truth_tables.cpp  truth-table tests for all 6 gates
examples/basic.cpp      AND + NOT demo
```

## Known rough edges

- **Gate ownership in tests.** `test_2input()` in the test suite allocates gates with
  `new` (intentional leak) because a stack-allocated gate would be destroyed before
  `Circuit::start()`. The right fix is to have `Circuit` take ownership of gates, or
  introduce a `Simulation` object with value semantics.

- **No assembly/module type yet.** The next major feature.

- **`Circuit::run_repl()` is written but untested interactively.** Should work; test it
  with a simple SR-latch or adder circuit.

- **Glitches are real.** Setting two inputs in sequence (not atomically) can produce a
  brief intermediate output. This is intentional (matches real circuit behaviour) but
  `wait_for` handles it correctly by draining the tap to the final settled value.

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
Implement with `select()` or `poll()` on `tap_pipe_[0]` with a `timeval`.

## Next steps (in rough priority order)

### 1. Assembly / module pattern

A class that owns its internal gates and exposes `Wire&` members as ports:

```cpp
class HalfAdder {
public:
    Wire &a, &b;      // inputs  — references into internal gates
    Wire &sum, &carry;
    HalfAdder();
private:
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

```cpp
Wire s("s"), r("r"), q("q"), nq("nq");
NOR g1(s, nq, q);   // Q  = NOR(S, /Q)
NOR g2(r,  q, nq);  // /Q = NOR(R,  Q)
Circuit::start();
s.set(1); s.set(0);  // set latch
q.wait_for(1);
```

### 3. Fix gate lifetime / ownership

Options:
- Have `Circuit` hold `std::unique_ptr<Gate>` and expose a factory:
  `Circuit::add<AND>(a, b, c)`
- Or wrap everything in a `Simulation` RAII object that owns all entities.

Either way, stack-allocated gates in examples should keep working.

### 4. REPL smoke test

Run an interactive REPL on a simple circuit (e.g. AND gate) and exercise all commands:
`set`, `get *`, `watch`, `list`, `quit`.

### 5. More standard components

Once assemblies work: HalfAdder, FullAdder, 4-bit ripple-carry adder, D flip-flop,
8-bit register. These mirror the progression in jcscpu.

### 6. Propagation timing / tracing (optional)

Add timestamps (CLOCK_MONOTONIC) to tap writes so the REPL can show how long signals
took to propagate through a circuit. Useful for comparing gate-count depth.
