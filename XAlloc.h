/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_XAlloc
#define HEADER_XAlloc

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_ERR_H
#include <err.h>
#else
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

static inline void
err (int eval, const char *fmt, ...)
{
	char msg[0x100]; // arbitrary
	va_list args;
	va_start(args, fmt);
	if (fmt != NULL) {
		vsnprintf (msg, 0x100, fmt, args);
		// htop is assumed to be the program name
		// we also don't care if strerror requires freeing/is const or
		// not since we're exiting after this anyways
		fprintf(stderr, "htop: %s: %s\n", msg, strerror(errno));
	} else {
		fprintf(stderr, "htop: %s\n", strerror(errno));
	}
	va_end(args);
	_exit (eval);
}
#endif
#include <assert.h>
#include <stdlib.h>

void* xMalloc(size_t size);

void* xCalloc(size_t nmemb, size_t size);

void* xRealloc(void* ptr, size_t size);

#define xSnprintf(fmt, len, ...) do { int _l=len; int _n=snprintf(fmt, _l, __VA_ARGS__); if (!(_n > -1 && _n < _l)) { curs_set(1); endwin(); err(1, NULL); } } while(0)

#undef xStrdup
#undef xStrdup_
#ifdef NDEBUG
# define xStrdup_ xStrdup
#else
# define xStrdup(str_) (assert(str_), xStrdup_(str_))
#endif

#ifndef __has_attribute // Clang's macro
# define __has_attribute(x) 0
#endif
#if (__has_attribute(nonnull) || \
    ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)))
char* xStrdup_(const char* str) __attribute__((nonnull));
#endif // __has_attribute(nonnull) || GNU C 3.3 or later

char* xStrdup_(const char* str);

#endif
