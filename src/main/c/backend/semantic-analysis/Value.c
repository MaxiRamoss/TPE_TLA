#include "Value.h"

/* PRIVATE FUNCTIONS */

static char * _copyString(const char * string);
static char * _formatDuration(const int seconds);
static char * _formatSpeed(const int metersPerHour);
static char * _printToString(const char * format, ...) __attribute__((format(printf, 1, 2)));

static char * _copyString(const char * string) {
	char * copy = calloc(1 + strlen(string), sizeof(char));
	strcpy(copy, string);
	return copy;
}

/** "1 h 2 min 5 s", omitting the zeros ("0 s" if every part is zero), and "-" if negative. */
static char * _formatDuration(const int seconds) {
	const long long total = seconds < 0 ? -(long long) seconds : seconds;
	const long long hours = total / 3600;
	const long long minutes = (total % 3600) / 60;
	const long long remainder = total % 60;
	char buffer[64] = "";
	int length = 0;
	if (seconds < 0) {
		length += snprintf(buffer + length, sizeof(buffer) - length, "-");
	}
	if (0 < hours) {
		length += snprintf(buffer + length, sizeof(buffer) - length, "%lld h", hours);
	}
	if (0 < minutes) {
		length += snprintf(buffer + length, sizeof(buffer) - length, "%s%lld min", 0 < hours ? " " : "", minutes);
	}
	if (0 < remainder || total == 0) {
		length += snprintf(buffer + length, sizeof(buffer) - length, "%s%lld s", 0 < hours || 0 < minutes ? " " : "", remainder);
	}
	return _copyString(buffer);
}

/** "40 km/h" if it is exact, or else one decimal rounded half away from zero ("4.3 km/h"). */
static char * _formatSpeed(const int metersPerHour) {
	const char * sign = metersPerHour < 0 ? "-" : "";
	const long long magnitude = metersPerHour < 0 ? -(long long) metersPerHour : metersPerHour;
	if (magnitude % 1000 == 0) {
		return _printToString("%s%lld km/h", sign, magnitude / 1000);
	}
	const long long tenths = (magnitude + 50) / 100;
	return _printToString("%s%lld.%lld km/h", sign, tenths / 10, tenths % 10);
}

static char * _printToString(const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	char * string = calloc(1 + length, sizeof(char));
	va_start(arguments, format);
	vsnprintf(string, 1 + length, format, arguments);
	va_end(arguments);
	return string;
}

/* PUBLIC FUNCTIONS */

Value * createBooleanValue(const bool boolean) {
	Value * value = calloc(1, sizeof(Value));
	value->type = BOOLEAN_VALUE_TYPE;
	value->boolean = boolean;
	return value;
}

Value * createEntityValue(const ValueType type, const char * identifier) {
	Value * value = calloc(1, sizeof(Value));
	value->type = type;
	value->identifier = _copyString(identifier);
	return value;
}

Value * createErrorValue() {
	Value * value = calloc(1, sizeof(Value));
	value->type = ERROR_VALUE_TYPE;
	return value;
}

Value * createIntegerValue(const ValueType type, const int integer) {
	Value * value = calloc(1, sizeof(Value));
	value->type = type;
	value->integer = integer;
	return value;
}

Value * createStringValue(char * string) {
	Value * value = calloc(1, sizeof(Value));
	value->type = STRING_VALUE_TYPE;
	value->string = string;
	return value;
}

Value * copyValue(const Value * value) {
	if (value->type == STRING_VALUE_TYPE) {
		return createStringValue(_copyString(value->string));
	}
	if (isEntityType(value->type)) {
		return createEntityValue(value->type, value->identifier);
	}
	Value * copy = calloc(1, sizeof(Value));
	*copy = *value;
	return copy;
}

void destroyValue(Value * value) {
	if (value != NULL) {
		if (value->type == STRING_VALUE_TYPE) {
			free(value->string);
		}
		else if (isEntityType(value->type)) {
			free(value->identifier);
		}
		free(value);
	}
}

bool isEntityType(const ValueType type) {
	switch (type) {
		case FLOW_VALUE_TYPE:
		case INTERSECTION_VALUE_TYPE:
		case LIGHT_VALUE_TYPE:
		case METRIC_VALUE_TYPE:
		case POLICY_VALUE_TYPE:
		case ROAD_VALUE_TYPE:
		case ROUTE_VALUE_TYPE:
			return true;
		default:
			return false;
	}
}

bool isMagnitudeType(const ValueType type) {
	return type == DISTANCE_VALUE_TYPE || type == DURATION_VALUE_TYPE || type == SPEED_VALUE_TYPE;
}

const char * valueTypeName(const ValueType type) {
	switch (type) {
		case BOOLEAN_VALUE_TYPE: return "boolean";
		case DISTANCE_VALUE_TYPE: return "distance";
		case DURATION_VALUE_TYPE: return "duration";
		case FLOW_VALUE_TYPE: return "flow";
		case INTEGER_VALUE_TYPE: return "integer";
		case INTERSECTION_VALUE_TYPE: return "intersection";
		case LIGHT_VALUE_TYPE: return "light";
		case METRIC_VALUE_TYPE: return "metric";
		case POLICY_VALUE_TYPE: return "policy";
		case ROAD_VALUE_TYPE: return "road";
		case ROUTE_VALUE_TYPE: return "route";
		case SPEED_VALUE_TYPE: return "speed";
		case STRING_VALUE_TYPE: return "string";
		default: return "error";
	}
}

const char * valueTypeNameWithArticle(const ValueType type) {
	switch (type) {
		case BOOLEAN_VALUE_TYPE: return "a boolean";
		case DISTANCE_VALUE_TYPE: return "a distance";
		case DURATION_VALUE_TYPE: return "a duration";
		case FLOW_VALUE_TYPE: return "a flow";
		case INTEGER_VALUE_TYPE: return "an integer";
		case INTERSECTION_VALUE_TYPE: return "an intersection";
		case LIGHT_VALUE_TYPE: return "a light";
		case METRIC_VALUE_TYPE: return "a metric";
		case POLICY_VALUE_TYPE: return "a policy";
		case ROAD_VALUE_TYPE: return "a road";
		case ROUTE_VALUE_TYPE: return "a route";
		case SPEED_VALUE_TYPE: return "a speed";
		case STRING_VALUE_TYPE: return "a string";
		default: return "an error";
	}
}

char * formatValue(const Value * value, const char * label) {
	switch (value->type) {
		case BOOLEAN_VALUE_TYPE:
			return _copyString(value->boolean ? "true" : "false");
		case DISTANCE_VALUE_TYPE:
			return _printToString("%d m", value->integer);
		case DURATION_VALUE_TYPE:
			return _formatDuration(value->integer);
		case INTEGER_VALUE_TYPE:
			return _printToString("%d", value->integer);
		case SPEED_VALUE_TYPE:
			return _formatSpeed(value->integer);
		case STRING_VALUE_TYPE:
			return _copyString(value->string);
		case ERROR_VALUE_TYPE:
			return _copyString("<error>");
		default:
			return _copyString(label != NULL ? label : value->identifier);
	}
}
