/* uuxcomp: xz utils
 * Copyright (C) 2022 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lzma.h>

// https://stackoverflow.com/questions/2171775/how-to-get-the-uncompressed-size-of-an-lzma2-file-xz-liblzma#tab-top
size_t get_uncompressed_size(uint8_t *data, size_t file_size)
{
	lzma_stream_flags stream_flags;

	// 12 is the size of the footer per the file-spec...
	const uint8_t *footer_ptr = data + file_size - LZMA_STREAM_HEADER_SIZE;

	// Decode the footer, so we have the backward_size pointing to the index
	lzma_stream_footer_decode(&stream_flags, (const uint8_t *)footer_ptr);
	/// This is the index pointer, where the size is ultimately stored...
	const uint8_t *index_ptr = footer_ptr - stream_flags.backward_size;

	lzma_index *index;
	uint64_t memlimit = 65536;
	size_t in_pos = 0;
	// decode the index we calculated
	lzma_index_buffer_decode(&index, &memlimit, NULL, index_ptr, &in_pos, footer_ptr - index_ptr);
	// Just make sure the whole index was decoded, otherwise, we might be
	// dealing with something utterly corrupt
	if (in_pos != stream_flags.backward_size)
	{
		lzma_index_end(index, NULL);
		return -1;
	}
	// Finally get the size
	lzma_vli uSize = lzma_index_uncompressed_size(index);
	lzma_index_end(index, NULL);
	return (size_t) uSize;
}


static bool
init_decoder(lzma_stream *strm)
{
	// Initialize a .xz decoder. The decoder supports a memory usage limit
	// and a set of flags.
	//
	// The memory usage of the decompressor depends on the settings used
	// to compress a .xz file. It can vary from less than a megabyte to
	// a few gigabytes, but in practice (at least for now) it rarely
	// exceeds 65 MiB because that's how much memory is required to
	// decompress files created with "xz -9". Settings requiring more
	// memory take extra effort to use and don't (at least for now)
	// provide significantly better compression in most cases.
	//
	// Memory usage limit is useful if it is important that the
	// decompressor won't consume gigabytes of memory. The need
	// for limiting depends on the application. In this example,
	// no memory usage limiting is used. This is done by setting
	// the limit to UINT64_MAX.
	//
	// The .xz format allows concatenating compressed files as is:
	//
	//     echo foo | xz > foobar.xz
	//     echo bar | xz >> foobar.xz
	//
	// When decompressing normal standalone .xz files, LZMA_CONCATENATED
	// should always be used to support decompression of concatenated
	// .xz files. If LZMA_CONCATENATED isn't used, the decoder will stop
	// after the first .xz stream. This can be useful when .xz data has
	// been embedded inside another file format.
	//
	// Flags other than LZMA_CONCATENATED are supported too, and can
	// be combined with bitwise-or. See lzma/container.h
	// (src/liblzma/api/lzma/container.h in the source package or e.g.
	// /usr/include/lzma/container.h depending on the install prefix)
	// for details.
	lzma_ret ret = lzma_stream_decoder(
			strm, UINT64_MAX, 0);

	// Return successfully if the initialization went fine.
	if (ret == LZMA_OK)
		return true;

	// Something went wrong. The possible errors are documented in
	// lzma/container.h (src/liblzma/api/lzma/container.h in the source
	// package or e.g. /usr/include/lzma/container.h depending on the
	// install prefix).
	//
	// Note that LZMA_MEMLIMIT_ERROR is never possible here. If you
	// specify a very tiny limit, the error will be delayed until
	// the first headers have been parsed by a call to lzma_code().
	const char *msg;
	switch (ret) {
	case LZMA_MEM_ERROR:
		msg = "Memory allocation failed";
		break;

	case LZMA_OPTIONS_ERROR:
		msg = "Unsupported decompressor flags";
		break;

	default:
		// This is most likely LZMA_PROG_ERROR indicating a bug in
		// this program or in liblzma. It is inconvenient to have a
		// separate error message for errors that should be impossible
		// to occur, but knowing the error code is important for
		// debugging. That's why it is good to print the error code
		// at least when there is no good error message to show.
		msg = "Unknown error, possibly a bug";
		break;
	}

	fprintf(stderr, "Error initializing the decoder: %s (error code %u)\n",
			msg, ret);
	return false;
}


static bool
decompress(lzma_stream *strm, uint8_t *output_buffer, size_t *output_buffer_size, uint8_t *input_buffer, size_t input_buffer_size)
{
	// When LZMA_CONCATENATED flag was used when initializing the decoder,
	// we need to tell lzma_code() when there will be no more input.
	// This is done by setting action to LZMA_FINISH instead of LZMA_RUN
	// in the same way as it is done when encoding.
	//
	// When LZMA_CONCATENATED isn't used, there is no need to use
	// LZMA_FINISH to tell when all the input has been read, but it
	// is still OK to use it if you want. When LZMA_CONCATENATED isn't
	// used, the decoder will stop after the first .xz stream. In that
	// case some unused data may be left in strm->next_in.
	lzma_action action = LZMA_RUN;

	strm->next_in = input_buffer;
	strm->avail_in = input_buffer_size;

	strm->next_out = output_buffer;
	strm->avail_out = *output_buffer_size;

	action = LZMA_FINISH;

	while (true) {
		lzma_ret ret = lzma_code(strm, action);

		if (ret == LZMA_STREAM_END)
		{
			*output_buffer_size -= strm->avail_out;
		}

		if (ret != LZMA_OK) {
			// Once everything has been decoded successfully, the
			// return value of lzma_code() will be LZMA_STREAM_END.
			//
			// It is important to check for LZMA_STREAM_END. Do not
			// assume that getting ret != LZMA_OK would mean that
			// everything has gone well or that when you aren't
			// getting more output it must have successfully
			// decoded everything.
			if (ret == LZMA_STREAM_END)
				return true;

			// It's not LZMA_OK nor LZMA_STREAM_END,
			// so it must be an error code. See lzma/base.h
			// (src/liblzma/api/lzma/base.h in the source package
			// or e.g. /usr/include/lzma/base.h depending on the
			// install prefix) for the list and documentation of
			// possible values. Many values listen in lzma_ret
			// enumeration aren't possible in this example, but
			// can be made possible by enabling memory usage limit
			// or adding flags to the decoder initialization.
			const char *msg;
			switch (ret) {
			case LZMA_MEM_ERROR:
				msg = "Memory allocation failed";
				break;

			case LZMA_FORMAT_ERROR:
				// .xz magic bytes weren't found.
				msg = "The input is not in the .xz format";
				break;

			case LZMA_OPTIONS_ERROR:
				// For example, the headers specify a filter
				// that isn't supported by this liblzma
				// version (or it hasn't been enabled when
				// building liblzma, but no-one sane does
				// that unless building liblzma for an
				// embedded system). Upgrading to a newer
				// liblzma might help.
				//
				// Note that it is unlikely that the file has
				// accidentally became corrupt if you get this
				// error. The integrity of the .xz headers is
				// always verified with a CRC32, so
				// unintentionally corrupt files can be
				// distinguished from unsupported files.
				msg = "Unsupported compression options";
				break;

			case LZMA_DATA_ERROR:
				msg = "Compressed file is corrupt";
				break;

			case LZMA_BUF_ERROR:
				// Typically this error means that a valid
				// file has got truncated, but it might also
				// be a damaged part in the file that makes
				// the decoder think the file is truncated.
				// If you prefer, you can use the same error
				// message for this as for LZMA_DATA_ERROR.
				msg = "Compressed file is truncated or "
						"otherwise corrupt";
				break;

			default:
				// This is most likely LZMA_PROG_ERROR.
				msg = "Unknown error, possibly a bug";
				break;
			}

			fprintf(stderr, "Decoder error: "
					"%s (error code %u)\n",
					msg, ret);
			return false;
		}
	}
}



bool xz_decompress(uint8_t *output_buffer, size_t *output_buffer_size, uint8_t *input_buffer, size_t input_buffer_size)
{
	lzma_stream strm = LZMA_STREAM_INIT;

	bool success;

	if (!init_decoder(&strm))
	{
		// Decoder initialization failed. There's no point
		// to retry it so we need to exit.
		return EXIT_FAILURE;
	}

	success = decompress(&strm, output_buffer, output_buffer_size, input_buffer, input_buffer_size);

	// success = decompress(&strm, argv[i], infile, stdout);

	// Free the memory allocated for the decoder. This only needs to be
	// done after the last file.
	lzma_end(&strm);

	return success;
}
