#ifndef POSTFGA_CLIENT_H
#define POSTFGA_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque client handle */
typedef struct GrpcClient GrpcClient;

/* Request/Response structs (BGW/FDW에서 사용) */
typedef struct GrpcCheckRequest {
    const char *store_id;
    const char *authorization_model_id;  /* NULL 또는 빈 문자열이면 미설정 */
    const char *object_type;
    const char *object_id;
    const char *relation;
    const char *subject_type;
    const char *subject_id;
} GrpcCheckRequest;

typedef struct CheckResponse {
    bool     allowed;
    uint32_t error_code;                /* 0 이면 성공, 그 외 gRPC/내부 에러 코드 */
    char     error_message[256];        /* '\0' 로 끝나는 문자열 */
} CheckResponse;

/* Write request parameters */
typedef struct GrpcWriteRequest {
    const char *store_id;
    const char *authorization_model_id;
    const char *object_type;
    const char *object_id;
    const char *relation;
    const char *subject_type;
    const char *subject_id;
} GrpcWriteRequest;

typedef struct WriteResponse {
    bool     success;
    uint32_t error_code;
    char     error_message[256];
} WriteResponse;

typedef void (*CheckCallback)(const CheckResponse *response, void *user_data);
typedef void (*WriteCallback)(const WriteResponse *response, void *user_data);

/* Lifecycle */
GrpcClient *postfga_client_init(const char *endpoint);
void        postfga_client_fini(GrpcClient *client);

/* Sync / Async Check */
bool postfga_client_check_sync(GrpcClient *client,
                            const GrpcCheckRequest *request,
                            CheckResponse *response);

bool postfga_client_check_async(GrpcClient *client,
                             const GrpcCheckRequest *request,
                             CheckCallback callback,
                             void *user_data);

/* Sync / Async Write */
bool postfga_client_write_sync(GrpcClient *client,
                            const GrpcWriteRequest *request,
                            WriteResponse *response);

bool postfga_client_write_async(GrpcClient *client,
                             const GrpcWriteRequest *request,
                             WriteCallback callback,
                             void *user_data);

/* Health check / 기타 헬퍼 */
bool postfga_client_is_healthy(GrpcClient *client);
int  postfga_client_poll(GrpcClient *client, int timeout_ms); /* 현재 구현은 no-op */

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CLIENT_H */