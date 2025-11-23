#ifndef POSTFGA_REQUEST_SLOT_H
#define POSTFGA_REQUEST_SLOT_H

/*
 * 슬롯 헤더: 큐에 들어가는 "메시지 헤더".
 *
 * 실데이터(payload)는 req_off/resp_off가 가리키는 shared arena 안에 있고,
 * 이 헤더는 그것을 어떻게 해석해야 하는지에 대한 최소 메타데이터만 가진다.
 */
typedef struct FgaRequestSlot
{
    uint64          request_id;   /* 이 요청 배치의 고유 ID */
    pid_t           backend_pid;  /* 보낸 프로세스 (BackendPidGetProc 용) */

    volatile uint32 state;        /* PostfgaRequestState as uint32 */
    uint16          kind;         /* PostfgaRequestKind as uint16 */
    uint16          flags;        /* reserved: oneway, priority 등 향후용 */

    uint32          req_off;      /* 요청 payload의 shared arena offset */
    uint32          req_size;     /* 요청 payload의 전체 크기 (bytes) */

    uint32          resp_off;     /* 응답 payload offset (없으면 0) */
    uint32          resp_size;    /* 응답 payload 버퍼 크기 (capacity) */
} FgaRequestSlot;

#endif // POSTFGA_REQUEST_SLOT_H