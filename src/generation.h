/*-------------------------------------------------------------------------
 *
 * generation.h
 *    Generation tracking for cache invalidation.
 *
 *-------------------------------------------------------------------------
 */

#ifndef POSTFGA_GENERATION_H
#define POSTFGA_GENERATION_H

#include <postgres.h>

/* -------------------------------------------------------------------------
 * Generation tracking
 * -------------------------------------------------------------------------
 */

/* Get generation for a scope */
uint64 get_generation(const char *scope_key);

/* Increment generation for a scope */
void increment_generation(const char *scope_key);

/* Build a scope key from type and id components */
void build_scope_key(char *dest, size_t dest_size,
                    const char *type, const char *id);

#endif /* POSTFGA_GENERATION_H */