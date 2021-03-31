/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

// like calloc, but with error throwing
void* ecalloc(size_t nmemb, size_t size) {
  void* p;

  if (!(p = calloc(nmemb, size)))
    die("calloc:");
  return p;
}

// throw an error and fucking die
void die(const char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    fputc(' ', stderr);
    perror(NULL);
  } else {
    fputc('\n', stderr);
  }

  exit(1);
}

// count number of ones in binary representation
unsigned int n_ones(unsigned int n) {
  unsigned int c = 0;
  while (n) {
    c += n & 1;
    n >>= 1;
  }
  return c;
}