#pragma once
#include <vector>
#include <cstdint>
#include <sys/types.h>

class Wire;

class Gate {
public:
    virtual ~Gate() = default;
    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;

    pid_t pid() const { return pid_; }

protected:
    Gate(std::vector<Wire*> inputs, Wire* output);
    virtual uint8_t evaluate(const std::vector<uint8_t>& inputs) const = 0;

private:
    friend class Circuit;

    void start();
    void run();
    void parent_cleanup();

    std::vector<int> input_fds_;
    int              output_fd_ = -1;
    pid_t            pid_       = -1;
};
