#ifndef XZ_DECOMPRESSION_H_
#define XZ_DECOMPRESSION_H_

size_t get_uncompressed_size(char *data, size_t file_size);

bool xz_decompress(char *output_buffer, size_t *output_buffer_size, char *input_buffer, size_t input_buffer_size);


#endif // XZ_DECOMPRESSION_H_
