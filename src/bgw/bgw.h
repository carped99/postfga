#ifndef FGA_BGW_H
#define FGA_BGW_H

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

#ifdef __cplusplus
}
#endif

#endif /* FGA_BGW_H */