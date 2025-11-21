#ifndef POSTFGA_GUC_H
#define POSTFGA_GUC_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

void postfga_guc_init(void);
void postfga_guc_fini(void);

PostfgaConfig *postfga_get_config(void);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_GUC_H */