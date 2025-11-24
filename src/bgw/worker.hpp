#pragma once

extern "C" {
#include "shmem.h"
}

namespace postfga::bgw {

class Worker {
public:
    explicit Worker(PostfgaShmemState *state);
    void run();

private:
    void initialize();
    void process();

    PostfgaShmemState *state_ = nullptr;
};

} // namespace postfga::bgw