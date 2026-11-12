#ifndef STRING_BUILDER_HEADER
#define STRING_BUILDER_HEADER

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * A growing buffer of characters, in heap-memory. Its content is always
 * terminated with '\0', but it may also contain '\0' characters, so its
 * length is kept apart.
 */
typedef struct {
	char * content;
	size_t length;
	size_t capacity;
} StringBuilder;

/**
 * Creates an empty string builder.
 */
StringBuilder * createStringBuilder();

/**
 * Destroys a string builder and its content.
 */
void destroyStringBuilder(StringBuilder * builder);

/**
 * Appends a sequence of characters of the specified length.
 */
void appendCharacters(StringBuilder * builder, const char * characters, const size_t length);

/**
 * Appends a formatted string, with "printf" semantics.
 */
void appendFormat(StringBuilder * builder, const char * format, ...) __attribute__((format(printf, 2, 3)));

/**
 * Analog to "appendFormat", but with a list of arguments (as "vprintf").
 */
void appendFormatArguments(StringBuilder * builder, const char * format, va_list arguments) __attribute__((format(printf, 2, 0)));

/**
 * Appends a string.
 */
void appendString(StringBuilder * builder, const char * string);

#endif
