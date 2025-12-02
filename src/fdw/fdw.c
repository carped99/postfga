/* postfga_fdw.c */

#include <postgres.h>

#include <access/htup_details.h>
#include <catalog/pg_type.h>
#include <commands/defrem.h>
#include <foreign/fdwapi.h>
#include <foreign/foreign.h>
#include <miscadmin.h>
#include <nodes/makefuncs.h>
#include <nodes/pg_list.h>
#include <optimizer/cost.h>
#include <optimizer/pathnode.h>
#include <optimizer/planmain.h>
#include <optimizer/restrictinfo.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/memutils.h>
#include <utils/rel.h>
#include <utils/timestamp.h>

extern Datum postfga_fdw_handler(PG_FUNCTION_ARGS);
extern Datum postfga_fdw_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(postfga_fdw_handler);
PG_FUNCTION_INFO_V1(postfga_fdw_validator);

typedef enum PostfgaTableKind
{
    POSTFGA_TABLE_TUPLE,
    POSTFGA_TABLE_ACL
} PostfgaTableKind;

typedef struct PostfgaScanState
{
    PostfgaTableKind kind;
    int row; /* 0, 1, ... */
} PostfgaScanState;

/* 콜백 프로토타입 */
static void postfgaGetForeignRelSize(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);
static void postfgaGetForeignPaths(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid);
static ForeignScan* postfgaGetForeignPlan(PlannerInfo* root,
                                          RelOptInfo* baserel,
                                          Oid foreigntableid,
                                          ForeignPath* best_path,
                                          List* tlist,
                                          List* scan_clauses,
                                          Plan* outer_plan);
static void postfgaBeginForeignScan(ForeignScanState* node, int eflags);
static TupleTableSlot* postfgaIterateForeignScan(ForeignScanState* node);
static void postfgaReScanForeignScan(ForeignScanState* node);
static void postfgaEndForeignScan(ForeignScanState* node);

/* FDW 핸들러 */
Datum postfga_fdw_handler(PG_FUNCTION_ARGS)
{
    FdwRoutine* routine = makeNode(FdwRoutine);

    routine->GetForeignRelSize = postfgaGetForeignRelSize;
    routine->GetForeignPaths = postfgaGetForeignPaths;
    routine->GetForeignPlan = postfgaGetForeignPlan;

    routine->BeginForeignScan = postfgaBeginForeignScan;
    routine->IterateForeignScan = postfgaIterateForeignScan;
    routine->ReScanForeignScan = postfgaReScanForeignScan;
    routine->EndForeignScan = postfgaEndForeignScan;

    PG_RETURN_POINTER(routine);
}

/* 옵션 검증 – 일단 아무 것도 안 함 */
Datum postfga_fdw_validator(PG_FUNCTION_ARGS)
{
    PG_RETURN_VOID();
}

/* row 수 대충 추정 – 데모라 고정 2 */
static void postfgaGetForeignRelSize(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid)
{
    baserel->rows = 2;
}

static void postfgaGetForeignPaths(PlannerInfo* root, RelOptInfo* baserel, Oid foreigntableid)
{
    Cost startup_cost = 0;
    Cost total_cost = baserel->rows;
    ForeignPath* path = create_foreignscan_path(root,
                                                baserel,
                                                NULL,          /* PathTarget (default) */
                                                baserel->rows, /* rows */
                                                0,             /* disabled_nodes */
                                                startup_cost,
                                                total_cost,
                                                NIL,                     /* pathkeys */
                                                baserel->lateral_relids, /* required_outer (보통 이렇게) */
                                                NULL,                    /* fdw_outerpath */
                                                NIL,                     /* fdw_restrictinfo */
                                                NIL);                    /* fdw_private */

    add_path(baserel, (Path*)path);
}

static ForeignScan* postfgaGetForeignPlan(PlannerInfo* root,
                                          RelOptInfo* baserel,
                                          Oid foreigntableid,
                                          ForeignPath* best_path,
                                          List* tlist,
                                          List* scan_clauses,
                                          Plan* outer_plan)
{
    Index scan_relid = baserel->relid;

    scan_clauses = extract_actual_clauses(scan_clauses, false);

    return make_foreignscan(tlist, scan_clauses, scan_relid, NIL, NIL, NIL, NIL, outer_plan);
}

/* 어떤 테이블인지 이름으로 구분 */
static PostfgaTableKind get_table_kind(Relation rel)
{
    Oid relid = RelationGetRelid(rel);
    char* name = get_rel_name(relid);

    if (name == NULL)
        ereport(ERROR, (errmsg("could not get relation name for OID %u", relid)));

    if (strcmp(name, "postfga_tuple") == 0)
        return POSTFGA_TABLE_TUPLE;
    if (strcmp(name, "postfga_acl") == 0)
        return POSTFGA_TABLE_ACL;

    ereport(ERROR, (errmsg("postfga_fdw: unknown foreign table \"%s\"", name)));
    return POSTFGA_TABLE_TUPLE; /* not reached */
}

static void postfgaBeginForeignScan(ForeignScanState* node, int eflags)
{
    Relation rel = node->ss.ss_currentRelation;
    PostfgaScanState* state;

    state = (PostfgaScanState*)palloc0(sizeof(PostfgaScanState));
    state->row = 0;
    state->kind = get_table_kind(rel);

    node->fdw_state = (void*)state;
}

/*
 * 실제 데이터 채우는 부분 (데모용)
 *
 * postfga_tuple:
 *   row 0: ('doc', 'doc:1', 'user', 'user:1', 'viewer')
 *   row 1: ('doc', 'doc:2', 'user', 'user:2', 'viewer')
 *
 * postfga_acl:
 *   row 0: 위 필드 + allow=true, evaluated_at=now()
 *   row 1: 다른 subject + allow=false, evaluated_at=now()
 */
static TupleTableSlot* postfgaIterateForeignScan(ForeignScanState* node)
{
    Datum* values;
    bool* nulls;
    PostfgaScanState* state = (PostfgaScanState*)node->fdw_state;
    TupleTableSlot* slot = node->ss.ss_ScanTupleSlot;
    TupleDesc tupdesc = slot->tts_tupleDescriptor;
    int natts = tupdesc->natts;

    ExecClearTuple(slot);

    if (state->row >= 2)
        return slot; /* 더 이상 row 없음 */


    values = (Datum*)palloc0(sizeof(Datum) * natts);
    nulls = (bool*)palloc0(sizeof(bool) * natts);

    for (int i = 0; i < natts; i++)
        nulls[i] = false;

    if (state->kind == POSTFGA_TABLE_TUPLE)
    {
        /* 기대 스키마: text x5 */
        if (natts < 5)
            ereport(ERROR, (errmsg("postfga_tuple must have at least 5 columns")));

        if (state->row == 0)
        {
            values[0] = CStringGetTextDatum("doc");
            values[1] = CStringGetTextDatum("doc:1");
            values[2] = CStringGetTextDatum("user");
            values[3] = CStringGetTextDatum("user:1");
            values[4] = CStringGetTextDatum("viewer");
        }
        else
        {
            values[0] = CStringGetTextDatum("doc");
            values[1] = CStringGetTextDatum("doc:2");
            values[2] = CStringGetTextDatum("user");
            values[3] = CStringGetTextDatum("user:2");
            values[4] = CStringGetTextDatum("viewer");
        }
    }
    else /* POSTFGA_TABLE_ACL */
    {
        /* 기대 스키마: text x5, bool, timestamptz */
        if (natts < 7)
            ereport(ERROR, (errmsg("postfga_acl must have at least 7 columns")));

        TimestampTz now = GetCurrentTimestamp();

        if (state->row == 0)
        {
            values[0] = CStringGetTextDatum("doc");
            values[1] = CStringGetTextDatum("doc:1");
            values[2] = CStringGetTextDatum("user");
            values[3] = CStringGetTextDatum("user:1");
            values[4] = CStringGetTextDatum("viewer");
            values[5] = BoolGetDatum(true);       /* allow */
            values[6] = TimestampTzGetDatum(now); /* evaluated_at */
        }
        else
        {
            values[0] = CStringGetTextDatum("doc");
            values[1] = CStringGetTextDatum("doc:2");
            values[2] = CStringGetTextDatum("user");
            values[3] = CStringGetTextDatum("user:2");
            values[4] = CStringGetTextDatum("viewer");
            values[5] = BoolGetDatum(false);
            values[6] = TimestampTzGetDatum(now);
        }
    }

    ExecClearTuple(slot);
    ExecStoreVirtualTuple(slot);
    memcpy(slot->tts_values, values, sizeof(Datum) * natts);
    memcpy(slot->tts_isnull, nulls, sizeof(bool) * natts);

    pfree(values);
    pfree(nulls);

    state->row++;

    return slot;
}

static void postfgaReScanForeignScan(ForeignScanState* node)
{
    PostfgaScanState* state = (PostfgaScanState*)node->fdw_state;

    state->row = 0;
}

static void postfgaEndForeignScan(ForeignScanState* node)
{
    if (node->fdw_state)
        pfree(node->fdw_state);
}
