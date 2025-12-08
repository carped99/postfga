extern "C"
{
#include <postgres.h>

#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
// #include <storage/lwlock.h>
// #include <storage/proc.h>
#include <utils/guc.h>
}

#include <optional>

#include "config/load.hpp"
#include "processor.hpp"
#include "state.h"
#include "worker.hpp"

/* signal flags */
static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested   = 0;

static void
bgw_sigterm_handler(SIGNAL_ARGS)
{
    int save_errno = errno;

    /* suppress unused parameter warning */
    (void) postgres_signal_arg;

    shutdown_requested = 1;

    if (MyLatch)
        SetLatch(MyLatch);

    errno = save_errno;
}

/* SIGHUP handler: request config reload */
static void
bgw_sighup_handler(SIGNAL_ARGS)
{
    int save_errno = errno;

    /* suppress unused parameter warning */
    (void) postgres_signal_arg;

    reload_requested = 1;

    if (MyLatch)
        SetLatch(MyLatch);

    errno = save_errno;
}

namespace postfga::bgw
{
    Worker::Worker(FgaState* state)
        : state_(state)
    {
        Assert(state_ != nullptr);
    }

    void Worker::run()
    {
        /* register latch in shared state */
        LWLockAcquire(state_->lock, LW_EXCLUSIVE);
        state_->bgw_latch = MyLatch;
        LWLockRelease(state_->lock);

        /* enter main loop */
        try
        {
            process();
        }
        catch (const std::exception& ex)
        {
            ereport(ERROR, (errmsg("postfga: bgw exception: %s", ex.what())));
        }
        catch (...)
        {
            ereport(ERROR, (errmsg("postfga: bgw unknown exception")));
        }        

        /* unregister latch on exit */
        LWLockAcquire(state_->lock, LW_EXCLUSIVE);
        state_->bgw_latch = NULL;
        LWLockRelease(state_->lock);
    }

    void Worker::process()
    {
        auto config = postfga::load_config_from_guc();

        std::optional<Processor> processor;
        processor.emplace(state_->channel, config);

        while (!shutdown_requested)
        {
            // wait for work or signal
            int rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_EXIT_ON_PM_DEATH, -1, PG_WAIT_EXTENSION);

            ResetLatch(MyLatch);

            CHECK_FOR_INTERRUPTS();

            if (!(rc & WL_LATCH_SET))
                continue;

            // handle config reload
            if (reload_requested)
            {
                reload_requested = false;
                ProcessConfigFile(PGC_SIGHUP);

                auto new_config = postfga::load_config_from_guc();
                // 실제로 변경된 경우만 재생성
                // if (new_config != config)  // operator!= 구현 필요

                processor.emplace(state_->channel, new_config);
            }

            processor->execute();
        }
    }
} // namespace postfga::bgw

extern "C" PGDLLEXPORT void
postfga_bgw_work(Datum arg)
{
    FgaState *state = fga_get_state();

    (void) arg;

    pqsignal(SIGTERM, bgw_sigterm_handler);
    pqsignal(SIGHUP, bgw_sighup_handler);

    // allow signals to be delivered
    BackgroundWorkerUnblockSignals();

    PG_TRY();
    {
        ereport(DEBUG1, (errmsg("postfga: bgw starting")));

        postfga::bgw::Worker worker(state);
        worker.run();

        ereport(DEBUG1, (errmsg("postfga: bgw finished")));
    }
    PG_CATCH();
    {
        EmitErrorReport();
        FlushErrorState();

        proc_exit(1);
    }
    PG_END_TRY();
    
    proc_exit(0);
}