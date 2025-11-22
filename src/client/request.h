#ifndef POSTFGA_CLIENT_REQUEST_H
#define POSTFGA_CLIENT_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#define FGA_TYPE_MAX_LEN 64
#define FGA_ID_MAX_LEN   64
#define FGA_REL_MAX_LEN  64
#define FGA_MAX_TUPLES   16

typedef struct {
    char object_type[FGA_TYPE_MAX_LEN];
    char object_id[FGA_ID_MAX_LEN];
    char subject_type[FGA_TYPE_MAX_LEN];
    char subject_id[FGA_ID_MAX_LEN];
    char relation[FGA_REL_MAX_LEN];
} FgaTuple;

typedef enum {
    FGA_REQ_CHECK  = 1,
    FGA_REQ_WRITE  = 2,
    FGA_REQ_DELETE = 3,
} FgaRequestKind;

typedef struct {
    uint32_t       id;
    FgaRequestKind kind;

    uint16_t       tuple_count;
    FgaTuple       tuples[FGA_MAX_TUPLES];
} Request;

#endif // POSTFGA_CLIENT_REQUEST_H