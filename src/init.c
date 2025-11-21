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

#include "guc.h"
#include "shmem.h"
#include "bgw/main.h"

/* Module magic */
PG_MODULE_MAGIC;

/* Hook variables */
static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

/* ------------------------------------------------------------------------- */
/* Hook implementations                                                      */
/* ------------------------------------------------------------------------- */
static void
postfga_shmem_request_hook(void)
{
    if (prev_shmem_request_hook)
        prev_shmem_request_hook();

    postfga_request_shmem();
}

static void
postfga_shmem_startup_hook(void)
{
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    postfga_startup_shmem();
}


/* ------------------------------------------------------------------------- */
/* Module load / unload                                                      */
/* ------------------------------------------------------------------------- */
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

    ereport(LOG, (errmsg("PostFGA: initializing extension")));

    // Initialize GUC parameters first (may affect shmem size/config)
    postfga_guc_init();

    // Register shared memory hooks, preserving existing hook chain
    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = postfga_shmem_request_hook;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = postfga_shmem_startup_hook;
    
    postfga_bgw_init();

    ereport(LOG, (errmsg("PostFGA: Extension initialized")));
}

void
_PG_fini(void)
{
    ereport(LOG, (errmsg("PostFGA: Extension unloading")));

    postfga_bgw_fini();
    postfga_guc_fini();

    // Restore previous hooks so other extensions in the chain keep working
    shmem_request_hook = prev_shmem_request_hook;
    shmem_startup_hook = prev_shmem_startup_hook;
}
