/* backend_request.c */

#include "channel_queue.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postgres.h"
#include "request.h"
#include "shmem.h"

/*-------------------------------------------------------------------------
 * Static helpers
 *-------------------------------------------------------------------------*/
FgaRequest* postfga_init_check_request(FgaRequest* req, const FgaCheckTupleRequest* body)
{
    MemSet(req, 0, sizeof(FgaRequest));
    // req->request_id = request_id;
    // req->type = (uint16)FGA_REQ_CHECK_TUPLE;
    req->reserved = 0;

    memcpy(&req->body.checkTuple, body, sizeof(FgaCheckTupleRequest));

    return req;
}

// /*
//  * 실제로 백엔드에서 요청 하나 enqueue 하는 함수
//  *
//  * 1. 슬롯 하나 확보
//  * 2. 요청 내용 채우기
//  * 3. 큐에 슬롯 인덱스 넣기
//  * 4. BGW 깨우기
//  */
// bool postfga_enqueue_check_request(const FgaCheckTupleRequest *request_body,
//                                    uint16 *out_slot_index,
//                                    uint64 *out_request_id,
//                                    char **err_msg)
// {
//     PostfgaShmemState *state = postfga_get_shmem_state();
//     uint64 request_id;

//     FgaSlotPool *slot_pool = &state->slot_pool;
//     FgaRequestSlot *slot;

//     /* 1. 슬롯 확보 */
//     uint16 slot_index;
//     if (!postfga_slot_acquire(slot_pool, &slot_index))
//     {
//         if (err_msg)
//             *err_msg = pstrdup("PostFGA: no free slot available");
//         return false;
//     }

//     slot = &slot_pool->slots[slot_index];

//     /* 2. 슬롯에 요청 내용 채우기 */
//     slot->backend_pid = MyProcPid;
//     slot->request_id = request_id;

//     FgaRequest *req = postfga_init_check_request(&slot->request, request_body);

//     // resp = &slot->response;
//     // memset(resp, 0, sizeof(FgaResponse));
//     // resp->request_id = request_id;
//     // resp->type = req->type;

//     // pg_write_barrier();
//     // pg_atomic_write_u32(&slot->state, FGA_SLOT_PENDING);

//     // /* 깊은 복사 / 문자열 복사는 필요한 만큼 */
//     // slot->check_req = *req; /* POD라면 얕은 복사 가능 */

//     /* 상태는 이미 PENDING 으로 set 되어 있음 (postfga_slot_acquire) */

//     LWLockAcquire(state->lock, LW_EXCLUSIVE);
//     bool enqueued = postfga_enqueue_slot(state->request_queue, slot_index);
//     LWLockRelease(state->lock);

//     if (!enqueued)
//     {
//         /* 큐가 가득 찬 경우: 슬롯 돌려주고 에러 */
//         postfga_slot_release(slot_pool, slot_index);

//         if (err_msg)
//             *err_msg = pstrdup("PostFGA: request queue is full");
//         return false;
//     }

//     /* 4. caller 에게 정보 반환 */
//     if (out_slot_index)
//         *out_slot_index = slot_index;
//     if (out_request_id)
//         *out_request_id = request_id;

//     return true;
// }
