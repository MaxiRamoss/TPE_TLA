#ifndef VALUE_TYPE_HEADER
#define VALUE_TYPE_HEADER

/**
 * The type of an expression. Magnitudes are exact integers in base units
 * (spec-v3 D11): a DISTANCE in m, a DURATION in s, and a SPEED in m/h. An
 * entity is referenced by its resolved identifier. ERROR is the type of an
 * expression with an error, which never produces more errors.
 *
 * It is shared by the AST (where the semantic analysis annotates the static
 * type of each expression) and the backend.
 */
typedef enum ValueType {
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
} ValueType;

#endif
