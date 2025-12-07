/*-------------------------------------------------------------------------
 *
 * relation.h
 *    Relation bitmap operations for PostFGA extension.
 *
 *-------------------------------------------------------------------------
 */

#ifndef FGA_RELATION_H
#define FGA_RELATION_H

#include <postgres.h>

#include "postfga.h"

#define RELATION_BIT_NOT_FOUND ((uint8)0xFF)

/*
 * Relation bit index mapping
 */
typedef struct RelationBitMapEntry
{
    char relation_name[RELATION_MAX_LEN];
    uint8 bit_index;
} RelationBitMapEntry;

/* -------------------------------------------------------------------------
 * Relation bitmap operations
 * -------------------------------------------------------------------------
 */

/* Get bit index for a relation name */
uint8 get_relation_bit_index(const char* relation_name);

/* Register a relation with a bit index */
void register_relation(const char* relation_name, uint8 bit_index);

/* Initialize relation bitmap from GUC string */
void init_relation_bitmap(const char* relations_str);

#endif /* FGA_RELATION_H */
