#ifndef POSTFGA_PAYLOAD_H
#define POSTFGA_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "postfga.h"

#define FGA_MAX_BATCH 64
#define FGA_RESPONSE_ERROR_MESSAGE 128

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

typedef enum FgaResponseStatus
{
    FGA_RESPONSE_OK = 0,          /* 정상 */
    FGA_RESPONSE_CLIENT_ERROR,    /* 클라이언트 쪽 (요청 포맷 등) */
    FGA_RESPONSE_TRANSPORT_ERROR, /* gRPC/네트워크/타임아웃 */
    FGA_RESPONSE_SERVER_ERROR     /* OpenFGA 서버 에러 */
} FgaResponseStatus;

/* ---- Request type --------------------------- */
typedef enum FgaRequestType
{
    FGA_REQUEST_CHECK_TUPLE = 1,
    FGA_REQUEST_READ,
    FGA_REQUEST_WRITE_TUPLE,
    FGA_REQUEST_DELETE_TUPLE,
    FGA_REQUEST_LIST,
    FGA_REQUEST_GET_STORE,
    FGA_REQUEST_CREATE_STORE,
    FGA_REQUEST_DELETE_STORE,
} FgaRequestType;

typedef struct FgaCheckTupleRequest
{
    FgaTuple tuple;
} FgaCheckTupleRequest;

typedef struct FgaCheckTupleResponse
{
    bool allow;
} FgaCheckTupleResponse;

typedef struct FgaWriteTupleRequest
{
    FgaTuple tuple;
} FgaWriteTupleRequest;

typedef struct FgaWriteTupleResponse
{
    bool success;
} FgaWriteTupleResponse;

typedef struct FgaDeleteTupleRequest
{
    FgaTuple tuple;
} FgaDeleteTupleRequest;

typedef struct FgaDeleteTupleResponse
{
    bool success;
} FgaDeleteTupleResponse;

/* store 조회 */
typedef struct FgaGetStoreRequest
{
} FgaGetStoreRequest;

typedef struct FgaGetStoreResponse
{
    bool found;
    char name[STORE_NAME_LEN];
} FgaGetStoreResponse;

/* store 생성 */
typedef struct FgaCreateStoreRequest
{
    char name[STORE_NAME_LEN];
} FgaCreateStoreRequest;

typedef struct FgaCreateStoreResponse
{
    char id[STORE_ID_LEN];
    char name[STORE_NAME_LEN];
} FgaCreateStoreResponse;

typedef struct FgaDeleteStoreRequest
{
} FgaDeleteStoreRequest;

typedef struct FgaDeleteStoreResponse
{
    // Empty
} FgaDeleteStoreResponse;

typedef struct FgaRequest
{
    uint64_t request_id; /* request identifier */
    uint16_t type;       /* FgaRequestType */
    // uint16_t reserved;   /* alignment / flags 용 */
    char store_id[STORE_ID_LEN];
    union
    {
        FgaCheckTupleRequest checkTuple;
        FgaWriteTupleRequest writeTuple;
        FgaDeleteTupleRequest deleteTuple;
        FgaGetStoreRequest getStore;
        FgaCreateStoreRequest createStore;
        FgaDeleteStoreRequest deleteStore;
        /* 나중에 추가 예정인 op 들도 여기에 계속 추가 */
    } body;
} FgaRequest;

typedef struct FgaResponse
{
    uint16_t status; /* FgaResponseStatus */
    char error_message[FGA_RESPONSE_ERROR_MESSAGE];

    union
    {
        FgaCheckTupleResponse checkTuple;
        FgaWriteTupleResponse writeTuple;
        FgaDeleteTupleResponse deleteTuple;
        FgaGetStoreResponse getStore;
        FgaCreateStoreResponse createStore;
        FgaDeleteStoreResponse deleteStore;
    } body;
} FgaResponse;

typedef struct FgaPayload
{
    FgaRequest  request;
    FgaResponse response;
} FgaPayload;

#endif // POSTFGA_PAYLOAD_H