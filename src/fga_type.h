#ifndef POSTFGA_TUPLE_H
#define POSTFGA_TUPLE_H

#include <stdint.h>
#include <stdbool.h>

#include "common.h"

#define FGA_MAX_BATCH 64

typedef struct FgaTuple
{
    char object_type[OBJECT_TYPE_MAX_LEN];
    char object_id[OBJECT_ID_MAX_LEN];
    char subject_type[SUBJECT_TYPE_MAX_LEN];
    char subject_id[SUBJECT_ID_MAX_LEN];
    char relation[RELATION_MAX_LEN];
} FgaTuple;

/* ---- Request state ----------------------------------------------------- */
typedef enum FgaRequestState
{
    FGA_REQ_EMPTY = 0,
    FGA_REQ_PENDING,
    FGA_REQ_PROCESSING,
    FGA_REQ_DONE,
    FGA_REQ_ERROR,
} FgaRequestState;

/* ---- Request type --------------------------- */
typedef enum FgaRequestType
{
    FGA_REQ_CHECK = 1,
    FGA_REQ_READ,
    FGA_REQ_WRITE,
    FGA_REQ_DELETE,
    FGA_REQ_LIST,
    FGA_REQ_GET_STORE,
    FGA_REQ_CREATE_STORE,
} FgaRequestType;

typedef struct FgaCheckTupleRequest
{
    uint16_t op_count; /* slot의 op_count와 동일하게 유지 */
    uint16_t _pad;
    FgaTuple tuples[FGA_MAX_BATCH];
} FgaCheckTupleRequest;

typedef struct FgaCheckTupleResponse
{
    uint16_t op_count;
    uint16_t _pad;
    bool allowed[FGA_MAX_BATCH];
} FgaCheckTupleResponse;

typedef struct FgaWriteTupleRequest
{
    uint16_t op_count; /* slot의 op_count와 동일하게 유지 */
    uint16_t _pad;
    FgaTuple tuples[FGA_MAX_BATCH];
} FgaWriteTupleRequest;

typedef struct FgaWriteTupleResponse
{
    uint16_t op_count;
    uint16_t _pad;
    int32_t error_codes[FGA_MAX_BATCH]; /* 0=OK, others=error */
} FgaWriteTupleResponse;

typedef struct FgaDeleteTupleRequest
{
    uint16_t op_count; /* slot의 op_count와 동일하게 유지 */
    uint16_t _pad;
    FgaTuple tuples[FGA_MAX_BATCH];
} FgaDeleteTupleRequest;

typedef struct FgaDeleteTupleResponse
{
    uint16_t op_count;
    uint16_t _pad;
    int32_t error_codes[FGA_MAX_BATCH]; /* 0=OK, others=error */
} FgaDeleteTupleResponse;

/* store 조회 */
typedef struct FgaGetStoreRequest
{
    char store_id[STORE_ID_LEN];
} FgaGetStoreRequest;

typedef struct FgaGetStoreResponse
{
    bool found;
    char store_id[STORE_ID_LEN];
    char name[STORE_NAME_LEN];
} FgaGetStoreResponse;

/* store 생성 */
typedef struct FgaCreateStoreRequest
{
    char name[STORE_NAME_LEN];
    char owner_subject_type[32];
    char owner_subject_id[64];
} FgaCreateStoreRequest;

typedef struct FgaCreateStoreResponse
{
    bool success;
    char store_id[STORE_ID_LEN];
    int32_t error_code;
} FgaCreateStoreResponse;

typedef struct FgaRequest
{
    uint32_t request_id; /* 요청 식별자 (slot index나 generation 등) */
    uint16_t type;       /* FgaRequestType */
    uint16_t reserved;   /* alignment / flags 용 */
    union
    {
        FgaCheckTupleRequest checkTuple; /* 사실상 check/write/delete와 동일 레이아웃이면 재활용 가능 */
        FgaWriteTupleRequest writeTuple;
        FgaDeleteTupleRequest deleteTuple;
        FgaGetStoreRequest getStore;
        FgaCreateStoreRequest createStore;
        /* 나중에 추가 예정인 op 들도 여기에 계속 추가 */
    } body;
} FgaRequest;

typedef struct FgaResponse
{
    uint16_t type; /* FgaMessageType (요청 타입과 매칭) */
    uint16_t reserved;
    uint32_t request_id; /* 요청 쪽 request_id 그대로 echo */

    union
    {
        FgaCheckTupleResponse checkTuple;
        FgaWriteTupleResponse writeTuple;
        FgaDeleteTupleResponse deleteTuple;
        FgaGetStoreResponse getStore;
        FgaCreateStoreResponse createStore;
    } body;
} FgaResponse;

#endif // POSTFGA_TUPLE_H