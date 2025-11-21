#ifndef POSTFGA_BGW_H
#define POSTFGA_BGW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <postgres.h>
#include <miscadmin.h>
#include <postmaster/bgworker.h>

void postfga_bgw_init(void);
void postfga_bgw_fini(void);
void postfga_bgw_main(Datum arg);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_BGW_H */
