/*
 * Background Worker implementation for PostFGA
 *
 * BGW lifecycle: Register -> Initialize -> Main Loop -> Shutdown
 */

#include <postgres.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>
#include <storage/latch.h>
#include <storage/proc.h>
#include <utils/guc.h>
#include <pgstat.h>

#include "bgw.h"
#include "state.h"
#include "guc.h"

static volatile sig_atomic_t shutdown_requested = false;
static volatile sig_atomic_t config_reload_requested = false;

/*
 * bgw_sigterm_handler - Handle SIGTERM signal
 */
static void
bgw_sigterm_handler(SIGNAL_ARGS)
{
    int save_errno = errno;
    shutdown_requested = true;
    if (MyLatch)
        SetLatch(MyLatch);
    errno = save_errno;
}

/*
 * bgw_sighup_handler - Handle SIGHUP signal
 */
static void
bgw_sighup_handler(SIGNAL_ARGS)
{
    int save_errno = errno;
    config_reload_requested = true;
    if (MyLatch != NULL)
        SetLatch(MyLatch);
    errno = save_errno;
}

/*
 * postfga_register_bgw - Register background worker with PostgreSQL
 */
void
postfga_bgw_init(void)
{
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));

    /* Configuration */
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 10;
    worker.bgw_notify_pid = 0;
    worker.bgw_main_arg = (Datum) 0;

    /* Entry point */
    strlcpy(worker.bgw_library_name, "postfga", BGW_MAXLEN);
    strlcpy(worker.bgw_function_name, "postfga_bgw_main", BGW_MAXLEN);
    strlcpy(worker.bgw_name, "postfga_bgw", BGW_MAXLEN);
    strlcpy(worker.bgw_type, "postfga_worker", BGW_MAXLEN);

    RegisterBackgroundWorker(&worker);
    elog(DEBUG1, "PostFGA BGW: registered");
}

void
postfga_bgw_fini(void)
{
    // noop
}

/*
 * postfga_bgw_main - Background worker main loop
 */
void
postfga_bgw_main(Datum arg)
{
    (void) arg;

    ereport(DEBUG1, (errmsg("postfga background worker started")));

    /* Setup signal handlers */
    pqsignal(SIGTERM, bgw_sigterm_handler);
    pqsignal(SIGHUP, bgw_sighup_handler);
    BackgroundWorkerUnblockSignals();

    /* Connect to shared memory */
    // BackgroundWorkerInitializeConnection(NULL, NULL, 0);

    /* Register latch */
    PostfgaShmemState *state = get_shared_state();
    if (!state)
    {
        elog(LOG, "PostFGA BGW: no shared state found");
        proc_exit(1);
    }

    /* Register latch in shared state */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);
    state->bgw_latch = MyLatch;
    LWLockRelease(state->lock);

    /* Main loop */
    while (!shutdown_requested)
    {
        int rc = WaitLatch(MyLatch,
            WL_LATCH_SET | WL_EXIT_ON_PM_DEATH,
            -1,
            PG_WAIT_EXTENSION
        );

        ResetLatch(MyLatch);

        /* Postmaster death */
        if (rc & WL_POSTMASTER_DEATH)
            proc_exit(1);

        /* Reload configuration if needed */
        if (config_reload_requested)
        {
            config_reload_requested = false;
            // ProcessConfigFile(PGC_SIGHUP);
        }            

        elog(LOG, "PostFGA BGW: processing tasks");

        /* TODO: Process pending FDW OpenFGA requests */
        /* TODO: handle queued ACL batch requests */
        /* TODO: monitor and invalidate shared-memory cache */
        /* TODO: perform batch-grpc to OpenFGA server */

        pgstat_report_activity(STATE_IDLE, NULL);
    }

    /* Cleanup */
    LWLockAcquire(state->lock, LW_EXCLUSIVE);
    state->bgw_latch = NULL;
    LWLockRelease(state->lock);    

    proc_exit(0);
}
