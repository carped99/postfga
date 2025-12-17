/* handler.c */

#include <postgres.h>

#include <utils/builtins.h>
// #include <fmgr.h>
// #include <access/reloptions.h>
// #include <foreign/fdwapi.h>
// #include <foreign/foreign.h>
// #include <catalog/pg_foreign_data_wrapper.h>
// #include <catalog/pg_foreign_server.h>
// #include <catalog/pg_foreign_table.h>
// #include <catalog/pg_user_mapping.h>
// #include <commands/defrem.h>
// #include <utils/rel.h>
// #include <utils/builtins.h>


#include "fdw.h"

PG_FUNCTION_INFO_V1(fga_fdw_handler);

extern Datum fga_fdw_handler(PG_FUNCTION_ARGS);

static List* fgaImportForeignSchema(ImportForeignSchemaStmt* stmt, Oid serverOid);


Datum fga_fdw_handler(PG_FUNCTION_ARGS)
{
    FdwRoutine* routine = makeNode(FdwRoutine);

    /* planner callbacks */
    routine->GetForeignRelSize = fgaGetForeignRelSize;
    routine->GetForeignPaths = fgaGetForeignPaths;
    routine->GetForeignPlan = fgaGetForeignPlan;

    /* executor callbacks */
    routine->BeginForeignScan = fgaBeginForeignScan;
    routine->IterateForeignScan = fgaIterateForeignScan;
    routine->ReScanForeignScan = fgaReScanForeignScan;
    routine->EndForeignScan = fgaEndForeignScan;

    routine->ImportForeignSchema = fgaImportForeignSchema;

    PG_RETURN_POINTER(routine);
}

static List* fgaImportForeignSchema(ImportForeignSchemaStmt* stmt, Oid serverOid)
{
    List* cmds = NIL;

    StringInfoData buf;
    initStringInfo(&buf);

    appendStringInfo(&buf,
                     "CREATE FOREIGN TABLE IF NOT EXISTS %s.%s (",
                     quote_identifier(stmt->local_schema),
                     quote_identifier("fga_acl"));
    appendStringInfo(&buf, "object_type text NOT NULL, ");
    appendStringInfo(&buf, "object_id text NOT NULL, ");
    appendStringInfo(&buf, "subject_type text NOT NULL, ");
    appendStringInfo(&buf, "subject_id text NOT NULL, ");
    appendStringInfo(&buf, "relation text NOT NULL, ");
    appendStringInfo(&buf, "allow bool NOT NULL, ");
    appendStringInfo(&buf, "evaluated_at timestamptz NOT NULL ");
    appendStringInfo(
        &buf, ") SERVER %s OPTIONS (kind '%s');", quote_identifier(stmt->server_name), FGA_FDW_TABLE_KIND_ACL_NAME);
    cmds = lappend(cmds, pstrdup(buf.data));

    resetStringInfo(&buf);
    appendStringInfo(&buf,
                     "CREATE FOREIGN TABLE IF NOT EXISTS %s.%s (",
                     quote_identifier(stmt->local_schema),
                     quote_identifier("fga_store"));
    appendStringInfo(&buf, "id text NOT NULL, ");
    appendStringInfo(&buf, "name text NOT NULL, ");
    appendStringInfo(&buf, "created_at timestamptz NOT NULL, ");
    appendStringInfo(&buf, "updated_at timestamptz NOT NULL ");
    appendStringInfo(
        &buf, ") SERVER %s OPTIONS (kind '%s');", quote_identifier(stmt->server_name), FGA_FDW_TABLE_KIND_STORE_NAME);
    cmds = lappend(cmds, pstrdup(buf.data));

    resetStringInfo(&buf);
    appendStringInfo(&buf,
                     "CREATE FOREIGN TABLE IF NOT EXISTS %s.%s (",
                     quote_identifier(stmt->local_schema),
                     quote_identifier("fga_tuple"));
    appendStringInfo(&buf, "object_type text NOT NULL, ");
    appendStringInfo(&buf, "object_id text NOT NULL, ");
    appendStringInfo(&buf, "subject_type text NOT NULL, ");
    appendStringInfo(&buf, "subject_id text NOT NULL, ");
    appendStringInfo(&buf, "relation text NOT NULL ");
    appendStringInfo(
        &buf, ") SERVER %s OPTIONS (kind '%s');", quote_identifier(stmt->server_name), FGA_FDW_TABLE_KIND_TUPLE_NAME);
    cmds = lappend(cmds, pstrdup(buf.data));

    return cmds;
}
