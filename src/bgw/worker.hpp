// bgw_worker.hpp
#pragma once

extern "C" {
#include <postgres.h>
#include <storage/latch.h>
#include <utils/guc.h>
#include <postmaster/bgworker.h>
}

#include "state.h"       // get_shared_state()
#include "processor.hpp"

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