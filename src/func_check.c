#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>

#include "guc.h"   /* postfga.store_id 등 GUC 선언 */
#include "shmem.h" /* shared state helper */
#include "state.h" /* shared state helper */
#include "queue.h" /* enqueue/wait helpers */
#include "func_check.h"

PG_FUNCTION_INFO_V1(postfga_check);

/*
 * SQL:
 *   postfga_check(object_type, object_id, relation, subject_type, subject_id)
 */
Datum postfga_check(PG_FUNCTION_ARGS)
{
    const char *object_type = text_to_cstring(PG_GETARG_TEXT_PP(0));
    const char *object_id = text_to_cstring(PG_GETARG_TEXT_PP(1));
    const char *relation = text_to_cstring(PG_GETARG_TEXT_PP(2));
    const char *subject_type = text_to_cstring(PG_GETARG_TEXT_PP(3));
    const char *subject_id = text_to_cstring(PG_GETARG_TEXT_PP(4));

    PostfgaShmemState *shmem_state = postfga_get_sheme_state();

    bool ok;
    // ok = enqueue_grpc_request(object_type, object_id, subject_type, subject_id, relation);

    /*
     * ok == false → 내부적으로 타임아웃/큐 오류 등.
     * 정책에 따라:
     *   - 바로 ERROR
     *   - false 반환
     * 를 GUC로 제어해도 좋음.
     */
    if (!ok)
        PG_RETURN_BOOL(false);

    PG_RETURN_BOOL(true);
}
