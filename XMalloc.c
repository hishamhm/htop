/* $OpenBSD: xmalloc.c,v 1.32 2015/04/24 01:36:01 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Versions of malloc and friends that check their results, and never return
 * failure (they call fatal if they encounter an error).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "XMalloc.h"

#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*{
#include <unistd.h>
}*/

void* xmalloc(size_t size) {

	void *ptr;

	if (size == 0)
		errx(1, "xmalloc: zero size");
	ptr = malloc(size);
	if (ptr == NULL)
		errx(1, "xmalloc: out of memory (allocating %zu bytes)", size);
	return ptr;
}

void* xcalloc(size_t nmemb, size_t size) {

	void *ptr;

	if (size == 0 || nmemb == 0)
		errx(1, "xcalloc: zero size");
	if (SIZE_MAX / nmemb < size)
		errx(1, "xcalloc: nmemb * size > SIZE_MAX");
	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		errx(1, "xcalloc: out of memory (allocating %zu bytes)",
		    size * nmemb);
	return ptr;
}

void* xstrdup(const char *str) {

	char *cp;

	cp = strdup(str);
	if (cp == NULL)
		errx(1, "xstrdup: out of memory");
	return cp;
}
