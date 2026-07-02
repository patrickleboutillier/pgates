#include "circuit.h"
#include "gate.h"
#include "wire.h"
#include "util.h"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

std::vector<Wire*>           Circuit::wires_;
std::vector<Gate*>           Circuit::gates_;
std::map<std::string, Wire*> Circuit::wire_map_;

void Circuit::register_wire(Wire* w) {
    wires_.push_back(w);
    if (!w->name().empty())
        wire_map_[w->name()] = w;
}

void Circuit::register_gate(Gate* g) {
    gates_.push_back(g);
}

void Circuit::start() {
    // Gates first so their epoll loops are ready before wires broadcast
    for (Gate* g : gates_) g->start();
    for (Wire* w : wires_) w->start();

    // Parent only keeps tap read ends and drive write ends for input wires
    std::set<int> parent_keep = {0, 1, 2};
    for (Wire* w : wires_) {
        auto pfds = w->parent_fds();
        parent_keep.insert(pfds.begin(), pfds.end());
    }
    close_all_except(parent_keep);

    // Null out any fds the parent no longer holds so destructors don't double-close
    for (Wire* w : wires_) w->parent_cleanup(parent_keep);
    for (Gate* g : gates_) g->parent_cleanup();

    // Make tap pipes non-blocking so Wire::get() can drain without stalling
    for (Wire* w : wires_) {
        if (w->tap_pipe_[0] >= 0) {
            int fl = fcntl(w->tap_pipe_[0], F_GETFL);
            fcntl(w->tap_pipe_[0], F_SETFL, fl | O_NONBLOCK);
        }
    }
}

void Circuit::stop() {
    for (Wire* w : wires_) if (w->pid_ > 0) kill(w->pid_, SIGTERM);
    for (Gate* g : gates_) if (g->pid_ > 0) kill(g->pid_, SIGTERM);
    int st;
    while (wait(&st) > 0) {}
    // Clear the registry so the next Circuit::start() starts fresh
    wires_.clear();
    gates_.clear();
    wire_map_.clear();
}

// ── REPL ──────────────────────────────────────────────────────────────────────

static volatile sig_atomic_t g_stop_watch = 0;
static void on_sigint(int) { g_stop_watch = 1; }

void Circuit::run_repl() {
    auto find_wire = [&](const char* name) -> Wire* {
        auto it = wire_map_.find(name);
        return it != wire_map_.end() ? it->second : nullptr;
    };
    puts("pgates — type 'help' for commands");
    char line[256];

    while (true) {
        printf("pgates> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;

        // Strip newline
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char cmd[64] = {}, arg1[64] = {};
        int  val = -1;
        int  n   = sscanf(line, "%63s %63s %d", cmd, arg1, &val);

        if (!cmd[0] || cmd[0] == '#') continue;

        if (!strcmp(cmd, "quit") || !strcmp(cmd, "q")) {
            break;

        } else if (!strcmp(cmd, "set") && n >= 3) {
            Wire* w = find_wire(arg1);
            if (!w) { printf("unknown wire: %s\n", arg1); continue; }
            w->set((uint8_t)val);

        } else if (!strcmp(cmd, "get") && n >= 2) {
            if (!strcmp(arg1, "*")) {
                for (Wire* w : wires_)
                    printf("  %-12s = %d\n", w->name().c_str(), w->get());
            } else {
                Wire* w = find_wire(arg1);
                if (!w) { printf("unknown wire: %s\n", arg1); continue; }
                printf("%s = %d\n", arg1, w->get());
            }

        } else if (!strcmp(cmd, "list")) {
            for (Wire* w : wires_)
                printf("  %s\n", w->name().c_str());

        } else if (!strcmp(cmd, "watch")) {
            // Monitor all tap pipes; print every value change until Ctrl+C
            struct sigaction sa{}, old{};
            sa.sa_handler = on_sigint;
            sigaction(SIGINT, &sa, &old);
            g_stop_watch = 0;

            // Switch taps to blocking for select()
            for (Wire* w : wires_) {
                if (w->tap_pipe_[0] < 0) continue;
                int fl = fcntl(w->tap_pipe_[0], F_GETFL);
                fcntl(w->tap_pipe_[0], F_SETFL, fl & ~O_NONBLOCK);
            }

            puts("watching... (Ctrl+C to stop)");
            while (!g_stop_watch) {
                fd_set rfds;
                FD_ZERO(&rfds);
                int maxfd = 0;
                for (Wire* w : wires_) {
                    if (w->tap_pipe_[0] < 0) continue;
                    FD_SET(w->tap_pipe_[0], &rfds);
                    if (w->tap_pipe_[0] > maxfd) maxfd = w->tap_pipe_[0];
                }
                struct timeval tv = {0, 100000};  // 100 ms
                int r = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
                if (r < 0) break;
                for (Wire* w : wires_) {
                    if (w->tap_pipe_[0] < 0) continue;
                    if (!FD_ISSET(w->tap_pipe_[0], &rfds)) continue;
                    uint8_t v;
                    if (read(w->tap_pipe_[0], &v, 1) > 0) {
                        w->cached_val_ = v;
                        printf("  %s -> %d\n", w->name().c_str(), v);
                        fflush(stdout);
                    }
                }
            }
            puts("(stopped)");

            // Restore non-blocking
            for (Wire* w : wires_) {
                if (w->tap_pipe_[0] < 0) continue;
                int fl = fcntl(w->tap_pipe_[0], F_GETFL);
                fcntl(w->tap_pipe_[0], F_SETFL, fl | O_NONBLOCK);
            }
            sigaction(SIGINT, &old, nullptr);

        } else if (!strcmp(cmd, "help")) {
            puts("  set <wire> <0|1>   drive an input wire");
            puts("  get <wire|*>       read current value(s)");
            puts("  list               list all wires");
            puts("  watch              monitor all changes (Ctrl+C to stop)");
            puts("  quit               exit");
        } else {
            printf("unknown command '%s' (try: help)\n", cmd);
        }
    }
}
