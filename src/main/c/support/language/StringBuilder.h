#ifndef STRING_BUILDER_HEADER
#define STRING_BUILDER_HEADER

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * The content is always terminated with '\0', but it may also contain '\0'
 * characters, so its length is kept apart.
 */
typedef struct {
	char * content;
	size_t length;
	size_t capacity;
} StringBuilder;

StringBuilder * createStringBuilder();

void destroyStringBuilder(StringBuilder * builder);

void appendCharacters(StringBuilder * builder, const char * characters, const size_t length);

void appendFormat(StringBuilder * builder, const char * format, ...) __attribute__((format(printf, 2, 3)));

void appendFormatArguments(StringBuilder * builder, const char * format, va_list arguments) __attribute__((format(printf, 2, 0)));

void appendString(StringBuilder * builder, const char * string);

#endif
