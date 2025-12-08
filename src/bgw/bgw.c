#include <postgres.h>

#include <postmaster/bgworker.h>

#include "bgw.h"

void fga_bgw_init(void)
{
    BackgroundWorker worker;

    MemSet(&worker, 0, sizeof(worker));

    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 1;
    worker.bgw_notify_pid = 0;
    worker.bgw_main_arg = (Datum)0;

    strlcpy(worker.bgw_library_name, "postfga", MAXPGPATH);
    strlcpy(worker.bgw_function_name, "postfga_bgw_work", BGW_MAXLEN); // worker.cpp 에 구현
    strlcpy(worker.bgw_name, "postfga_bgw", BGW_MAXLEN);
    strlcpy(worker.bgw_type, "postfga_worker", BGW_MAXLEN);

    RegisterBackgroundWorker(&worker);
}

void fga_bgw_fini(void)
{
    // noop
}