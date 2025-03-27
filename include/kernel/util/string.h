#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include <string.h>
#include <stdarg.h>

int32 memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void* dest, const void* src, size_t len);
void *memset(void* dest, int32 byte, size_t len);
size_t strlen(const char* s);
int32 strcmp(const char* s1, const char* s2);
char *strcpy(char* dest, const char* src);
char *strchr(const char *p, int32 ch);
char *strtok(char* str, const char* delim);
char *strcat(char *dst, const char *src);
int64 atol(const char* str);
void *memmove(void* dst, const void* src, size_t n);
char *safestrcpy(char* s, const char* t, int32 n);

int32 vsnprintf(char* out, size_t n, const char* s, va_list vl);
int32 snprintf(char* str, size_t size, const char* format, ...);


#endif
