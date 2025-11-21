#include <postgres.h>
#include <storage/ipc.h>
#include <postmaster/bgworker.h>
#include <string.h>

#include "worker.hpp"
#include "shmem.h"

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

extern "C" void
postfga_bgw_main(Datum arg)
{
    (void)arg;
    PG_TRY();
    {
        postfga::bgw::Worker worker(postfga_get_shared_state());
        ereport(DEBUG1, (errmsg("postfga background worker running")));
        worker.run();
        ereport(DEBUG1, (errmsg("postfga background worker finished")));
        proc_exit(0);
    }
    PG_CATCH();
    {
        /* 로그 출력 후 종료 */
        EmitErrorReport();
        FlushErrorState();
        proc_exit(1);
    }
    PG_END_TRY();
}