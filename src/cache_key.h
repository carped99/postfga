#ifndef POSTFGA_CACHE_KEY_H
#define POSTFGA_CACHE_KEY_H

#include <postgres.h>

#include <varatt.h>
#include <xxhash.h>

#include "cache.h"

/*-------------------------------------------------------------------------
 * CacheKey helpers
 *-------------------------------------------------------------------------*/
static inline char* append_cache_key_field_text(char* buffer, const text* str)
{
    size_t str_len = str != NULL ? VARSIZE_ANY_EXHDR(str) : 0;
    Assert(str_len <= 125);

    *buffer++ = (uint8_t)str_len;

    if (str_len > 0)
    {
        memcpy(buffer, VARDATA_ANY(str), str_len);
        buffer += str_len;
    }

    return buffer;
}

static inline char* append_cache_key_field(char* buffer, const char* str)
{
    size_t str_len = str != NULL ? strlen(str) : 0;
    Assert(str_len <= 128);

    *buffer++ = (uint8_t)str_len;

    if (str_len > 0)
    {
        memcpy(buffer, str, str_len);
        buffer += str_len;
    }

    return buffer;
}

void postfga_cache_key(FgaAclCacheKey* key,
                       const char* store_id,
                       const char* model_id,
                       const text* object_type,
                       const text* object_id,
                       const text* subject_type,
                       const text* subject_id,
                       const text* relation)
{
    XXH128_hash_t h;
    char buf[1024]; // 각 필드 길이 128 바이트 이하 가정으로 충분
    char* p = buf;

    p = append_cache_key_field(p, store_id);
    p = append_cache_key_field(p, model_id);
    p = append_cache_key_field_text(p, object_type);
    p = append_cache_key_field_text(p, object_id);

    // object_key 생성
    key->object_key = XXH3_64bits(buf, p - buf);

    p = append_cache_key_field_text(p, subject_type);
    p = append_cache_key_field_text(p, subject_id);
    p = append_cache_key_field_text(p, relation);

    h = XXH3_128bits(buf, p - buf);
    key->low = h.low64;
    key->high = h.high64;
}

#endif /* POSTFGA_CACHE_KEY_H */
