// SPDX-License-Identifier: GPL-2.0-only OR ISC
// SPDX-FileCopyrightText: Copyright (c) 2008, 2010, 2011, 2016 Otto Moerbeek
// SPDX-FileCopyrightText: Copyright (c) 2012 Matthew Dempsky
// SPDX-FileCopyrightText: Copyright (c) 2008 Damien Miller
// SPDX-FileCopyrightText: Copyright (c) 2000 Poul-Henning Kamp
// SPDX-FileContributor: Otto Moerbeek <otto at drijf.net>
// SPDX-FileContributor: Matthew Dempsky <matthew at openbsd.org>
// SPDX-FileContributor: Damien Miller <djm at openbsd.org>
// SPDX-FileContributor: Poul-Henning Kamp <phk at FreeBSD.org>

#include <string.h>
#include <stdlib.h>

void
freezero(void *ptr, size_t sz)
{
	/* This is legal. */
	if (ptr == NULL)
		return;

	explicit_bzero(ptr, sz);
	free(ptr);
}
