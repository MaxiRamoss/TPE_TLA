#ifndef VALUE_HEADER
#define VALUE_HEADER

#include "../../support/type/ValueType.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Value Value;

/**
 * A value known at compile time. It owns its strings.
 */
struct Value {
	union {
		bool boolean;
		/** The resolved identifier of an entity (e.g., "Rivadavia[3]"). */
		char * identifier;
		/** An integer, or a magnitude in base units. */
		int integer;
		char * string;
	};
	ValueType type;
};

/** Creates a boolean value. */
Value * createBooleanValue(const bool boolean);

/** Creates an entity value of the specified type, copying its identifier. */
Value * createEntityValue(const ValueType type, const char * identifier);

/** Creates a value with an error. */
Value * createErrorValue();

/** Creates an integer value, or a magnitude in base units. */
Value * createIntegerValue(const ValueType type, const int integer);

/** Creates a string value. It takes ownership of the string. */
Value * createStringValue(char * string);

/** Creates a deep copy of a value. */
Value * copyValue(const Value * value);

/** Destroys a value and its resources. */
void destroyValue(Value * value);

/** True if the type is an entity (an intersection, a road, etc.). */
bool isEntityType(const ValueType type);

/** True if the type is a magnitude (a distance, a duration or a speed). */
bool isMagnitudeType(const ValueType type);

/** The name of the type (e.g., "integer"), for messages. */
const char * valueTypeName(const ValueType type);

/** The name of the type with its indefinite article (e.g., "an integer"). */
const char * valueTypeNameWithArticle(const ValueType type);

/**
 * Formats a value for its interpolation in a string (spec-v3 §3.8). An entity
 * formats as its label, or as its identifier if the label is NULL. Returns a
 * new string, allocated in heap-memory.
 */
char * formatValue(const Value * value, const char * label);

#endif
