/* SPDX-License-Identifier: Public-Domain                                                          */
/* SPDX-FileContributor: Alexander Peslyak, better known as Solar Designer <solar at openwall.com> */
/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 */
#pragma once

/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int MD5_u32plus;

typedef struct {
    MD5_u32plus lo, hi;
    MD5_u32plus a, b, c, d;
    unsigned char buffer[64];
    MD5_u32plus block[16];
} MD5_CTX;

extern void MD5_Init(MD5_CTX *ctx);

extern void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size);

extern void MD5_Final(unsigned char *result, MD5_CTX *ctx);
