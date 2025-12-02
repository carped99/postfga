/* fdw.c */

#include <postgres.h>
#include <fmgr.h>
#include <foreign/fdwapi.h>

#include "fdw.h"

PG_FUNCTION_INFO_V1(postfga_fdw_handler);
PG_FUNCTION_INFO_V1(postfga_fdw_validator);

extern Datum postfga_fdw_handler(PG_FUNCTION_ARGS);
extern Datum postfga_fdw_validator(PG_FUNCTION_ARGS);

Datum
postfga_fdw_handler(PG_FUNCTION_ARGS)
{
    FdwRoutine *routine = makeNode(FdwRoutine);

    /* planner 콜백 */
    routine->GetForeignRelSize = postfgaGetForeignRelSize;
    routine->GetForeignPaths   = postfgaGetForeignPaths;
    routine->GetForeignPlan    = postfgaGetForeignPlan;

    /* executor 콜백 */
    routine->BeginForeignScan   = postfgaBeginForeignScan;
    routine->IterateForeignScan = postfgaIterateForeignScan;
    routine->ReScanForeignScan  = postfgaReScanForeignScan;
    routine->EndForeignScan     = postfgaEndForeignScan;

    PG_RETURN_POINTER(routine);
}

/* 옵션 validator – 필요하면 FDW 옵션 검사 추가 */
Datum
postfga_fdw_validator(PG_FUNCTION_ARGS)
{
    PG_RETURN_VOID();
}
