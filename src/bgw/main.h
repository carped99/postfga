#ifndef FGA_BGW_MAIN_H
#define FGA_BGW_MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * fga_bgw_init - Initialize Background Worker
     */
    void fga_bgw_init(void);

    /**
     * fga_bgw_fini - Finalize Background Worker
     */
    void fga_bgw_fini(void);

    /**
     * postfga_bgw_main - Background Worker main loop
     */
    PGDLLEXPORT void postfga_bgw_main(Datum arg);

#ifdef __cplusplus
}
#endif

#endif /* FGA_BGW_MAIN_H */
