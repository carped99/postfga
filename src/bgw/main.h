#ifndef POSTFGA_BGW_MAIN_H
#define POSTFGA_BGW_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * postfga_bgw_init - Initialize Background Worker
 */
void postfga_bgw_init(void);

/**
 * postfga_bgw_fini - Finalize Background Worker
 */
void postfga_bgw_fini(void);

/**
 * postfga_bgw_main - Background Worker main loop
 */
PGDLLEXPORT void postfga_bgw_main(Datum arg);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_BGW_MAIN_H */