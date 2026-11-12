#include "Generator.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/**
 * FAILED if the model has something that the semantic analysis should have
 * rejected: then nothing is emitted.
 */
static CompilationStatus _status = SUCCEEDED;

/** Shutdown module's internal state. */
void _shutdownGeneratorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Generator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeGeneratorModule() {
	_logger = createLogger("Generator");
	return _shutdownGeneratorModule;
}

/* PRIVATE FUNCTIONS */

typedef enum Marker Marker;

/** Each marker must appear in the viewer exactly once. */
enum Marker {
	CONFIG_MARKER,
	LOGIC_MARKER,
	RUNTIME_MARKER,
	MARKER_COUNT
};

static const char * const _markers[MARKER_COUNT] = {
	"<!-- VIAL:CONFIG -->",
	"<!-- VIAL:LOGIC -->",
	"<!-- VIAL:RUNTIME -->"
};

/** Relative to the working directory, which is the root of the repository for the scripts. */
static const char * _defaultRuntimePath = "src/main/runtime";

static const char * _indentation = "  ";

/** A policy has no index, so in a valid program its body is analyzed only once. */
static const unsigned int _policyInstance = 0;

static char * _assetPath(const char * directory, const char * name);
static const char * _entityClass(const ValueType type);
static const char * _formatType(const ValueType type);
static void _generateApplication(StringBuilder * output, const PolicyApplication * application);
static void _generateBlock(StringBuilder * output, const Block * block, const unsigned int level);
static void _generateCall(StringBuilder * output, const Call * call, const unsigned int instance);
static void _generateConfig(StringBuilder * output, const Model * model);
static void _generateExpression(StringBuilder * output, const Expression * expression, const unsigned int instance);
static void _generateFactor(StringBuilder * output, const Factor * factor, const unsigned int instance);
static void _generateFlow(StringBuilder * output, const Flow * flow);
static bool _generateHtml(StringBuilder * html, const StringBuilder * viewer, const StringBuilder * runtime, const Model * model, const char * viewerPath);
static void _generateIdentity(StringBuilder * output, const char * id, const char * label);
static void _generateIf(StringBuilder * output, const IfStatement * ifStatement, const unsigned int level);
static void _generateInteger(StringBuilder * output, const long long integer);
static void _generateInterpolation(StringBuilder * output, const Expression * expression, const unsigned int instance);
static void _generateIntersection(StringBuilder * output, const Intersection * intersection);
static void _generateLight(StringBuilder * output, const Light * light);
static void _generateLogic(StringBuilder * output, const Model * model);
static void _generateMarker(StringBuilder * html, const Marker marker, const StringBuilder * runtime, const Model * model);
static void _generateMetric(StringBuilder * output, const Metric * metric);
static void _generateMetricFunction(StringBuilder * output, const Metric * metric);
static void _generatePhase(StringBuilder * output, const Phase * phase);
static void _generatePolicy(StringBuilder * output, const Policy * policy);
static void _generateQuantity(StringBuilder * output, const Quantity * quantity);
static void _generateReference(StringBuilder * output, const Reference * reference, const unsigned int instance);
static void _generateRoad(StringBuilder * output, const Road * road);
static void _generateRoute(StringBuilder * output, const Route * route);
static void _generateStatement(StringBuilder * output, const Statement * statement, const unsigned int level);
static void _generateString(StringBuilder * output, const char * string);
static void _generateStringLiteral(StringBuilder * output, const StringLiteral * stringLiteral, const unsigned int instance, const bool grouped);
static void _generateValue(StringBuilder * output, const Value * value);
static void _indent(StringBuilder * output, const unsigned int level);
static int _markerOf(const char * line, const size_t length);
static const char * _missingEntity();
static const char * _operator(const ExpressionType type);
static void _output(StringBuilder * output, const unsigned int level, const char * const format, ...) __attribute__((format(printf, 3, 4)));
static StringBuilder * _readAsset(const char * path);
static void _reportUnexpected(const char * what);
static long long _scaleOf(const Unit unit);
static void _separate(StringBuilder * output, const unsigned int k, const unsigned int count);
static bool _write(const StringBuilder * html);

/** A valid model has no unresolved references; if it had one, the generation fails. */
#define ID_OF(entity) ((entity) != NULL ? (entity)->id : _missingEntity())

static char * _assetPath(const char * directory, const char * name) {
	const size_t length = strlen(directory);
	if (length == 0) {
		return concatenate(1, name);
	}
	return concatenate(3, directory, directory[length - 1] == '/' ? "" : "/", name);
}

static const char * _entityClass(const ValueType type) {
	return isEntityType(type) ? valueTypeName(type) : NULL;
}

static const char * _formatType(const ValueType type) {
	switch (type) {
		case BOOLEAN_VALUE_TYPE:
		case DISTANCE_VALUE_TYPE:
		case DURATION_VALUE_TYPE:
		case INTEGER_VALUE_TYPE:
		case SPEED_VALUE_TYPE:
		case STRING_VALUE_TYPE:
			return valueTypeName(type);
		default:
			return NULL;
	}
}

static void _indent(StringBuilder * output, const unsigned int level) {
	for (unsigned int k = 0; k < level; ++k) {
		appendString(output, _indentation);
	}
}

static void _output(StringBuilder * output, const unsigned int level, const char * const format, ...) {
	_indent(output, level);
	va_list arguments;
	va_start(arguments, format);
	appendFormatArguments(output, format, arguments);
	va_end(arguments);
}

static void _separate(StringBuilder * output, const unsigned int k, const unsigned int count) {
	appendString(output, k + 1 < count ? ",\n" : "\n");
}

static void _reportUnexpected(const char * what) {
	logError(_logger, "Unexpected %s in the model of an accepted program.", what);
	_status = FAILED;
}

static const char * _missingEntity() {
	_reportUnexpected("unresolved reference to an entity");
	return NULL;
}

/* LITERALS */

/**
 * Valid both in JSON and in JavaScript. "<" is escaped too, so that no string
 * can close the <script> that contains it.
 */
static void _generateString(StringBuilder * output, const char * string) {
	if (string == NULL) {
		appendString(output, "null");
		return;
	}
	appendString(output, "\"");
	for (const unsigned char * character = (const unsigned char *) string; *character != '\0'; ++character) {
		switch (*character) {
			case '"': appendString(output, "\\\""); break;
			case '\\': appendString(output, "\\\\"); break;
			case '<': appendString(output, "\\u003c"); break;
			case '\b': appendString(output, "\\b"); break;
			case '\f': appendString(output, "\\f"); break;
			case '\n': appendString(output, "\\n"); break;
			case '\r': appendString(output, "\\r"); break;
			case '\t': appendString(output, "\\t"); break;
			default:
				if (*character < 0x20) {
					appendFormat(output, "\\u%04x", *character);
				}
				else {
					appendCharacters(output, (const char *) character, 1);
				}
				break;
		}
	}
	appendString(output, "\"");
}

/** A negative integer goes between parentheses, so that it never forms "--" with another minus sign. */
static void _generateInteger(StringBuilder * output, const long long integer) {
	appendFormat(output, integer < 0 ? "(%lld)" : "%lld", integer);
}

static void _generateValue(StringBuilder * output, const Value * value) {
	switch (value->type) {
		case BOOLEAN_VALUE_TYPE:
			appendString(output, value->boolean ? "true" : "false");
			break;
		case DISTANCE_VALUE_TYPE:
		case DURATION_VALUE_TYPE:
		case INTEGER_VALUE_TYPE:
		case SPEED_VALUE_TYPE:
			appendFormat(output, "%d", value->integer);
			break;
		case STRING_VALUE_TYPE:
			_generateString(output, value->string);
			break;
		default:
			if (isEntityType(value->type)) {
				_generateString(output, value->identifier);
			}
			else {
				_reportUnexpected("value with an error");
				appendString(output, "null");
			}
			break;
	}
}

/* VIAL-CONFIG */

static void _generateIdentity(StringBuilder * output, const char * id, const char * label) {
	appendString(output, "\"id\": ");
	_generateString(output, id);
	appendString(output, ", \"label\": ");
	_generateString(output, label);
}

static void _generateIntersection(StringBuilder * output, const Intersection * intersection) {
	_output(output, 2, "{ ");
	_generateIdentity(output, intersection->id, intersection->label);
	appendFormat(output, ", \"x\": %d, \"y\": %d }", intersection->x, intersection->y);
}

static void _generateRoad(StringBuilder * output, const Road * road) {
	_output(output, 2, "{ ");
	_generateIdentity(output, road->id, road->label);
	appendString(output, ", \"from\": ");
	_generateString(output, ID_OF(road->from));
	appendString(output, ", \"to\": ");
	_generateString(output, ID_OF(road->to));
	appendFormat(output, ", \"length\": %d, \"cells\": %d, \"limit\": %d }", road->length, road->cells, road->limit);
}

static void _generatePhase(StringBuilder * output, const Phase * phase) {
	_output(output, 4, "{ \"roads\": [");
	for (unsigned int k = 0; k < phase->roadCount; ++k) {
		appendString(output, k == 0 ? "" : ", ");
		_generateString(output, ID_OF(phase->roads[k]));
	}
	appendFormat(output, "], \"duration\": %d }", phase->duration);
}

static void _generateApplication(StringBuilder * output, const PolicyApplication * application) {
	_output(output, 4, "{ \"policy\": ");
	_generateString(output, ID_OF(application->policy));
	appendString(output, ", \"args\": [");
	for (unsigned int k = 0; k < application->argumentCount; ++k) {
		appendString(output, k == 0 ? "" : ", ");
		_generateValue(output, application->arguments[k]);
	}
	appendString(output, "] }");
}

static void _generateLight(StringBuilder * output, const Light * light) {
	_output(output, 2, "{\n");
	_indent(output, 3);
	_generateIdentity(output, light->id, light->label);
	appendString(output, ", \"at\": ");
	_generateString(output, ID_OF(light->at));
	appendString(output, ",\n");
	_output(output, 3, "\"phases\": [%s", light->phaseCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < light->phaseCount; ++k) {
		_generatePhase(output, &light->phases[k]);
		_separate(output, k, light->phaseCount);
	}
	_output(output, light->phaseCount == 0 ? 0 : 3, "],\n");
	_output(output, 3, "\"apply\": [%s", light->applicationCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < light->applicationCount; ++k) {
		_generateApplication(output, &light->applications[k]);
		_separate(output, k, light->applicationCount);
	}
	_output(output, light->applicationCount == 0 ? 0 : 3, "]\n");
	_output(output, 2, "}");
}

static void _generateRoute(StringBuilder * output, const Route * route) {
	_output(output, 2, "{ ");
	_generateIdentity(output, route->id, route->label);
	appendString(output, ", \"roads\": [");
	for (unsigned int k = 0; k < route->roadCount; ++k) {
		appendString(output, k == 0 ? "" : ", ");
		_generateString(output, ID_OF(route->roads[k]));
	}
	appendString(output, "] }");
}

static void _generateFlow(StringBuilder * output, const Flow * flow) {
	_output(output, 2, "{ ");
	_generateIdentity(output, flow->id, flow->label);
	appendString(output, ", \"route\": ");
	_generateString(output, ID_OF(flow->route));
	appendFormat(output, ", \"spawn\": %d, \"every\": %d, \"from\": %d, \"to\": %d }", flow->spawn, flow->every, flow->from, flow->to);
}

static void _generateMetric(StringBuilder * output, const Metric * metric) {
	_output(output, 2, "{ ");
	_generateIdentity(output, metric->id, metric->label);
	appendString(output, ", \"aggregator\": ");
	_generateString(output, aggregatorName(metric->aggregator));
	appendString(output, ", \"event\": ");
	_generateString(output, eventName(metric->event));
	appendString(output, ", \"unit\": ");
	_generateString(output, metric->unit);
	appendString(output, " }");
}

/** Each array keeps the order of declaration, which the runtime uses as the priority. */
static void _generateConfig(StringBuilder * output, const Model * model) {
	appendString(output, "{\n");
	_output(output, 1, "\"simulation\": { ");
	_generateIdentity(output, model->simulation.id, model->simulation.label);
	appendFormat(output, ", \"duration\": %d },\n", model->simulation.duration);

	_output(output, 1, "\"intersections\": [%s", model->intersectionCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->intersectionCount; ++k) {
		_generateIntersection(output, model->intersections[k]);
		_separate(output, k, model->intersectionCount);
	}
	_output(output, model->intersectionCount == 0 ? 0 : 1, "],\n");

	_output(output, 1, "\"roads\": [%s", model->roadCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->roadCount; ++k) {
		_generateRoad(output, model->roads[k]);
		_separate(output, k, model->roadCount);
	}
	_output(output, model->roadCount == 0 ? 0 : 1, "],\n");

	_output(output, 1, "\"lights\": [%s", model->lightCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->lightCount; ++k) {
		_generateLight(output, model->lights[k]);
		_separate(output, k, model->lightCount);
	}
	_output(output, model->lightCount == 0 ? 0 : 1, "],\n");

	_output(output, 1, "\"routes\": [%s", model->routeCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->routeCount; ++k) {
		_generateRoute(output, model->routes[k]);
		_separate(output, k, model->routeCount);
	}
	_output(output, model->routeCount == 0 ? 0 : 1, "],\n");

	_output(output, 1, "\"flows\": [%s", model->flowCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->flowCount; ++k) {
		_generateFlow(output, model->flows[k]);
		_separate(output, k, model->flowCount);
	}
	_output(output, model->flowCount == 0 ? 0 : 1, "],\n");

	_output(output, 1, "\"metrics\": [%s", model->metricCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->metricCount; ++k) {
		_generateMetric(output, model->metrics[k]);
		_separate(output, k, model->metricCount);
	}
	_output(output, model->metricCount == 0 ? 0 : 1, "]\n");
	appendString(output, "}");
}

/* VIAL_LOGIC */

/**
 * Division is not here: it is "$ctx.div", which truncates towards zero and
 * returns 0 when the divisor is 0.
 */
static const char * _operator(const ExpressionType type) {
	switch (type) {
		case ADDITION: return "+";
		case CONJUNCTION: return "&&";
		case DISJUNCTION: return "||";
		case EQUALITY: return "===";
		case GREATER_THAN: return ">";
		case GREATER_THAN_OR_EQUAL_TO: return ">=";
		case INEQUALITY: return "!==";
		case LESS_THAN: return "<";
		case LESS_THAN_OR_EQUAL_TO: return "<=";
		case MULTIPLICATION: return "*";
		case SUBTRACTION: return "-";
		default:
			_reportUnexpected("binary operator");
			return "?";
	}
}

/** The base units are m, s and m/h, so that every speed is an exact integer. */
static long long _scaleOf(const Unit unit) {
	switch (unit) {
		case HOURS_UNIT: return 3600;
		case KILOMETERS_PER_HOUR_UNIT: return 1000;
		case KILOMETERS_UNIT: return 1000;
		case METERS_PER_SECOND_UNIT: return 3600;
		case METERS_UNIT: return 1;
		case MINUTES_UNIT: return 60;
		case SECONDS_UNIT: return 1;
		default:
			_reportUnexpected("unit");
			return 1;
	}
}

/**
 * The instance selects what each reference resolved to: a metric declared in
 * a "for" has one instance for each iteration.
 */
static void _generateExpression(StringBuilder * output, const Expression * expression, const unsigned int instance) {
	switch (expression->type) {
		case FACTOR:
			_generateFactor(output, expression->factor, instance);
			break;
		case ARITHMETIC_NEGATION:
			appendString(output, "(-");
			_generateExpression(output, expression->expression, instance);
			appendString(output, ")");
			break;
		case LOGICAL_NEGATION:
			appendString(output, "(!");
			_generateExpression(output, expression->expression, instance);
			appendString(output, ")");
			break;
		case DIVISION:
			appendString(output, "$ctx.div(");
			_generateExpression(output, expression->leftExpression, instance);
			appendString(output, ", ");
			_generateExpression(output, expression->rightExpression, instance);
			appendString(output, ")");
			break;
		default:
			appendString(output, "(");
			_generateExpression(output, expression->leftExpression, instance);
			appendFormat(output, " %s ", _operator(expression->type));
			_generateExpression(output, expression->rightExpression, instance);
			appendString(output, ")");
			break;
	}
}

static void _generateFactor(StringBuilder * output, const Factor * factor, const unsigned int instance) {
	switch (factor->type) {
		case BOOLEAN_FACTOR:
			appendString(output, factor->boolean ? "true" : "false");
			break;
		case CALL_FACTOR:
			_generateCall(output, factor->call, instance);
			break;
		case EXPRESSION_FACTOR:
			_generateExpression(output, factor->expression, instance);
			break;
		case INTEGER_FACTOR:
			_generateInteger(output, factor->integer);
			break;
		case MEMBER_ACCESS_FACTOR:
			if (strcmp(factor->member, "travel_time") != 0) {
				_reportUnexpected("member");
			}
			appendString(output, "$trip.travel_time");
			break;
		case QUANTITY_FACTOR:
			_generateQuantity(output, factor->quantity);
			break;
		case REFERENCE_FACTOR:
			_generateReference(output, factor->reference, instance);
			break;
		case STRING_FACTOR:
			_generateStringLiteral(output, factor->stringLiteral, instance, true);
			break;
	}
}

static void _generateCall(StringBuilder * output, const Call * call, const unsigned int instance) {
	if (strcmp(call->identifier, "queue") == 0) {
		appendString(output, "$ctx.queue(");
	}
	else if (strcmp(call->identifier, "occupancy") == 0) {
		appendString(output, "$ctx.occupancy(");
	}
	else {
		_reportUnexpected("call");
		appendString(output, "(");
	}
	for (const Argument * argument = call->arguments->first; argument != NULL; argument = argument->next) {
		appendString(output, argument == call->arguments->first ? "" : ", ");
		_generateExpression(output, argument->expression, instance);
	}
	appendString(output, ")");
}

/**
 * In "long long", because inside policies and metrics a quantity that does
 * not fit in an int is not an error.
 */
static void _generateQuantity(StringBuilder * output, const Quantity * quantity) {
	_generateInteger(output, quantity->value * _scaleOf(quantity->unit));
}

/**
 * No identifier of Vial is used as a JavaScript identifier: a parameter is
 * "$i", a constant is folded, and an entity is its identifier as a string.
 */
static void _generateReference(StringBuilder * output, const Reference * reference, const unsigned int instance) {
	if (reference->resolutionCount <= instance) {
		_reportUnexpected("reference without a resolution");
		appendString(output, "null");
		return;
	}
	const Resolution * resolution = &reference->resolutions[instance];
	switch (resolution->type) {
		case BUILTIN_RESOLUTION:
			if (strcmp(reference->identifier, "elapsed") != 0) {
				_reportUnexpected("builtin");
			}
			appendString(output, "$ctx.elapsed");
			break;
		case CONSTANT_RESOLUTION:
		case LOOP_VARIABLE_RESOLUTION:
			if (resolution->valueType == STRING_VALUE_TYPE) {
				_generateString(output, resolution->string);
			}
			else if (resolution->valueType == BOOLEAN_VALUE_TYPE) {
				appendString(output, resolution->boolean ? "true" : "false");
			}
			else {
				_generateInteger(output, resolution->integer);
			}
			break;
		case ENTITY_RESOLUTION:
			_generateString(output, resolution->identifier);
			break;
		case PARAMETER_RESOLUTION:
			appendFormat(output, "$%u", resolution->parameter);
			break;
		case ERROR_RESOLUTION:
			_reportUnexpected("reference with an error");
			appendString(output, "null");
			break;
	}
}

static void _generateInterpolation(StringBuilder * output, const Expression * expression, const unsigned int instance) {
	const char * entityClass = _entityClass(expression->staticType);
	const char * formatType = _formatType(expression->staticType);
	if (entityClass != NULL) {
		appendString(output, "$ctx.name(");
		_generateString(output, entityClass);
	}
	else {
		if (formatType == NULL) {
			_reportUnexpected("type of an interpolation");
		}
		appendString(output, "$ctx.fmt(");
		_generateString(output, formatType);
	}
	appendString(output, ", ");
	_generateExpression(output, expression, instance);
	appendString(output, ")");
}

/** A concatenation that is an operand ("grouped") goes between parentheses. */
static void _generateStringLiteral(StringBuilder * output, const StringLiteral * stringLiteral, const unsigned int instance, const bool grouped) {
	const StringPart * first = stringLiteral->parts->first;
	if (first == NULL) {
		_generateString(output, "");
		return;
	}
	const bool parenthesized = grouped && first->next != NULL;
	appendString(output, parenthesized ? "(" : "");
	for (const StringPart * part = first; part != NULL; part = part->next) {
		appendString(output, part == first ? "" : " + ");
		if (part->type == FRAGMENT_STRING_PART) {
			_generateString(output, part->fragment);
		}
		else {
			_generateInterpolation(output, part->expression, instance);
		}
	}
	appendString(output, parenthesized ? ")" : "");
}

/** The indentation of its first line is already generated, since it may follow an "else". */
static void _generateIf(StringBuilder * output, const IfStatement * ifStatement, const unsigned int level) {
	appendString(output, "if (");
	_generateExpression(output, ifStatement->condition, _policyInstance);
	appendString(output, ") {\n");
	_generateBlock(output, ifStatement->thenBlock, 1 + level);
	_output(output, level, "}");
	switch (ifStatement->elseBranchType) {
		case BLOCK_ELSE_BRANCH:
			appendString(output, " else {\n");
			_generateBlock(output, ifStatement->elseBlock, 1 + level);
			_output(output, level, "}");
			break;
		case IF_ELSE_BRANCH:
			appendString(output, " else ");
			_generateIf(output, ifStatement->elseIfStatement, level);
			break;
		case NO_ELSE_BRANCH:
			break;
	}
}

static void _generateStatement(StringBuilder * output, const Statement * statement, const unsigned int level) {
	_indent(output, level);
	switch (statement->type) {
		case EXTEND_STATEMENT:
			appendString(output, "$ctx.extend(");
			_generateReference(output, statement->road, _policyInstance);
			appendString(output, ", ");
			_generateExpression(output, statement->duration, _policyInstance);
			appendString(output, ");");
			break;
		case IF_STATEMENT:
			_generateIf(output, statement->ifStatement, level);
			break;
		case KEEP_STATEMENT:
			appendString(output, "/* keep */");
			break;
		case LOG_STATEMENT:
			appendString(output, "$ctx.log(");
			_generateStringLiteral(output, statement->message, _policyInstance, false);
			appendString(output, ");");
			break;
	}
	appendString(output, "\n");
}

static void _generateBlock(StringBuilder * output, const Block * block, const unsigned int level) {
	for (const Statement * statement = block->first; statement != NULL; statement = statement->next) {
		_generateStatement(output, statement, level);
	}
}

static void _generatePolicy(StringBuilder * output, const Policy * policy) {
	_indent(output, 2);
	_generateString(output, policy->id);
	appendString(output, ": function ($ctx");
	for (unsigned int k = 0; k < policy->parameterCount; ++k) {
		appendFormat(output, ", $%u", k);
	}
	appendString(output, ") {\n");
	_generateBlock(output, policy->declaration->block, 3);
	_output(output, 2, "}");
}

static void _generateMetricFunction(StringBuilder * output, const Metric * metric) {
	_indent(output, 2);
	if (metric->aggregator == COUNT_AGGREGATOR) {
		appendString(output, "null");
		return;
	}
	if (metric->argument == NULL) {
		_reportUnexpected("metric without an argument");
		appendString(output, "null");
		return;
	}
	appendString(output, metric->event == ARRIVAL_EVENT ? "function ($ctx, $trip) { return " : "function ($ctx) { return ");
	_generateExpression(output, metric->argument, metric->instance);
	appendString(output, "; }");
}

/** The functions of the metrics go in the same order as the metrics of "vial-config". */
static void _generateLogic(StringBuilder * output, const Model * model) {
	appendString(output, "{\n");
	_output(output, 1, "policies: {%s", model->policyCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->policyCount; ++k) {
		_generatePolicy(output, model->policies[k]);
		_separate(output, k, model->policyCount);
	}
	_output(output, model->policyCount == 0 ? 0 : 1, "},\n");
	_output(output, 1, "metrics: [%s", model->metricCount == 0 ? "" : "\n");
	for (unsigned int k = 0; k < model->metricCount; ++k) {
		_generateMetricFunction(output, model->metrics[k]);
		_separate(output, k, model->metricCount);
	}
	_output(output, model->metricCount == 0 ? 0 : 1, "]\n");
	appendString(output, "}");
}

/* HTML */

static StringBuilder * _readAsset(const char * path) {
	FILE * file = fopen(path, "rb");
	if (file == NULL) {
		logError(_logger, "Cannot read the runtime asset \"%s\": %s.", path, strerror(errno));
		return NULL;
	}
	StringBuilder * asset = createStringBuilder();
	char buffer[4096];
	size_t count = 0;
	while (0 < (count = fread(buffer, sizeof(char), sizeof(buffer), file))) {
		appendCharacters(asset, buffer, count);
	}
	const int error = errno;
	const bool failed = ferror(file) != 0;
	fclose(file);
	if (failed) {
		logError(_logger, "Cannot read the runtime asset \"%s\": %s.", path, strerror(error));
		destroyStringBuilder(asset);
		return NULL;
	}
	return asset;
}

/** A marker must be alone on its line, but it may be surrounded by blanks. */
static int _markerOf(const char * line, const size_t length) {
	size_t start = 0;
	size_t end = length;
	while (start < end && (line[start] == ' ' || line[start] == '\t')) {
		++start;
	}
	while (start < end && (line[end - 1] == ' ' || line[end - 1] == '\t' || line[end - 1] == '\r')) {
		--end;
	}
	for (int marker = 0; marker < MARKER_COUNT; ++marker) {
		if (strlen(_markers[marker]) == end - start && memcmp(line + start, _markers[marker], end - start) == 0) {
			return marker;
		}
	}
	return -1;
}

static void _generateMarker(StringBuilder * html, const Marker marker, const StringBuilder * runtime, const Model * model) {
	switch (marker) {
		case CONFIG_MARKER:
			appendString(html, "<script type=\"application/json\" id=\"vial-config\">\n");
			_generateConfig(html, model);
			appendString(html, "\n</script>");
			break;
		case LOGIC_MARKER:
			appendString(html, "<script>var VIAL_LOGIC = ");
			_generateLogic(html, model);
			appendString(html, ";</script>");
			break;
		case RUNTIME_MARKER:
			appendString(html, "<script>\n");
			appendCharacters(html, runtime->content, runtime->length);
			if (runtime->length == 0 || runtime->content[runtime->length - 1] != '\n') {
				appendString(html, "\n");
			}
			appendString(html, "</script>");
			break;
		default:
			break;
	}
}

static bool _generateHtml(StringBuilder * html, const StringBuilder * viewer, const StringBuilder * runtime, const Model * model, const char * viewerPath) {
	unsigned int occurrences[MARKER_COUNT] = { 0 };
	size_t start = 0;
	while (start < viewer->length) {
		const char * line = viewer->content + start;
		const char * newline = memchr(line, '\n', viewer->length - start);
		const size_t length = newline != NULL ? (size_t) (newline - line) : viewer->length - start;
		const int marker = _markerOf(line, length);
		if (marker < 0) {
			appendCharacters(html, line, length);
		}
		else {
			++occurrences[marker];
			_generateMarker(html, (Marker) marker, runtime, model);
		}
		appendString(html, newline != NULL ? "\n" : "");
		start += length + 1;
	}
	bool valid = true;
	for (int marker = 0; marker < MARKER_COUNT; ++marker) {
		if (occurrences[marker] == 0) {
			logError(_logger, "The viewer \"%s\" has no marker \"%s\" (it must be alone on its line).", viewerPath, _markers[marker]);
			valid = false;
		}
		else if (1 < occurrences[marker]) {
			logError(_logger, "The viewer \"%s\" has the marker \"%s\" %u times, but it must have it exactly once.",
				viewerPath, _markers[marker], occurrences[marker]);
			valid = false;
		}
	}
	return valid;
}

static bool _write(const StringBuilder * html) {
	const size_t written = fwrite(html->content, sizeof(char), html->length, stdout);
	if (written != html->length || fflush(stdout) != 0) {
		logError(_logger, "Cannot write the generated HTML to the standard output: %s.", strerror(errno));
		return false;
	}
	return true;
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeGenerator(CompilerState * compilerState) {
	const Model * model = compilerState->model;
	if (model == NULL) {
		logError(_logger, "There is no model to generate.");
		return FAILED;
	}
	_status = SUCCEEDED;
	const char * runtimePath = getStringOrDefault("VIAL_RUNTIME_PATH", _defaultRuntimePath);
	logDebugging(_logger, "Generating the HTML viewer, with the runtime of \"%s\"...", runtimePath);
	char * viewerPath = _assetPath(runtimePath, "viewer.html");
	char * runtimeJsPath = _assetPath(runtimePath, "runtime.js");
	StringBuilder * viewer = _readAsset(viewerPath);
	StringBuilder * runtime = _readAsset(runtimeJsPath);
	if (viewer == NULL || runtime == NULL) {
		logError(_logger, "The runtime directory is \"%s\" (set VIAL_RUNTIME_PATH to change it).", runtimePath);
		_status = FAILED;
	}
	else {
		StringBuilder * html = createStringBuilder();
		if (!_generateHtml(html, viewer, runtime, model, viewerPath)) {
			_status = FAILED;
		}
		if (_status == SUCCEEDED && _write(html)) {
			logDebugging(_logger, "Generation is done: %zu bytes of HTML.", html->length);
		}
		else {
			_status = FAILED;
		}
		destroyStringBuilder(html);
	}
	destroyStringBuilder(runtime);
	destroyStringBuilder(viewer);
	free(runtimeJsPath);
	free(viewerPath);
	return _status;
}
