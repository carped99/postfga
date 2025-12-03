/* options.c */

#include <postgres.h>

#include <commands/defrem.h>
#include <foreign/foreign.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>

#include "fdw.h"

static inline bool
is_blank(const char *str)
{
    return str == NULL || str[0] == '\0';
}

static PostfgaFdwTableKind parse_table_kind(const char* val)
{
    if (pg_strcasecmp(val, POSTFGA_FDW_TABLE_KIND_ACL_NAME) == 0)
        return POSTFGA_FDW_TABLE_KIND_ACL;
    if (pg_strcasecmp(val, POSTFGA_FDW_TABLE_KIND_TUPLE_NAME) == 0)
        return POSTFGA_FDW_TABLE_KIND_TUPLE;
    if (pg_strcasecmp(val, POSTFGA_FDW_TABLE_KIND_STORE_NAME) == 0)
        return POSTFGA_FDW_TABLE_KIND_STORE;

    return POSTFGA_FDW_TABLE_KIND_UNKNOWN;
}

PostfgaFdwTableOptions* postfga_get_table_options(Oid foreigntableid)
{
    ForeignTable* ft = GetForeignTable(foreigntableid);
    ForeignServer* fs = GetForeignServer(ft->serverid);
    PostfgaFdwTableOptions* opts;
    ListCell* lc;

    opts = (PostfgaFdwTableOptions*)palloc0(sizeof(PostfgaFdwTableOptions));
    opts->kind = POSTFGA_FDW_TABLE_KIND_UNKNOWN;

    /* 1) foreign server */
    foreach (lc, fs->options)
    {
        DefElem* def = (DefElem*)lfirst(lc);

        if (strcmp(def->defname, "endpoint") == 0)
            opts->endpoint = pstrdup(defGetString(def));
        else if (strcmp(def->defname, "store_id") == 0)
            opts->store_id = pstrdup(defGetString(def));
        else if (strcmp(def->defname, "auth_model_id") == 0)
            opts->auth_model_id = pstrdup(defGetString(def));
    }

    /* 2) foreign table */
    foreach (lc, ft->options)
    {
        DefElem* def = (DefElem*)lfirst(lc);

        if (strcmp(def->defname, POSTFGA_FDW_TABLE_KIND_NAME) == 0)
            opts->kind = parse_table_kind(defGetString(def));
        else if (strcmp(def->defname, "endpoint") == 0)
            opts->endpoint = pstrdup(defGetString(def));
        else if (strcmp(def->defname, "store_id") == 0)
            opts->store_id = pstrdup(defGetString(def));
        else if (strcmp(def->defname, "auth_model_id") == 0)
            opts->auth_model_id = pstrdup(defGetString(def));
    }

    if (opts->kind == POSTFGA_FDW_TABLE_KIND_UNKNOWN)
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OPTION_NAME_NOT_FOUND),
                 errmsg("option \"%s\" is required", POSTFGA_FDW_TABLE_KIND_NAME),
                 errcontext("foreign table \"%s\"", get_rel_name(foreigntableid))));

    if (is_blank(opts->endpoint))
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OPTION_NAME_NOT_FOUND),
                 errmsg("option \"endpoint\" is required"),
                 errcontext("foreign table \"%s\"", get_rel_name(foreigntableid))));

    if ((opts->kind == POSTFGA_FDW_TABLE_KIND_ACL || opts->kind == POSTFGA_FDW_TABLE_KIND_TUPLE) && is_blank(opts->store_id))
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OPTION_NAME_NOT_FOUND),
                 errmsg("option \"store_id\" is required"),
                 errcontext("foreign table \"%s\"", get_rel_name(foreigntableid))));

    return opts;
}
