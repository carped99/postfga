#ifndef POSTFGA_CHANNEL_SHMEM_H
#define POSTFGA_CHANNEL_SHMEM_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <postgres.h>

    struct FgaChannel;
    typedef struct FgaChannel FgaChannel;

    Size postfga_channel_shmem_size(void);
    void postfga_channel_shmem_init(FgaChannel* ch);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_CHANNEL_SHMEM_H */
