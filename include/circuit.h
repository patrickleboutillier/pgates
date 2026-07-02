#pragma once
#include <map>
#include <string>
#include <vector>

class Wire;
class Gate;

class Circuit {
public:
    static void register_wire(Wire* w);
    static void register_gate(Gate* g);

    static void start();     // fork all processes; parent keeps tap/drive fds
    static void stop();      // SIGTERM all children and reap
    static void run_repl();  // interactive monitor (blocks until quit)

private:
    static std::vector<Wire*>        wires_;
    static std::vector<Gate*>        gates_;
    static std::map<std::string, Wire*> wire_map_;
};
