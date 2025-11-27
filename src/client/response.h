#ifndef POSTFGA_CLIENT_RESPONSE_H
#define POSTFGA_CLIENT_RESPONSE_H

typedef struct
{
    uint32_t id;
    bool allowed;
    uint32_t status_code;
    char error_message[256];
} CheckResponse;

typedef struct
{
    uint32_t id;
    uint32_t status_code;
    uint16_t tuple_count;
    uint16_t success_count;
    char error_message[256];
} WriteResponse;

typedef struct
{
    uint32_t id;
    uint32_t status_code;
    uint16_t tuple_count;
    uint16_t success_count;
    char error_message[256];
} DeleteResponse;

#endif // POSTFGA_CLIENT_RESPONSE_H
