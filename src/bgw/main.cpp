/*
 * main.cpp
 *
 * Background worker entrypoints for the PostFGA extension.
 *
 * This module provides:
 *   - C-visible initialization hook to register the background worker
 *     (postfga_bgw_init)
 *   - Optional shutdown hook (postfga_bgw_fini)
 *   - Background worker main function (postfga_bgw_main), which:
 *       - Attaches to the PostFGA shared memory state
 *       - Wraps the C++ worker (postfga::bgw::Worker) in a PG_TRY/PG_CATCH
 *         block so that C++ exceptions are converted into PostgreSQL errors
 *       - Ensures clean process exit via proc_exit()
 *
 * All PostgreSQL-facing symbols are declared extern "C" for C linkage, while
 * the actual worker logic is implemented in C++ (worker.hpp / Worker::run()).
 */

extern "C" {
#include <postgres.h>
#include <storage/ipc.h>
#include <postmaster/bgworker.h>
#include "shmem.h"
}

#include <string.h>
#include <exception>
#include "worker.hpp"

extern "C" void
postfga_bgw_init(void)
{
    BackgroundWorker worker{};

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 1;
    worker.bgw_notify_pid = 0;
    worker.bgw_main_arg = (Datum) 0;

    strlcpy(worker.bgw_library_name, "postfga", BGW_MAXLEN);
    strlcpy(worker.bgw_function_name, "postfga_bgw_main", BGW_MAXLEN);
    strlcpy(worker.bgw_name, "postfga_bgw", BGW_MAXLEN);
    strlcpy(worker.bgw_type, "postfga_worker", BGW_MAXLEN);

    RegisterBackgroundWorker(&worker);
}

extern "C" void
postfga_bgw_fini(void)
{
    // noop
}

extern "C" PGDLLEXPORT void
postfga_bgw_main(Datum arg)
{
    (void)arg;
    
    PG_TRY();
    {
        PostfgaShmemState *state = postfga_get_shared_state();
        if (state == nullptr)
            ereport(ERROR, (errmsg("postfga bgw: shared memory state is not initialized")));

        try
        {
            postfga::bgw::Worker worker(state);
            ereport(DEBUG1, (errmsg("postfga bgw: running")));
            worker.run();
            ereport(DEBUG1, (errmsg("postfga bgw: finished")));
            proc_exit(0);
        }
        catch (const std::exception &ex)
        {
            ereport(ERROR, (errmsg("postfga bgw: exception: %s", ex.what())));
        }
        catch (...)
        {
            ereport(ERROR, (errmsg("postfga bgw: unknown exception")));
        }
    }
    PG_CATCH();
    {
        EmitErrorReport();
        FlushErrorState();
        proc_exit(1);
    }
    PG_END_TRY();
}