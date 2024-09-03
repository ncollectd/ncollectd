#pragma once

#ifdef HAVE_CONFIG_H                                                                                
#    include "config.h"                                                                             
#endif

#include <inttypes.h>

#ifdef HAVE_SYS_TYPES_H
#    include <sys/types.h>
#endif

#include <stdint.h>
#include <string.h>

/*  The below was retrieved from
 *  http://www.openbsd.org/cgi-bin/cvsweb/~checkout~/src/sys/crypto/md5.h?rev=1.1
 *  With the following changes: uint64_t => uint32_t[2]
 *  Commented out #include <sys/cdefs.h>
 *  Commented out the __BEGIN and __END _DECLS, and the __attributes.
 */

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 */

#define	MD5_BLOCK_LENGTH		64
#define	MD5_DIGEST_LENGTH		16

typedef struct MD5Context {
	uint32_t state[4];			//!< State.
	uint32_t count[2];			//!< Number of bits, mod 2^64.
	uint8_t buffer[MD5_BLOCK_LENGTH];	//!< Input buffer.
} MD5_CTX;

void	MD5Init(MD5_CTX *);
void	MD5Update(MD5_CTX *, uint8_t const *, size_t);
void	MD5Final(uint8_t [MD5_DIGEST_LENGTH], MD5_CTX *);
void	MD5Transform(uint32_t [4], uint8_t const [MD5_BLOCK_LENGTH]);
