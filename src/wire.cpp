#include "wire.h"
#include "circuit.h"
#include "util.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>

static void safe_close(int& fd) {
    if (fd >= 0) { close(fd); fd = -1; }
}

Wire::Wire(std::string name) : name_(std::move(name)) {
    if (pipe(driver_pipe_) < 0 || pipe(tap_pipe_) < 0)
        throw std::runtime_error(std::string("pipe: ") + strerror(errno));
    Circuit::register_wire(this);
}

Wire::~Wire() {
    safe_close(driver_pipe_[0]);
    safe_close(driver_pipe_[1]);
    safe_close(tap_pipe_[0]);
    safe_close(tap_pipe_[1]);
    for (auto& lp : listener_pipes_) {
        safe_close(lp[0]);
        safe_close(lp[1]);
    }
}

int Wire::request_listener_fd() {
    std::array<int, 2> lp;
    if (pipe(lp.data()) < 0)
        throw std::runtime_error(std::string("pipe: ") + strerror(errno));
    listener_pipes_.push_back(lp);
    return lp[0];  // read end goes to the gate
}

int Wire::request_driver_fd() {
    has_gate_driver_ = true;
    return driver_pipe_[1];  // write end goes to the gate
}

void Wire::set(uint8_t val) {
    if (has_gate_driver_)
        throw std::runtime_error("cannot set a gate-driven wire");
    write(driver_pipe_[1], &val, 1);
}

uint8_t Wire::get() {
    uint8_t tmp;
    while (read(tap_pipe_[0], &tmp, 1) > 0)
        cached_val_ = tmp;
    return cached_val_;
}

void Wire::wait_for(uint8_t target) {
    for (;;) {
        if (get() == target) return;  // pipe drained; last/cached value is target

        int flags = fcntl(tap_pipe_[0], F_GETFL);
        fcntl(tap_pipe_[0], F_SETFL, flags & ~O_NONBLOCK);
        uint8_t tmp;
        do {
            if (read(tap_pipe_[0], &tmp, 1) <= 0) {
                fcntl(tap_pipe_[0], F_SETFL, flags);
                return;
            }
            cached_val_ = tmp;
        } while (tmp != target);
        fcntl(tap_pipe_[0], F_SETFL, flags);
        // Saw target in blocking mode; loop back to drain any bytes that
        // arrived since (deferred glitch from a lagging gate evaluation),
        // confirming the circuit has fully settled before returning.
    }
}

std::set<int> Wire::process_fds() const {
    std::set<int> fds = {driver_pipe_[0], tap_pipe_[1]};
    for (const auto& lp : listener_pipes_) fds.insert(lp[1]);
    return fds;
}

std::set<int> Wire::parent_fds() const {
    std::set<int> fds = {tap_pipe_[0]};
    if (!has_gate_driver_) fds.insert(driver_pipe_[1]);
    return fds;
}

void Wire::parent_cleanup(const std::set<int>& parent_keep) {
    auto inv = [&](int& fd) { if (fd >= 0 && !parent_keep.count(fd)) fd = -1; };
    inv(driver_pipe_[0]); inv(driver_pipe_[1]);
    inv(tap_pipe_[0]);    inv(tap_pipe_[1]);
    for (auto& lp : listener_pipes_) { inv(lp[0]); inv(lp[1]); }
}

void Wire::start() {
    pid_ = fork();
    if (pid_ < 0)
        throw std::runtime_error(std::string("fork: ") + strerror(errno));
    if (pid_ == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        auto keep = process_fds();
        keep.insert({0, 1, 2});
        close_all_except(keep);
        run();
        _exit(0);
    }
}

// Wire process main loop: read one byte from driver, broadcast on change.
// Sends initial 0 to all listeners so gates evaluate at startup.
void Wire::run() {
    uint8_t val = 0;

    // Prime all connected gates with the initial LOW value
    for (const auto& lp : listener_pipes_)
        write(lp[1], &val, 1);

    for (;;) {
        uint8_t nv;
        if (read(driver_pipe_[0], &nv, 1) <= 0) break;
        if (nv == val) continue;
        val = nv;
        for (const auto& lp : listener_pipes_)
            write(lp[1], &val, 1);
        write(tap_pipe_[1], &val, 1);
    }
}
