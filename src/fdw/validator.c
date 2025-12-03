/* validator.c */

#include <postgres.h>
#include <fmgr.h>
#include <access/reloptions.h>
#include <foreign/fdwapi.h>
#include <foreign/foreign.h>
#include <catalog/pg_foreign_data_wrapper.h>
#include <catalog/pg_foreign_server.h>
#include <catalog/pg_foreign_table.h>
#include <catalog/pg_user_mapping.h>
#include <commands/defrem.h>
#include <utils/rel.h>
#include <utils/builtins.h>

#include "fdw.h"

PG_FUNCTION_INFO_V1(postfga_fdw_validator);

extern Datum postfga_fdw_validator(PG_FUNCTION_ARGS);

static void
postfga_validate_server_options(List *options)
{
    ListCell *lc;
    bool endpoint_seen = false;

    foreach (lc, options)
    {
        DefElem    *def  = (DefElem *) lfirst(lc);
        const char *name = def->defname;

        if (strcmp(name, "endpoint") == 0)
        {
            (void) defGetString(def);  /* 문자열인지 확인 */
            endpoint_seen = true;
        }
        else if (strcmp(name, "store_id") == 0)
        {
            (void) defGetString(def);
        }
        else if (strcmp(name, "auth_model_id") == 0)
        {
            (void) defGetString(def);
        }
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid option \"%s\" for SERVER", name),
                     errhint("Valid options: endpoint, store_id, auth_model_id")));
        }
    }

    if (!endpoint_seen)
    {
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OPTION_NAME_NOT_FOUND),
                 errmsg("option \"endpoint\" is required for SERVER"),
                 errhint("Specify endpoint option.")));
    }
}

static void
postfga_validate_user_mapping_options(List *options)
{
    ListCell *lc;

    foreach (lc, options)
    {
        DefElem    *def  = (DefElem *) lfirst(lc);
        const char *name = def->defname;

        if (strcmp(name, "bearer_token") == 0)
        {
            (void) defGetString(def);
        }
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid option \"%s\" for USER MAPPING", name),
                     errhint("Valid options: bearer_token")));
        }
    }
}

static void
postfga_validate_table_options(List *options)
{
    ListCell *lc;
    bool      kind_seen = false;

    foreach (lc, options)
    {
        DefElem    *def  = (DefElem *) lfirst(lc);
        const char *name = def->defname;

        if (strcmp(name, POSTFGA_FDW_TABLE_KIND_NAME) == 0)
        {
            const char *val = defGetString(def);

            if (strcmp(val, POSTFGA_FDW_TABLE_KIND_ACL_NAME) != 0 &&
                strcmp(val, POSTFGA_FDW_TABLE_KIND_STORE_NAME) != 0 &&
                strcmp(val, POSTFGA_FDW_TABLE_KIND_TUPLE_NAME) != 0)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                         errmsg("invalid value for option \"%s\": \"%s\"", POSTFGA_FDW_TABLE_KIND_NAME, val),
                         errhint("Available values: acl, tuple, store")));
            }
            kind_seen = true;
        }
        else if (strcmp(name, "store_id") == 0)
        {
            (void) defGetString(def);
        }
        else if (strcmp(name, "auth_model_id") == 0)
        {
            (void) defGetString(def);
        }
        else
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid option \"%s\" for FOREIGN TABLE", name),
                     errhint("Valid options: resource, store_id, auth_model_id")));
        }
    }

    if (!kind_seen)
    {
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OPTION_NAME_NOT_FOUND),
                 errmsg("option \"kind\" is required for FOREIGN TABLE"),
                 errhint("Specify kind option (acl, store, tuple).")));
    }
}

Datum
postfga_fdw_validator(PG_FUNCTION_ARGS)
{
    List *options = untransformRelOptions(PG_GETARG_DATUM(0));
    Oid   catalog = PG_GETARG_OID(1);

    switch (catalog)
    {
        case ForeignServerRelationId:
            postfga_validate_server_options(options);
            break;

        case UserMappingRelationId:
            postfga_validate_user_mapping_options(options);
            break;

        case ForeignTableRelationId:
            postfga_validate_table_options(options);
            break;

        case ForeignDataWrapperRelationId:
            /* Nothing to do */
            break;

        default:
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_ERROR),
                     errmsg("postfga_fdw_validator: unexpected catalog OID %u", catalog)));
    }

    PG_RETURN_VOID();
}