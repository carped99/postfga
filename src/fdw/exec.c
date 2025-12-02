/* exec.c */

#include <postgres.h>

#include <access/htup_details.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>

#include "fdw.h"

void postfgaBeginForeignScan(ForeignScanState* node, int eflags)
{
    Relation rel = node->ss.ss_currentRelation;
    Oid relid = RelationGetRelid(rel);
    PostfgaExecState* estate;

    estate = (PostfgaExecState*)palloc0(sizeof(PostfgaExecState));
    estate->row = 0;
    estate->opts = postfga_get_table_options(relid);

    /* TODO: 여기서 estate 안에 BGW 채널 핸들 등 초기화 */

    node->fdw_state = (void*)estate;
}

TupleTableSlot* postfgaIterateForeignScan(ForeignScanState* node)
{
    PostfgaExecState* estate = (PostfgaExecState*)node->fdw_state;
    TupleTableSlot* slot = node->ss.ss_ScanTupleSlot;
    TupleDesc tupdesc = slot->tts_tupleDescriptor;
    int natts = tupdesc->natts;

    ExecClearTuple(slot);

    /* 데모: 각 테이블마다 2 row만 반환 */
    if (estate->row >= 2)
        return slot;

    Datum* values = (Datum*)palloc0(sizeof(Datum) * natts);
    bool* nulls = (bool*)palloc0(sizeof(bool) * natts);
    for (int i = 0; i < natts; i++)
        nulls[i] = false;

    if (estate->opts->kind == POSTFGA_FDW_TABLE_KIND_TUPLE)
    {
        /* postfga_tuple: text x5 */
        if (natts < 5)
            ereport(ERROR, (errmsg("postfga_tuple must have at least 5 columns")));

        if (estate->row == 0)
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
    else if (estate->opts->kind == POSTFGA_FDW_TABLE_KIND_ACL)
    {
        /* postfga_acl: text x5, bool, timestamptz */
        if (natts < 7)
            ereport(ERROR, (errmsg("postfga_acl must have at least 7 columns")));

        TimestampTz now = GetCurrentTimestamp();

        if (estate->row == 0)
        {
            values[0] = CStringGetTextDatum("doc");
            values[1] = CStringGetTextDatum("doc:1");
            values[2] = CStringGetTextDatum("user");
            values[3] = CStringGetTextDatum("user:1");
            values[4] = CStringGetTextDatum("viewer");
            values[5] = BoolGetDatum(true);
            values[6] = TimestampTzGetDatum(now);
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
    else
    {
        ereport(ERROR, (errmsg("postfga_fdw: unknown table kind")));
    }

    ExecStoreVirtualTuple(slot);
    memcpy(slot->tts_values, values, sizeof(Datum) * natts);
    memcpy(slot->tts_isnull, nulls, sizeof(bool) * natts);

    pfree(values);
    pfree(nulls);

    estate->row++;

    return slot;
}

void postfgaReScanForeignScan(ForeignScanState* node)
{
    PostfgaExecState* estate = (PostfgaExecState*)node->fdw_state;

    estate->row = 0;
}

void postfgaEndForeignScan(ForeignScanState* node)
{
    PostfgaExecState* estate = (PostfgaExecState*)node->fdw_state;

    if (estate != NULL)
    {
        /* TODO: BGW 채널 핸들 등 정리 */
        pfree(estate);
        node->fdw_state = NULL;
    }
}
