#include "gate.h"
#include "wire.h"
#include "circuit.h"
#include "util.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <unistd.h>

Gate::Gate(std::vector<Wire*> inputs, Wire* output) {
    for (Wire* w : inputs)
        input_fds_.push_back(w->request_listener_fd());
    output_fd_ = output->request_driver_fd();
    Circuit::register_gate(this);
}

void Gate::parent_cleanup() {
    for (int& fd : input_fds_) fd = -1;
    output_fd_ = -1;
}

void Gate::start() {
    pid_ = fork();
    if (pid_ < 0)
        throw std::runtime_error(std::string("fork: ") + strerror(errno));
    if (pid_ == 0) {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        std::set<int> keep(input_fds_.begin(), input_fds_.end());
        keep.insert(output_fd_);
        keep.insert({0, 1, 2});
        close_all_except(keep);
        run();
        _exit(0);
    }
}

// Gate process main loop: wake on any input change via epoll, re-evaluate,
// write to output wire only when the result changes.
//
// Evaluation is deferred until every input has been primed at least once.
// Without this, a gate in a feedback loop (e.g. MEM's NAND SR latch) may
// evaluate with stale 0s for inputs that haven't been primed yet, producing
// a spurious output that kicks off an unresolvable cascade at startup.
void Gate::run() {
    int epfd = epoll_create1(0);
    if (epfd < 0) _exit(1);

    int n = (int)input_fds_.size();
    std::vector<uint8_t> inputs(n, 0);
    int n_pending = n;  // decrements to 0 once every input has been seen once

    for (int i = 0; i < n; i++) {
        struct epoll_event ev{};
        ev.events   = EPOLLIN;
        ev.data.u32 = (uint32_t)i;
        epoll_ctl(epfd, EPOLL_CTL_ADD, input_fds_[i], &ev);
        // Non-blocking so we can drain each pipe fully in the evaluation loop.
        int fl = fcntl(input_fds_[i], F_GETFL);
        fcntl(input_fds_[i], F_SETFL, fl | O_NONBLOCK);
    }

    // 0xFF forces a write on the very first evaluation
    uint8_t last_out = 0xFF;

    std::vector<bool> received(n, false);
    struct epoll_event events[16];
    for (;;) {
        int ready = epoll_wait(epfd, events, 16, -1);
        if (ready < 0) break;

        // Drain ALL input pipes, not just the event-triggering ones.
        // This prevents a stale-input race: if two inputs update near-simultaneously
        // and only one triggers this epoll wakeup, we consume both before evaluating.
        bool eof = false;
        for (int i = 0; i < n; i++) {
            uint8_t v;
            ssize_t r;
            while ((r = read(input_fds_[i], &v, 1)) == 1) {
                if (!received[i]) { received[i] = true; --n_pending; }
                inputs[i] = v;
            }
            if (r == 0) { eof = true; break; }
            // r == -1: EAGAIN (pipe empty, normal) or EINTR (harmless)
        }
        if (eof) goto done;

        if (n_pending > 0) continue;

        uint8_t out = evaluate(inputs);
        if (out != last_out) {
            write(output_fd_, &out, 1);
            last_out = out;
        }
    }
done:
    close(epfd);
}
