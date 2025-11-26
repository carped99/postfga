#ifndef POSTFGA_CHECK_CHANNEL_SHMEM_H
#define POSTFGA_CHECK_CHANNEL_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <postgres.h>

    Size postfga_check_channel_shmem_size(uint32 slot_count);
    void postfga_check_channel_shmem_init(FgaCheckChannel *ch, uint32 slot_count);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHECK_CHANNEL_SHMEM_H */