/*
 * init.c - Main entry point for PostFGA extension
 *
 * PostgreSQL extension initialization and cleanup
 */

#include <postgres.h>

#include <fmgr.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>
#include <storage/ipc.h>

#include "bgw/main.h"
#include "guc.h"
#include "state.h"

/* Module magic */
PG_MODULE_MAGIC;

/* Hook variables */
static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

/* Function prototypes */
void _PG_init(void);
void _PG_fini(void);

static void fga_shmem_request_hook(void);
static void fga_shmem_startup_hook(void);

/* ------------------------------------------------------------------------- */
/* Module load / unload                                                      */
/* ------------------------------------------------------------------------- */
void _PG_init(void)
{
    if (!process_shared_preload_libraries_in_progress)
    {
        ereport(ERROR,
                (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                 errmsg("postfga must be loaded via shared_preload_libraries"),
                 errhint("Add 'postfga' to shared_preload_libraries in postgresql.conf and restart the server.")));
    }

    ereport(DEBUG1, (errmsg("postfga: initializing")));

    // Initialize GUC parameters first (may affect shmem size/config)
    fga_guc_init();

    // Register shared memory hooks, preserving existing hook chain
    prev_shmem_request_hook = shmem_request_hook;
    shmem_request_hook = fga_shmem_request_hook;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = fga_shmem_startup_hook;

    fga_bgw_init();

    ereport(DEBUG1, (errmsg("postfga: Extension initialized")));
}

void _PG_fini(void)
{
    ereport(LOG, (errmsg("postfga: Extension unloading")));

    fga_bgw_fini();
    fga_guc_fini();

    // Restore previous hooks so other extensions in the chain keep working
    shmem_request_hook = prev_shmem_request_hook;
    shmem_startup_hook = prev_shmem_startup_hook;
}

static void fga_shmem_request_hook(void)
{
    if (prev_shmem_request_hook)
        prev_shmem_request_hook();

    fga_shmem_request();
}

static void fga_shmem_startup_hook(void)
{
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    fga_shmem_startup();
}
