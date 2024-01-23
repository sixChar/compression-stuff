#ifndef UTIL_H
#define UTIL_H 1
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint8_t u64;

int diff_file(FILE *fp1, FILE *fp2);
#endif

