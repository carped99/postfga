#ifndef POSTFGA_CHANNEL_SHMEM_H
#define POSTFGA_CHANNEL_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <postgres.h>

    struct FgaChannel;
    typedef struct FgaChannel FgaChannel;

    Size postfga_channel_shmem_size(uint32 slot_count);
    void postfga_channel_shmem_init(FgaChannel* ch, uint32 slot_count);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHANNEL_SHMEM_H */
