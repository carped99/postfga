#ifndef FGA_CHANNEL_SHMEM_H
#define FGA_CHANNEL_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <postgres.h>

#include <storage/lwlock.h>

    struct FgaChannel;
    typedef struct FgaChannel FgaChannel;

    Size fga_channel_shmem_size(void);
    void fga_channel_shmem_init(FgaChannel* ch, LWLock* pool_lock, LWLock* queue_lock);

#ifdef __cplusplus
}
#endif

#endif /* FGA_CHANNEL_SHMEM_H */
