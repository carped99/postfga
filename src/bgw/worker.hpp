#pragma once

#include "state.h"

namespace postfga::bgw {

class Worker {
public:
    explicit Worker(PostfgaShmemState *state);
    void run();

private:
    void initialize();
    void process();
    void handle_sighup();
    void handle_sigterm();

    PostfgaShmemState *state_ = nullptr;
};

} // namespace postfga::bgw