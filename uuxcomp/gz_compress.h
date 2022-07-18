#ifndef GZ_COMPRESS_H_
#define GZ_COMPRESS_H_


bool gz_decompress(uint8_t *src, size_t srcLen, uint8_t *dst, size_t *dstLen);
bool gz_compress(uint8_t *src, size_t srcLen, uint8_t *dst, size_t *dstLen);

#endif // GZ_COMPRESS_H_
