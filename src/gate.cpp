#include "gate.h"
#include "wire.h"
#include "circuit.h"
#include "util.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
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
void Gate::run() {
    int epfd = epoll_create1(0);
    if (epfd < 0) _exit(1);

    int n = (int)input_fds_.size();
    std::vector<uint8_t> inputs(n, 0);

    for (int i = 0; i < n; i++) {
        struct epoll_event ev{};
        ev.events   = EPOLLIN;
        ev.data.u32 = (uint32_t)i;
        epoll_ctl(epfd, EPOLL_CTL_ADD, input_fds_[i], &ev);
    }

    // 0xFF forces a write on the very first evaluation
    uint8_t last_out = 0xFF;

    struct epoll_event events[16];
    for (;;) {
        int ready = epoll_wait(epfd, events, 16, -1);
        if (ready < 0) break;

        for (int i = 0; i < ready; i++) {
            uint8_t v;
            if (read(input_fds_[events[i].data.u32], &v, 1) <= 0)
                goto done;
            inputs[events[i].data.u32] = v;
        }

        uint8_t out = evaluate(inputs);
        if (out != last_out) {
            write(output_fd_, &out, 1);
            last_out = out;
        }
    }
done:
    close(epfd);
}
