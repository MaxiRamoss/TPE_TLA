#ifndef VALUE_TYPE_HEADER
#define VALUE_TYPE_HEADER

/**
 * Magnitudes are exact integers in base units: m, s and m/h. ERROR is the
 * type of an expression with an error, which never produces more errors.
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
