#pragma once

typedef struct {
    uint32_t id;
    bool     allowed;
    uint32_t error_code;
} CheckResponse;

typedef struct {
    uint32_t id;
    uint32_t error_code;
    uint16_t tuple_count;
    uint16_t success_count;
} WriteResponse;

typedef struct {
    uint32_t id;
    uint32_t error_code;
    uint16_t tuple_count;
    uint16_t success_count;
} DeleteResponse;
