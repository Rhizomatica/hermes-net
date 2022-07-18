#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "gz_compress.h"

bool gz_compress(uint8_t *src, size_t srcLen, uint8_t *dst, size_t *dstLen)
{
    z_stream strm;
    strm.zalloc=NULL;
    strm.zfree=NULL;
    strm.opaque=NULL;

    strm.avail_in = srcLen;
    strm.avail_out = *dstLen;
    strm.next_in = (Bytef *)src;
    strm.next_out = (Bytef *)dst;

    int err1=-1, err2=-1;

    err1 = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15 | 16, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    err2 = deflate(&strm, Z_FINISH);

    if (err1 != Z_OK || err2 != Z_STREAM_END)
    {
        deflateEnd(&strm);
        return false;
    }

    *dstLen = strm.total_out;
    deflateEnd(&strm);

    return true;
}


bool gz_decompress(uint8_t *src, size_t srcLen, uint8_t *dst, size_t *dstLen)
{
    z_stream strm;
    strm.zalloc=NULL;
    strm.zfree=NULL;
    strm.opaque=NULL;

    strm.avail_in = srcLen;
    strm.avail_out = *dstLen;
    strm.next_in = (Bytef *)src;
    strm.next_out = (Bytef *)dst;

    int err1=-1, err2=-1;
    err1 = inflateInit2(&strm, MAX_WBITS+16);
    err2 = inflate(&strm, Z_FINISH);

    if (err1 != Z_OK || err2 != Z_STREAM_END)
    {
        inflateEnd(&strm);
        return false;
    }

    *dstLen = strm.total_out;
    inflateEnd(&strm);

    return true;
}
