/* exec.c */

#include <postgres.h>

#include <access/htup_details.h>
#include <utils/builtins.h>
#include <utils/timestamp.h>

#include "fdw.h"

void postfgaBeginForeignScan(ForeignScanState* node, int eflags)
{
    Relation            rel   = node->ss.ss_currentRelation;
    Oid                 relid = RelationGetRelid(rel);
    PostfgaFdwExecState *estate = (PostfgaFdwExecState *) palloc0(sizeof(PostfgaFdwExecState));
    estate->row  = 0;
    estate->opts = postfga_get_table_options(relid);

    /* TODO: 여기서 BGW 채널, 클라이언트 핸들 등 초기화 */

    node->fdw_state = (void *) estate;
}

TupleTableSlot* postfgaIterateForeignScan(ForeignScanState* node)
{
    PostfgaFdwExecState* estate = (PostfgaFdwExecState*)node->fdw_state;
    TupleTableSlot* slot = node->ss.ss_ScanTupleSlot;
    TupleDesc tupdesc = slot->tts_tupleDescriptor;
    int natts = tupdesc->natts;

    ExecClearTuple(slot);

    /* 데모: 각 테이블마다 2 row만 반환 */
    if (estate->row >= 2)
        return slot;

    Datum *values = slot->tts_values;
    bool  *nulls  = slot->tts_isnull;

    /* 기본은 전부 NULL */
    for (int i = 0; i < natts; i++)
    {
        values[i] = (Datum) 0;
        nulls[i]  = false;
    }        

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
    else if (estate->opts->kind == POSTFGA_FDW_TABLE_KIND_STORE)
    {
        TimestampTz now = GetCurrentTimestamp();
        /* postfga_store: text x2 */
        if (natts < 2)
            ereport(ERROR, (errmsg("postfga_store must have at least 2 columns")));

        if (estate->row == 0)
        {
            values[0] = CStringGetTextDatum("store:1");
            values[1] = CStringGetTextDatum("First Store");
            values[2] = TimestampTzGetDatum(now);
            values[3] = TimestampTzGetDatum(now);
        }
        else
        {
            values[0] = CStringGetTextDatum("store:2");
            values[1] = CStringGetTextDatum("Second Store");
            values[2] = TimestampTzGetDatum(now);
            values[3] = TimestampTzGetDatum(now);            
        }
    }
    else
    {
        ereport(ERROR, (errcode(ERRCODE_FDW_TABLE_NOT_FOUND), errmsg("postfga_fdw: unknown table kind")));
    }

    ExecStoreVirtualTuple(slot);
    estate->row++;

    return slot;
}

void postfgaReScanForeignScan(ForeignScanState* node)
{
    PostfgaFdwExecState* estate = (PostfgaFdwExecState*)node->fdw_state;

    if (estate)
        estate->row = 0;
    /* 나중에 OpenFGA paging 붙이면 여기서 continuation_token 초기화 같은 것 할 수 있음 */
}

void postfgaEndForeignScan(ForeignScanState* node)
{
    PostfgaFdwExecState* estate = (PostfgaFdwExecState*)node->fdw_state;

    if (estate == NULL)
        return;

    /* TODO: BGW 채널 핸들, 클라이언트, 버퍼 등 정리 */

    if (estate->opts)
        pfree(estate->opts);

    pfree(estate);
    node->fdw_state = NULL;
}
