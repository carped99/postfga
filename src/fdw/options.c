/* options.c */

#include <postgres.h>

#include <commands/defrem.h>
#include <foreign/foreign.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>

#include "fdw.h"

static PostfgaTableKind parse_kind(const char* val)
{
    if (pg_strcasecmp(val, "acl") == 0)
        return POSTFGA_FDW_TABLE_KIND_ACL;
    if (pg_strcasecmp(val, "tuple") == 0)
        return POSTFGA_FDW_TABLE_KIND_TUPLE;
    if (pg_strcasecmp(val, "store") == 0)
        return POSTFGA_FDW_TABLE_KIND_STORE;

    ereport(ERROR,
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
             errmsg("postfga: invalid kind option \"%s\"", val),
             errhint("available values: acl, tuple, store")));

    return POSTFGA_FDW_TABLE_KIND_UNKNOWN; /* not reached */
}

PostfgaTableOptions* postfga_get_table_options(Oid foreigntableid)
{
    ForeignTable* ft = GetForeignTable(foreigntableid);
    ForeignServer* fs = GetForeignServer(ft->serverid);
    PostfgaTableOptions* opts;
    ListCell* lc;

    opts = (PostfgaTableOptions*)palloc0(sizeof(PostfgaTableOptions));
    opts->kind = POSTFGA_FDW_TABLE_KIND_UNKNOWN;
    opts->endpoint = NULL;
    opts->base_url = NULL;

    /* 1) foreign table 옵션 (kind, endpoint 등) */
    foreach (lc, ft->options)
    {
        DefElem* def = (DefElem*)lfirst(lc);

        if (strcmp(def->defname, "kind") == 0)
            opts->kind = parse_kind(defGetString(def));
        else if (strcmp(def->defname, "endpoint") == 0)
            opts->endpoint = pstrdup(defGetString(def));
    }

    /* 2) server 옵션 (base_url 등) */
    foreach (lc, fs->options)
    {
        DefElem* def = (DefElem*)lfirst(lc);

        if (strcmp(def->defname, "base_url") == 0)
            opts->base_url = pstrdup(defGetString(def));
    }

    if (opts->kind == POSTFGA_FDW_TABLE_KIND_UNKNOWN)
        ereport(ERROR,
                (errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
                 errmsg("postfga_fdw: option \"kind\" is required for foreign table \"%s\"",
                        get_rel_name(foreigntableid))));

    return opts;
}
