/* plan.c */

#include <postgres.h>

#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <optimizer/planmain.h>
#include <optimizer/restrictinfo.h>

#include "fdw.h"

void postfgaGetForeignRelSize(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid)
{
    PostfgaFdwTableOptions* opts = postfga_get_table_options(foreigntableid);

    switch (opts->kind)
    {
    case POSTFGA_FDW_TABLE_KIND_ACL:
        baserel->rows = 10000;
        if (baserel->baserestrictinfo == NIL)
            ereport(ERROR, errmsg("full scan not allowed for postfga_tuple"));
        break;
    case POSTFGA_FDW_TABLE_KIND_TUPLE:
        baserel->rows = 10000; /* TODO: 적당히 추정 */
        break;
    case POSTFGA_FDW_TABLE_KIND_STORE:
        baserel->rows = 10000;
        break;
    default:
        baserel->rows = 100;
        break;
    }
}

void postfgaGetForeignPaths(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid)
{
    Cost startup_cost = 0;
    Cost total_cost = baserel->rows;
    ForeignPath* path;

#if PG_VERSION_NUM >= 180000
    path = create_foreignscan_path(root,
                                   baserel,
                                   NULL,          /* target */
                                   baserel->rows, /* rows */
                                   0,             /* disabled_nodes */
                                   startup_cost,
                                   total_cost,
                                   NIL,                     /* pathkeys */
                                   baserel->lateral_relids, /* required_outer */
                                   NULL,                    /* fdw_outerpath */
                                   NIL,                     /* fdw_restrictinfo */
                                   NIL);                    /* fdw_private */
#else
#error "Add compatibility code for this PostgreSQL version"
#endif

    add_path(baserel, (Path*)path);
}

ForeignScan* postfgaGetForeignPlan(PlannerInfo* root,
                                   RelOptInfo* baserel,
                                   Oid foreigntableid,
                                   ForeignPath* best_path,
                                   List* tlist,
                                   List* scan_clauses,
                                   Plan* outer_plan)
{
    Index scan_relid = baserel->relid;

    scan_clauses = extract_actual_clauses(scan_clauses, false);

    return make_foreignscan(tlist,
                            scan_clauses,
                            scan_relid,
                            NIL, /* expressions */
                            NIL, /* fdw_private (원하면 opts 일부를 담을 수 있음) */
                            NIL,
                            NIL,
                            outer_plan);
}
