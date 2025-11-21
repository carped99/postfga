#ifndef POSTFGA_BGW_MAIN_H
#define POSTFGA_BGW_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

void postfga_bgw_init(void);
void postfga_bgw_fini(void);
void postfga_bgw_main(Datum arg);

#ifdef __cplusplus
}
#endif

#endif /* POSTFGA_BGW_MAIN_H */