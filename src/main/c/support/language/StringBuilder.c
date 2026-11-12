#include "StringBuilder.h"

/* PRIVATE FUNCTIONS */

static void _ensureCapacity(StringBuilder * builder, const size_t length);

static const size_t _initialCapacity = 256;

static void _ensureCapacity(StringBuilder * builder, const size_t length) {
	const size_t required = builder->length + length + 1;
	if (required <= builder->capacity) {
		return;
	}
	size_t capacity = builder->capacity;
	while (capacity < required) {
		capacity *= 2;
	}
	builder->content = realloc(builder->content, capacity);
	builder->capacity = capacity;
}

/* PUBLIC FUNCTIONS */

StringBuilder * createStringBuilder() {
	StringBuilder * builder = calloc(1, sizeof(StringBuilder));
	builder->content = calloc(_initialCapacity, sizeof(char));
	builder->capacity = _initialCapacity;
	builder->length = 0;
	return builder;
}

void destroyStringBuilder(StringBuilder * builder) {
	if (builder != NULL) {
		free(builder->content);
		free(builder);
	}
}

void appendCharacters(StringBuilder * builder, const char * characters, const size_t length) {
	_ensureCapacity(builder, length);
	memcpy(builder->content + builder->length, characters, length);
	builder->length += length;
	builder->content[builder->length] = '\0';
}

void appendFormat(StringBuilder * builder, const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	appendFormatArguments(builder, format, arguments);
	va_end(arguments);
}

void appendFormatArguments(StringBuilder * builder, const char * format, va_list arguments) {
	va_list copy;
	va_copy(copy, arguments);
	const int length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	if (length <= 0) {
		return;
	}
	_ensureCapacity(builder, length);
	vsnprintf(builder->content + builder->length, length + 1, format, arguments);
	builder->length += length;
}

void appendString(StringBuilder * builder, const char * string) {
	appendCharacters(builder, string, strlen(string));
}
