#pragma once
#include <array>
#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <sys/types.h>

class Wire {
public:
    explicit Wire(std::string name = "");
    ~Wire();
    Wire(const Wire&) = delete;
    Wire& operator=(const Wire&) = delete;

    // Called by Gate constructors during topology setup
    int request_listener_fd();  // returns read fd for the listening gate
    int request_driver_fd();    // returns write fd for the driving gate

    // Parent/REPL interface
    void    set(uint8_t val);
    uint8_t get();               // non-blocking drain; returns last byte seen
    uint8_t last() const { return cached_val_; }  // cached value, no pipe read
    void    wait_for(uint8_t target);

    const std::string& name() const { return name_; }
    pid_t              pid()  const { return pid_; }

private:
    friend class Circuit;

    void start();
    void run();

    std::set<int> process_fds() const;  // fds the wire process needs to keep
    std::set<int> parent_fds()  const;  // fds the parent needs from this wire
    void          parent_cleanup(const std::set<int>& parent_keep);

    std::string name_;

    // Gate or parent writes to [1]; wire process reads from [0]
    int  driver_pipe_[2]   = {-1, -1};
    bool has_gate_driver_  = false;

    // Wire process writes to [i][1]; listening gate reads from [i][0]
    std::vector<std::array<int, 2>> listener_pipes_;

    // Wire process writes to [1] on change; parent reads from [0]
    int tap_pipe_[2] = {-1, -1};

    pid_t   pid_        = -1;
    uint8_t cached_val_ =  0;
};
