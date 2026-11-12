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

Value * createBooleanValue(const bool boolean);

/** Copies the identifier. */
Value * createEntityValue(const ValueType type, const char * identifier);

Value * createErrorValue();

/** A magnitude goes in base units. */
Value * createIntegerValue(const ValueType type, const int integer);

/** Takes ownership of the string. */
Value * createStringValue(char * string);

/** A deep copy. */
Value * copyValue(const Value * value);

void destroyValue(Value * value);

bool isEntityType(const ValueType type);

bool isMagnitudeType(const ValueType type);

/** As written in Vial (e.g., "integer"): the messages and the runtime use these names. */
const char * valueTypeName(const ValueType type);

const char * valueTypeNameWithArticle(const ValueType type);

/**
 * An entity formats as its label, or as its identifier if the label is NULL.
 * The runtime formats in the same way. Returns a new string.
 */
char * formatValue(const Value * value, const char * label);

#endif
