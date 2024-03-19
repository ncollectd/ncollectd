#ifndef __CSNAPPY_H__
#define __CSNAPPY_H__
/*
File modified for the Linux Kernel by
Zeev Tarantov <zeev.tarantov@gmail.com>
*/
#ifdef __cplusplus
extern "C" {
#endif

#define CSNAPPY_VERSION	5

#define CSNAPPY_WORKMEM_BYTES_POWER_OF_TWO 16
#define CSNAPPY_WORKMEM_BYTES (1 << CSNAPPY_WORKMEM_BYTES_POWER_OF_TWO)

#ifndef __GNUC__
#define __attribute__(x) /*NOTHING*/
#endif

#if defined(__SUNPRO_C) || defined(_AIX)
# include <inttypes.h>
#else
# include <stdint.h>
#endif

/*
 * Returns the maximal size of the compressed representation of
 * input data that is "source_len" bytes in length;
 */
uint32_t
csnappy_max_compressed_length(uint32_t source_len) __attribute__((const));

/*
 * Flat array compression that does not emit the "uncompressed length"
 * prefix. Compresses "input" array to the "output" array.
 *
 * REQUIRES: "input" is at most 32KiB long.
 * REQUIRES: "output" points to an array of memory that is at least
 * "csnappy_max_compressed_length(input_length)" in size.
 * REQUIRES: working_memory has (1 << workmem_bytes_power_of_two) bytes.
 * REQUIRES: 9 <= workmem_bytes_power_of_two <= 15.
 *
 * Returns an "end" pointer into "output" buffer.
 * "end - output" is the compressed size of "input".
 */
char*
csnappy_compress_fragment(
	const char *input,
	const uint32_t input_length,
	char *output,
	void *working_memory,
	const int workmem_bytes_power_of_two);

/*
 * REQUIRES: "compressed" must point to an area of memory that is at
 * least "csnappy_max_compressed_length(input_length)" bytes in length.
 * REQUIRES: working_memory has (1 << workmem_bytes_power_of_two) bytes.
 * REQUIRES: 9 <= workmem_bytes_power_of_two <= 15.
 *
 * Takes the data stored in "input[0..input_length-1]" and stores
 * it in the array pointed to by "compressed".
 *
 * "*out_compressed_length" is set to the length of the compressed output.
 */
void
csnappy_compress(
	const char *input,
	uint32_t input_length,
	char *compressed,
	uint32_t *out_compressed_length,
	void *working_memory,
	const int workmem_bytes_power_of_two);

/*
 * Return values (< 0 = Error)
 */
#define CSNAPPY_E_OK			0
#define CSNAPPY_E_HEADER_BAD		(-1)
#define CSNAPPY_E_OUTPUT_INSUF		(-2)
#define CSNAPPY_E_OUTPUT_OVERRUN	(-3)
#define CSNAPPY_E_INPUT_NOT_CONSUMED	(-4)
#define CSNAPPY_E_DATA_MALFORMED	(-5)

#ifdef __cplusplus
}
#endif

#endif
