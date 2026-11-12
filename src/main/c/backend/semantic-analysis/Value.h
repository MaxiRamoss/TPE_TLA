#ifndef VALUE_HEADER
#define VALUE_HEADER

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ValueType ValueType;
typedef struct Value Value;

/**
 * The type of an expression. Magnitudes are exact integers in base units
 * (spec-v3 D11): a DISTANCE in m, a DURATION in s, and a SPEED in m/h. An
 * entity is referenced by its resolved identifier. ERROR is the type of an
 * expression with an error, which never produces more errors.
 */
enum ValueType {
	BOOLEAN_VALUE_TYPE,
	DISTANCE_VALUE_TYPE,
	DURATION_VALUE_TYPE,
	ERROR_VALUE_TYPE,
	FLOW_VALUE_TYPE,
	INTEGER_VALUE_TYPE,
	INTERSECTION_VALUE_TYPE,
	LIGHT_VALUE_TYPE,
	METRIC_VALUE_TYPE,
	POLICY_VALUE_TYPE,
	ROAD_VALUE_TYPE,
	ROUTE_VALUE_TYPE,
	SPEED_VALUE_TYPE,
	STRING_VALUE_TYPE
};

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
