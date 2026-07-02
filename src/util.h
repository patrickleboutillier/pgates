#pragma once
#include <cstdlib>
#include <dirent.h>
#include <set>
#include <unistd.h>
#include <vector>

// Close every open fd not in 'keep'. Uses /proc/self/fd on Linux.
static void close_all_except(const std::set<int>& keep) {
    DIR* dir = opendir("/proc/self/fd");
    if (!dir) {
        for (int fd = 3; fd < 4096; fd++)
            if (!keep.count(fd)) close(fd);
        return;
    }
    std::vector<int> to_close;
    int dfd = dirfd(dir);
    struct dirent* e;
    while ((e = readdir(dir))) {
        if (e->d_name[0] == '.') continue;
        int fd = atoi(e->d_name);
        if (fd != dfd && !keep.count(fd))
            to_close.push_back(fd);
    }
    closedir(dir);
    for (int fd : to_close) close(fd);
}

