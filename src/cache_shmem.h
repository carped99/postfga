#ifndef POSTFGA_CACHE_SHMEM_H
#define POSTFGA_CACHE_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "cache.h"

    Size postfga_cache_shmem_size(void);
    void postfga_cache_shmem_init(FgaL2Cache* cache, LWLock *lock);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CACHE_SHMEM_H */
