#ifndef POSTFGA_channel_SHMEM_H
#define POSTFGA_channel_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

    Size postfga_channel_shmem_size(uint32 slot_count);
    void postfga_channel_shmem_init(FgaChannel *ch, uint32 slot_count);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_channel_SHMEM_H */