#include "fdw.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "postgres.h"
#include "utils/builtins.h"

List* fgaImportForeignSchema(ImportForeignSchemaStmt* stmt, Oid serverOid)
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
    ;
    appendStringInfo(
        &buf, ") SERVER %s OPTIONS (kind '%s');", quote_identifier(stmt->server_name), FGA_FDW_TABLE_KIND_TUPLE_NAME);
    cmds = lappend(cmds, pstrdup(buf.data));

    return cmds;
}
