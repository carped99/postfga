extern "C"
{
#include <postgres.h>

#include <miscadmin.h>
#include <pgstat.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/lwlock.h>
#include <storage/proc.h>
#include <utils/guc.h>
}

#include <optional>

#include "config/load.hpp"
#include "processor.hpp"
#include "shmem.h"
#include "worker.hpp"

namespace
{
    // signal flags
    volatile sig_atomic_t shutdown_requested = false;
    volatile sig_atomic_t reload_requested = false;

    // SIGTERM handler: request shutdown
    void bgw_sigterm_handler(SIGNAL_ARGS)
    {
        int save_errno = errno;
        shutdown_requested = true;
        if (MyLatch)
            SetLatch(MyLatch);
        errno = save_errno;
    }

    // SIGHUP handler: request config reload
    void bgw_sighup_handler(SIGNAL_ARGS)
    {
        int save_errno = errno;
        reload_requested = true;
        if (MyLatch)
            SetLatch(MyLatch);
        errno = save_errno;
    }
} // anonymous namespace

namespace postfga::bgw
{
    Worker::Worker(FgaState* state)
        : state_(state)
    {
        Assert(state_ != nullptr);

        initialize();
    }

    void Worker::run()
    {
        /* register latch in shared state */
        LWLockAcquire(state_->lock, LW_EXCLUSIVE);
        state_->bgw_latch = MyLatch;
        LWLockRelease(state_->lock);

        /* enter main loop */
        process();

        /* unregister latch on exit */
        LWLockAcquire(state_->lock, LW_EXCLUSIVE);
        state_->bgw_latch = NULL;
        LWLockRelease(state_->lock);
    }

    void Worker::initialize()
    {
        // install signal handlers
        pqsignal(SIGTERM, bgw_sigterm_handler);
        pqsignal(SIGHUP, bgw_sighup_handler);

        // allow signals to be delivered
        BackgroundWorkerUnblockSignals();

        /* optional: connect to database
         * BackgroundWorkerInitializeConnection(NULL, NULL, 0);
         */
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

            // latch reset
            ResetLatch(MyLatch);

            CHECK_FOR_INTERRUPTS();

            // handle config reload
            if (reload_requested)
            {
                reload_requested = false;
                ProcessConfigFile(PGC_SIGHUP);
                auto config = postfga::load_config_from_guc();
                ereport(LOG,
                        (errmsg("postfga: reloaded configuration, %s, %s",
                                config.endpoint.c_str(),
                                config.store_id.c_str())));
                processor.emplace(state_->channel, config);
            }

            pgstat_report_activity(STATE_RUNNING, "postfga: processing requests");
            processor->execute();
            pgstat_report_activity(STATE_IDLE, NULL);
        }

        // report activity: stopped
        pgstat_report_activity(STATE_IDLE, "postfga: stopped");
    }

} // namespace postfga::bgw
