/*
 * init.c - Main entry point for PostFGA extension
 *
 * PostgreSQL extension initialization and cleanup
 */
#include <postgres.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <storage/ipc.h>
#include <postmaster/bgworker.h>

#include "state.h"
#include "bgw.h"
#include "guc.h"

/* Module magic */
PG_MODULE_MAGIC;

/* Hook variables */
shmem_request_hook_type prev_shmem_request_hook = NULL;
shmem_startup_hook_type prev_shmem_startup_hook = NULL;

static void
postfga_shmem_request_hook(void)
{
    if (prev_shmem_request_hook)
        prev_shmem_request_hook();

    postfga_shmem_request();
}

static void
postfga_shmem_startup_hook(void)
{
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    postfga_shmem_startup();
}

void
_PG_init(void)
{
    if (!process_shared_preload_libraries_in_progress)
    {
        ereport(ERROR,
                (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                 errmsg("postfga must be loaded via shared_preload_libraries"),
                 errhidestmt(true)));
    }

    elog(LOG, "PostFGA: Initializing extension");

    /* Register shared memory hooks */
    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = postfga_shmem_request_hook;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = postfga_shmem_startup_hook;

    postfga_guc_init();
    postfga_bgw_init();

    elog(LOG, "PostFGA: Extension initialized");
}

void
_PG_fini(void)
{
    elog(LOG, "PostFGA: Extension unloading");

    postfga_bgw_fini();
    postfga_guc_fini();

    /* Restore hooks */
    shmem_request_hook = prev_shmem_request_hook;
    shmem_startup_hook = prev_shmem_startup_hook;
}
