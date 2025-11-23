/*-------------------------------------------------------------------------
 *
 * shmem.h
 *    Shared memory lifecycle API for PostFGA extension.
 *
 * Responsibilities:
 *    - Request shared memory size before allocation
 *    - Initialize shared memory after allocation
 *    - Provide read-only access to shared state pointer
 *
 *-------------------------------------------------------------------------
 */
#ifndef POSTFGA_SHMEM_H
#define POSTFGA_SHMEM_H

#include "state.h"

/*
 * C++에서 이 헤더를 include할 때
 * 심볼 이름이 C 방식으로 유지되도록 보장
 */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * postfga_request_shmem
 *
 * _PG_init() 에서 호출해야 하는 함수.
 * RequestAddinShmemSpace / RequestNamedLWLockTranche 를 통해
 * PostgreSQL의 shared memory 예약을 수행한다.
 */
void postfga_request_shmem(void);

/*
 * postfga_startup_shmem    
 *
 * shmem_startup_hook 에서 호출해야 하는 함수.
 * 실제 shared memory 영역을 초기화하고, global shared_state 포인터를 설정한다.
 */
void postfga_startup_shmem(void);


/*
 * get_shared_state
 *
 * 초기화된 shared memory 상태 구조체 포인터를 반환한다.
 * 아직 초기화되지 않았다면 NULL 을 반환할 수 있다.
 */
PostfgaShmemState *postfga_get_shared_state(void);

Cache *postfga_get_cache_state(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_SHMEM_H */
