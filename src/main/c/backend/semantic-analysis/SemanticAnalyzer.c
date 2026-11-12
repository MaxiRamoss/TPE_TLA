#include "SemanticAnalyzer.h"
#include "Evaluator.h"
#include "SymbolTable.h"

/* MODULE INTERNAL STATE */

/**
 * An error already reported. When the unrolling analyzes the same
 * declaration more than once, an identical error (same line and message) is
 * reported only once (spec-v3 D15).
 */
typedef struct ReportedError ReportedError;

struct ReportedError {
	int line;
	char * message;
	ReportedError * next;
};

static unsigned int _errors = 0;
static Logger * _logger = NULL;
static Model * _model = NULL;
static ReportedError * _reportedErrors = NULL;
static SymbolTable * _symbolTable = NULL;

static void _destroyReportedErrors();

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_destroyReportedErrors();
	_errors = 0;
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	_errors = 0;
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE FUNCTIONS */

static void _analyzeConstant(const ConstantDeclaration * declaration, const int line);
static void _analyzeDeclaration(const Declaration * declaration);
static void _analyzeDeclarations(const Declarations * declarations);
static void _analyzeFlow(const FlowDeclaration * declaration, const int line);
static void _analyzeFor(const ForDeclaration * declaration, const int line);
static void _analyzeIntersection(const IntersectionDeclaration * declaration, const int line);
static void _analyzeLight(const LightDeclaration * declaration, const int line);
static void _analyzeMetric(const MetricDeclaration * declaration, const int line);
static void _analyzePolicy(const PolicyDeclaration * declaration, const int line);
static void _analyzePosition(const Position * position, Intersection * intersection, const char * owner);
static void _analyzeRoad(const RoadDeclaration * declaration, const int line);
static void _analyzeRoute(const RouteDeclaration * declaration, const int line);
static void _analyzeSimulation(const Simulation * simulation);
static int _cells(const int length);
static bool _checkArrayElement(const Symbol * base, const char * subject, const SymbolClass symbolClass, const ValueType type, const int line);
static bool _checkUndeclared(const char * name, const int line);
static Symbol * _declare(const Reference * reference, const char * name, const SymbolClass symbolClass, const ValueType type, const int line);
static char * _describe(const SymbolClass symbolClass, const char * name, const Reference * reference);
static void _destroyReportedErrors();
static bool _euclideanDistance(const Intersection * from, const Intersection * to, int * distance);
static Value * _evaluateField(const Expression * expression, const ValueType expected, const char * field, const char * owner);
static bool _evaluateInteger(const Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer);
static char * _evaluateLabel(const Expression * label, const char * owner);
static char * _kindName(const SymbolClass symbolClass, const ValueType type, const bool plural);
static char * _resolveDeclaredName(const Reference * reference, const SymbolClass symbolClass, const ValueType type, const int line);
static unsigned long long _squareRoot(const unsigned long long number);
static ValueType _valueTypeOfFundamentalType(const FundamentalType type);
static ValueType _valueTypeOfParameterType(const ParameterType type);

/** The default speed limit of a road (spec-v3 D12): 40 km/h, in m/h. */
static const int _defaultLimit = 40000;

/**
 * Analyzes a constant: its value is folded, and its type must be the declared
 * one (V04). A constant with an error has an ERROR value, so its references
 * produce no more errors.
 */
static void _analyzeConstant(const ConstantDeclaration * declaration, const int line) {
	const ValueType type = _valueTypeOfFundamentalType(declaration->type);
	char * name = _resolveDeclaredName(declaration->reference, CONSTANT_SYMBOL, type, line);
	char * owner = _describe(CONSTANT_SYMBOL, name, declaration->reference);
	Value * value = _evaluateField(declaration->expression, type, "the value of", owner);
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, CONSTANT_SYMBOL, type, line) : NULL;
	if (symbol != NULL) {
		symbol->value = value != NULL ? value : createErrorValue();
	}
	else {
		destroyValue(value);
	}
	free(name);
	free(owner);
}

static void _analyzeDeclaration(const Declaration * declaration) {
	switch (declaration->type) {
		case CONSTANT_DECLARATION:
			_analyzeConstant(declaration->constantDeclaration, declaration->line);
			break;
		case FLOW_DECLARATION:
			_analyzeFlow(declaration->flowDeclaration, declaration->line);
			break;
		case FOR_DECLARATION:
			_analyzeFor(declaration->forDeclaration, declaration->line);
			break;
		case INTERSECTION_DECLARATION:
			_analyzeIntersection(declaration->intersectionDeclaration, declaration->line);
			break;
		case LIGHT_DECLARATION:
			_analyzeLight(declaration->lightDeclaration, declaration->line);
			break;
		case METRIC_DECLARATION:
			_analyzeMetric(declaration->metricDeclaration, declaration->line);
			break;
		case POLICY_DECLARATION:
			_analyzePolicy(declaration->policyDeclaration, declaration->line);
			break;
		case ROAD_DECLARATION:
			_analyzeRoad(declaration->roadDeclaration, declaration->line);
			break;
		case ROUTE_DECLARATION:
			_analyzeRoute(declaration->routeDeclaration, declaration->line);
			break;
	}
}

/**
 * Analyzes a list of declarations, in order of appearance (spec-v3 D13).
 */
static void _analyzeDeclarations(const Declarations * declarations) {
	for (const Declaration * declaration = declarations->first; declaration != NULL; declaration = declaration->next) {
		_analyzeDeclaration(declaration);
	}
}

/**
 * Analyzes a flow. Without a window, it lasts the entire simulation.
 */
static void _analyzeFlow(const FlowDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, FLOW_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(FLOW_SYMBOL, name, declaration->reference);
	Flow * flow = calloc(1, sizeof(Flow));
	flow->line = line;
	const Symbol * route = resolveEntityReference(_symbolTable, declaration->route, ROUTE_SYMBOL, owner);
	flow->route = route != NULL ? route->route : NULL;
	flow->label = _evaluateLabel(declaration->label, owner);
	_evaluateInteger(declaration->amount, INTEGER_VALUE_TYPE, "the spawn of", owner, &flow->spawn);
	_evaluateInteger(declaration->period, DURATION_VALUE_TYPE, "the period of", owner, &flow->every);
	flow->from = 0;
	flow->to = _model->simulation.duration;
	if (declaration->from != NULL) {
		_evaluateInteger(declaration->from, DURATION_VALUE_TYPE, "the start of the window of", owner, &flow->from);
		_evaluateInteger(declaration->to, DURATION_VALUE_TYPE, "the end of the window of", owner, &flow->to);
	}
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, FLOW_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		flow->id = name;
		symbol->flow = flow;
		addFlow(_model, flow);
	}
	else {
		free(name);
		destroyFlow(flow);
	}
	free(owner);
}

/**
 * Unrolls a "for" (spec-v3 §3.3): for each value of the inclusive range, the
 * body is analyzed with the loop variable bound to it, in a new scope. If the
 * range has an error, or the variable is a duplicate, the body is analyzed
 * only once, with the variable bound to an ERROR value, so its errors are
 * reported without cascading (spec-v3 D15).
 */
static void _analyzeFor(const ForDeclaration * declaration, const int line) {
	char * owner = describeSymbol(LOOP_VARIABLE_SYMBOL, declaration->variable);
	int from = 0;
	int to = 0;
	bool valid = _evaluateInteger(declaration->from, INTEGER_VALUE_TYPE, "the lower bound of", owner, &from);
	valid = _evaluateInteger(declaration->to, INTEGER_VALUE_TYPE, "the upper bound of", owner, &to) && valid;
	valid = _checkUndeclared(declaration->variable, line) && valid;
	pushScope(_symbolTable);
	Symbol * variable = declareLocalSymbol(_symbolTable, declaration->variable, LOOP_VARIABLE_SYMBOL, line);
	if (valid) {
		for (long long k = from; k <= to; ++k) {
			logDebugging(_logger, "Unrolling the loop at line %d: %s = %lld", line, declaration->variable, k);
			destroyValue(variable->value);
			variable->value = createIntegerValue(INTEGER_VALUE_TYPE, (int) k);
			_analyzeDeclarations(declaration->declarations);
		}
	}
	else {
		logDebugging(_logger, "Analyzing the loop at line %d once, without a value for %s", line, declaration->variable);
		variable->value = createErrorValue();
		_analyzeDeclarations(declaration->declarations);
	}
	popScope(_symbolTable);
	free(owner);
}

/**
 * Analyzes an intersection: its position, in m, and its label.
 */
static void _analyzeIntersection(const IntersectionDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, INTERSECTION_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(INTERSECTION_SYMBOL, name, declaration->reference);
	Intersection * intersection = calloc(1, sizeof(Intersection));
	intersection->line = line;
	_analyzePosition(declaration->position, intersection, owner);
	intersection->label = _evaluateLabel(declaration->label, owner);
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, INTERSECTION_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		intersection->id = name;
		symbol->intersection = intersection;
		addIntersection(_model, intersection);
	}
	else {
		free(name);
		destroyIntersection(intersection);
	}
	free(owner);
}

/**
 * Analyzes a light: its intersection, the roads and the duration of each
 * phase, and the policy of each "apply" with its arguments (their arity and
 * types are checked in step 4).
 */
static void _analyzeLight(const LightDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, LIGHT_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(LIGHT_SYMBOL, name, declaration->reference);
	Light * light = calloc(1, sizeof(Light));
	light->line = line;
	const Symbol * at = resolveEntityReference(_symbolTable, declaration->intersection, INTERSECTION_SYMBOL, owner);
	light->at = at != NULL ? at->intersection : NULL;
	light->label = _evaluateLabel(declaration->label, owner);
	for (const LightItem * item = declaration->items->first; item != NULL; item = item->next) {
		if (item->type == PHASE_LIGHT_ITEM) {
			++light->phaseCount;
		}
		else {
			++light->applicationCount;
		}
	}
	light->phases = calloc(light->phaseCount, sizeof(Phase));
	light->applications = calloc(light->applicationCount, sizeof(PolicyApplication));
	unsigned int phaseIndex = 0;
	unsigned int applicationIndex = 0;
	for (const LightItem * item = declaration->items->first; item != NULL; item = item->next) {
		if (item->type == PHASE_LIGHT_ITEM) {
			Phase * phase = &light->phases[phaseIndex++];
			phase->line = item->line;
			for (const ReferenceItem * road = item->roads->first; road != NULL; road = road->next) {
				++phase->roadCount;
			}
			phase->roads = calloc(phase->roadCount, sizeof(Road *));
			unsigned int roadIndex = 0;
			for (const ReferenceItem * road = item->roads->first; road != NULL; road = road->next) {
				const Symbol * symbol = resolveEntityReference(_symbolTable, road->reference, ROAD_SYMBOL, owner);
				phase->roads[roadIndex++] = symbol != NULL ? symbol->road : NULL;
			}
			char field[48];
			snprintf(field, sizeof(field), "the duration of phase %u of", phaseIndex);
			_evaluateInteger(item->duration, DURATION_VALUE_TYPE, field, owner, &phase->duration);
		}
		else {
			PolicyApplication * application = &light->applications[applicationIndex++];
			application->line = item->line;
			const Symbol * policy = lookupSymbol(_symbolTable, item->call->identifier);
			if (policy == NULL) {
				semanticError(item->call->line, "%s applies undeclared policy \"%s\"", owner, item->call->identifier);
			}
			else if (policy->symbolClass != POLICY_SYMBOL) {
				semanticError(item->call->line, "%s applies %s \"%s\", which is not a policy",
					owner, symbolClassName(policy->symbolClass), policy->name);
			}
			else {
				application->policy = policy->policy;
			}
			for (const Argument * argument = item->call->arguments->first; argument != NULL; argument = argument->next) {
				++application->argumentCount;
			}
			application->arguments = calloc(application->argumentCount, sizeof(Value *));
			unsigned int argumentIndex = 0;
			for (const Argument * argument = item->call->arguments->first; argument != NULL; argument = argument->next) {
				application->arguments[argumentIndex++] = evaluateExpression(_symbolTable, argument->expression, owner);
			}
		}
	}
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, LIGHT_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		light->id = name;
		symbol->light = light;
		addLight(_model, light);
	}
	else {
		free(name);
		destroyLight(light);
	}
	free(owner);
}

/**
 * Registers a metric and its label. Its aggregator and its event are analyzed
 * in step 4.
 */
static void _analyzeMetric(const MetricDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, METRIC_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(METRIC_SYMBOL, name, declaration->reference);
	Metric * metric = calloc(1, sizeof(Metric));
	metric->line = line;
	metric->declaration = declaration;
	metric->label = _evaluateLabel(declaration->label, owner);
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, METRIC_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		metric->id = name;
		symbol->metric = metric;
		addMetric(_model, metric);
	}
	else {
		free(name);
		destroyMetric(metric);
	}
	free(owner);
}

/**
 * Registers a policy and the types of its parameters. Its body (and the scope
 * of its parameters) is analyzed in step 4.
 */
static void _analyzePolicy(const PolicyDeclaration * declaration, const int line) {
	Policy * policy = calloc(1, sizeof(Policy));
	policy->line = line;
	policy->declaration = declaration;
	for (const Parameter * parameter = declaration->parameters->first; parameter != NULL; parameter = parameter->next) {
		++policy->parameterCount;
	}
	policy->parameterTypes = calloc(policy->parameterCount, sizeof(ValueType));
	unsigned int parameterIndex = 0;
	for (const Parameter * parameter = declaration->parameters->first; parameter != NULL; parameter = parameter->next) {
		policy->parameterTypes[parameterIndex++] = _valueTypeOfParameterType(parameter->type);
	}
	Symbol * symbol = NULL;
	if (_checkUndeclared(declaration->identifier, line)) {
		symbol = declareGlobalSymbol(_symbolTable, declaration->identifier, POLICY_SYMBOL, line);
	}
	if (symbol != NULL) {
		policy->id = concatenate(1, declaration->identifier);
		symbol->policy = policy;
		addPolicy(_model, policy);
	}
	else {
		destroyPolicy(policy);
	}
}

/**
 * Computes the position of an intersection, in m: absolute, or relative to
 * another intersection (Y grows towards the north, and X towards the east).
 * If it has an error, the position stays at the origin.
 */
static void _analyzePosition(const Position * position, Intersection * intersection, const char * owner) {
	if (position->type == ABSOLUTE_POSITION) {
		_evaluateInteger(position->x, DISTANCE_VALUE_TYPE, "the x coordinate of", owner, &intersection->x);
		_evaluateInteger(position->y, DISTANCE_VALUE_TYPE, "the y coordinate of", owner, &intersection->y);
		return;
	}
	int distance = 0;
	const bool valid = _evaluateInteger(position->distance, DISTANCE_VALUE_TYPE, "the relative distance of", owner, &distance);
	const Symbol * origin = resolveEntityReference(_symbolTable, position->origin, INTERSECTION_SYMBOL, owner);
	if (!valid || origin == NULL) {
		return;
	}
	int x = origin->intersection->x;
	int y = origin->intersection->y;
	bool overflow = false;
	switch (position->direction) {
		case EAST_DIRECTION: overflow = __builtin_add_overflow(x, distance, &x); break;
		case NORTH_DIRECTION: overflow = __builtin_add_overflow(y, distance, &y); break;
		case SOUTH_DIRECTION: overflow = __builtin_sub_overflow(y, distance, &y); break;
		case WEST_DIRECTION: overflow = __builtin_sub_overflow(x, distance, &x); break;
	}
	if (overflow) {
		semanticError(position->distance->line, "the position of %s overflows an integer", owner);
		return;
	}
	intersection->x = x;
	intersection->y = y;
}

/**
 * Analyzes a road: its intersections, its length (explicit, or else the
 * euclidean distance rounded to the meter), its speed limit (40 km/h by
 * default), its cells and its label.
 */
static void _analyzeRoad(const RoadDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, ROAD_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(ROAD_SYMBOL, name, declaration->reference);
	Road * road = calloc(1, sizeof(Road));
	road->line = line;
	const Symbol * from = resolveEntityReference(_symbolTable, declaration->from, INTERSECTION_SYMBOL, owner);
	const Symbol * to = resolveEntityReference(_symbolTable, declaration->to, INTERSECTION_SYMBOL, owner);
	road->from = from != NULL ? from->intersection : NULL;
	road->to = to != NULL ? to->intersection : NULL;
	if (declaration->length != NULL) {
		_evaluateInteger(declaration->length, DISTANCE_VALUE_TYPE, "the length of", owner, &road->length);
	}
	else if (road->from != NULL && road->to != NULL && !_euclideanDistance(road->from, road->to, &road->length)) {
		semanticError(line, "the length of %s overflows an integer", owner);
	}
	road->limit = _defaultLimit;
	if (declaration->limit != NULL) {
		_evaluateInteger(declaration->limit, SPEED_VALUE_TYPE, "the limit of", owner, &road->limit);
	}
	road->cells = _cells(road->length);
	road->label = _evaluateLabel(declaration->label, owner);
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, ROAD_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		road->id = name;
		symbol->road = road;
		addRoad(_model, road);
	}
	else {
		free(name);
		destroyRoad(road);
	}
	free(owner);
}

/**
 * Analyzes a route: its waypoints are intersections. The roads of each leg
 * are resolved in step 3.
 */
static void _analyzeRoute(const RouteDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, ROUTE_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(ROUTE_SYMBOL, name, declaration->reference);
	Route * route = calloc(1, sizeof(Route));
	route->line = line;
	for (const Waypoint * waypoint = declaration->path->first; waypoint != NULL; waypoint = waypoint->next) {
		++route->waypointCount;
	}
	route->waypoints = calloc(route->waypointCount, sizeof(Intersection *));
	unsigned int waypointIndex = 0;
	for (const Waypoint * waypoint = declaration->path->first; waypoint != NULL; waypoint = waypoint->next) {
		const Symbol * symbol = resolveEntityReference(_symbolTable, waypoint->reference, INTERSECTION_SYMBOL, owner);
		route->waypoints[waypointIndex++] = symbol != NULL ? symbol->intersection : NULL;
	}
	route->label = _evaluateLabel(declaration->label, owner);
	Symbol * symbol = name != NULL ? _declare(declaration->reference, name, ROUTE_SYMBOL, ERROR_VALUE_TYPE, line) : NULL;
	if (symbol != NULL) {
		route->id = name;
		symbol->route = route;
		addRoute(_model, route);
	}
	else {
		free(name);
		destroyRoute(route);
	}
	free(owner);
}

/**
 * Analyzes the header of the program. The simulation is not a symbol.
 */
static void _analyzeSimulation(const Simulation * simulation) {
	char * owner = concatenate(3, "simulation \"", simulation->identifier, "\"");
	_model->simulation.id = concatenate(1, simulation->identifier);
	_model->simulation.label = _evaluateLabel(simulation->label, owner);
	_evaluateInteger(simulation->duration, DURATION_VALUE_TYPE, "the duration of", owner, &_model->simulation.duration);
	free(owner);
}

/**
 * The number of cells of a road (spec-v3 §4.1): its length divided by 7.5 m,
 * rounded, and at least 1.
 */
static int _cells(const int length) {
	const long long cells = (4LL * length + 15) / 30;
	return cells < 1 ? 1 : (int) cells;
}

/**
 * Checks that a new element of an array has the same class as the others
 * and, if it is a constant, the same type (V03). The subject describes the
 * element (e.g., '"X[1]"').
 */
static bool _checkArrayElement(const Symbol * base, const char * subject, const SymbolClass symbolClass, const ValueType type, const int line) {
	if (base->elementClass == symbolClass && (symbolClass != CONSTANT_SYMBOL || base->elementType == type)) {
		return true;
	}
	char * kind = _kindName(symbolClass, type, false);
	char * elements = _kindName(base->elementClass, base->elementType, true);
	semanticError(line, "cannot declare %s as %s: the elements of array \"%s\" are %s", subject, kind, base->name, elements);
	free(kind);
	free(elements);
	return false;
}

/**
 * Checks that a scalar name is not visible yet (there is no shadowing): it is
 * not a builtin (V01), an array (V03), nor any other symbol (V01).
 */
static bool _checkUndeclared(const char * name, const int line) {
	const Symbol * symbol = lookupSymbol(_symbolTable, name);
	if (symbol == NULL) {
		return true;
	}
	if (symbol->symbolClass == BUILTIN_SYMBOL) {
		semanticError(line, "identifier \"%s\" is reserved for a builtin", name);
	}
	else if (symbol->symbolClass == ARRAY_SYMBOL) {
		semanticError(line, "cannot declare \"%s\": it is an array declared at line %d", name, symbol->line);
	}
	else {
		semanticError(line, "identifier \"%s\" is already declared at line %d", name, symbol->line);
	}
	return false;
}

/**
 * Declares the resolved name of a global entity or constant, in the global
 * scope. The base name of an element of an array must be a homogeneous array
 * (V03), and every name must be new (V01). The type is only relevant for
 * constants. Returns NULL if it cannot be declared.
 */
static Symbol * _declare(const Reference * reference, const char * name, const SymbolClass symbolClass, const ValueType type, const int line) {
	if (reference->index == NULL) {
		return _checkUndeclared(name, line) ? declareGlobalSymbol(_symbolTable, name, symbolClass, line) : NULL;
	}
	Symbol * base = lookupSymbol(_symbolTable, reference->identifier);
	if (base == NULL) {
		base = declareGlobalSymbol(_symbolTable, reference->identifier, ARRAY_SYMBOL, line);
		base->elementClass = symbolClass;
		base->elementType = type;
	}
	else if (base->symbolClass == BUILTIN_SYMBOL) {
		semanticError(line, "identifier \"%s\" is reserved for a builtin", reference->identifier);
		return NULL;
	}
	else if (base->symbolClass != ARRAY_SYMBOL) {
		semanticError(line, "cannot declare \"%s\": \"%s\" is %s declared at line %d, not an array",
			name, base->name, symbolClassNameWithArticle(base->symbolClass), base->line);
		return NULL;
	}
	char * subject = concatenate(3, "\"", name, "\"");
	const bool homogeneous = _checkArrayElement(base, subject, symbolClass, type, line);
	free(subject);
	if (!homogeneous) {
		return NULL;
	}
	const Symbol * symbol = lookupSymbol(_symbolTable, name);
	if (symbol != NULL) {
		semanticError(line, "identifier \"%s\" is already declared at line %d", name, symbol->line);
		return NULL;
	}
	return declareGlobalSymbol(_symbolTable, name, symbolClass, line);
}

/**
 * Describes the declaration being analyzed (e.g., 'road "Tramo[5]"'), using
 * its base name if its index failed.
 */
static char * _describe(const SymbolClass symbolClass, const char * name, const Reference * reference) {
	return describeSymbol(symbolClass, name != NULL ? name : reference->identifier);
}

static void _destroyReportedErrors() {
	while (_reportedErrors != NULL) {
		ReportedError * next = _reportedErrors->next;
		free(_reportedErrors->message);
		free(_reportedErrors);
		_reportedErrors = next;
	}
}

/**
 * The euclidean distance between two intersections, rounded to the meter,
 * with an integer square root (no libm). Returns false if it does not fit in
 * an integer.
 */
static bool _euclideanDistance(const Intersection * from, const Intersection * to, int * distance) {
	const long long dx = (long long) to->x - from->x;
	const long long dy = (long long) to->y - from->y;
	const unsigned long long ax = dx < 0 ? -dx : dx;
	const unsigned long long ay = dy < 0 ? -dy : dy;
	if (INT_MAX < ax || INT_MAX < ay) {
		return false;
	}
	const unsigned long long square = ax * ax + ay * ay;
	unsigned long long root = _squareRoot(square);
	if (root < square - root * root) {
		++root;
	}
	if (INT_MAX < root) {
		return false;
	}
	*distance = (int) root;
	return true;
}

/**
 * Evaluates an expression whose type must be the expected one (V04). The
 * field describes it in the message (e.g., "the length of"). Returns NULL if
 * it has an error.
 */
static Value * _evaluateField(const Expression * expression, const ValueType expected, const char * field, const char * owner) {
	Value * value = evaluateExpression(_symbolTable, expression, owner);
	if (value->type == expected) {
		return value;
	}
	if (value->type != ERROR_VALUE_TYPE) {
		semanticError(expression->line, "%s %s must be %s, but it is %s",
			field, owner, valueTypeNameWithArticle(expected), valueTypeNameWithArticle(value->type));
	}
	destroyValue(value);
	return NULL;
}

/**
 * Evaluates an integer or a magnitude of the expected type (V04). Returns
 * false if it has an error, without changing the integer.
 */
static bool _evaluateInteger(const Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer) {
	Value * value = _evaluateField(expression, expected, field, owner);
	if (value == NULL) {
		return false;
	}
	*integer = value->integer;
	destroyValue(value);
	return true;
}

/**
 * Evaluates a label, which must be a string (V04). Returns NULL if there is
 * no label, or if it has an error.
 */
static char * _evaluateLabel(const Expression * label, const char * owner) {
	if (label == NULL) {
		return NULL;
	}
	Value * value = _evaluateField(label, STRING_VALUE_TYPE, "the label of", owner);
	if (value == NULL) {
		return NULL;
	}
	char * string = concatenate(1, value->string);
	destroyValue(value);
	return string;
}

/**
 * The kind of a declared name, for messages: its class (e.g., "a road" or
 * "roads") or, for a constant, also its type ("an integer constant").
 */
static char * _kindName(const SymbolClass symbolClass, const ValueType type, const bool plural) {
	if (symbolClass != CONSTANT_SYMBOL) {
		return plural
			? concatenate(2, symbolClassName(symbolClass), "s")
			: concatenate(1, symbolClassNameWithArticle(symbolClass));
	}
	return plural
		? concatenate(2, valueTypeName(type), " constants")
		: concatenate(2, valueTypeNameWithArticle(type), " constant");
}

/**
 * Resolves the name of a declaration, evaluating its index. If the index has
 * an error, returns NULL, and the base name is marked as an incomplete array,
 * so the references to its elements produce no more errors.
 */
static char * _resolveDeclaredName(const Reference * reference, const SymbolClass symbolClass, const ValueType type, const int line) {
	if (reference->index == NULL) {
		return concatenate(1, reference->identifier);
	}
	char * owner = describeSymbol(symbolClass, reference->identifier);
	int index = 0;
	const bool succeeded = evaluateIndex(_symbolTable, reference, owner, &index);
	free(owner);
	if (succeeded) {
		return resolvedName(reference->identifier, true, index);
	}
	Symbol * base = lookupSymbol(_symbolTable, reference->identifier);
	if (base == NULL) {
		base = declareGlobalSymbol(_symbolTable, reference->identifier, ARRAY_SYMBOL, line);
		base->elementClass = symbolClass;
		base->elementType = type;
	}
	if (base->symbolClass == ARRAY_SYMBOL) {
		char * subject = concatenate(3, "an element of \"", reference->identifier, "\"");
		_checkArrayElement(base, subject, symbolClass, type, line);
		free(subject);
		base->incomplete = true;
	}
	else if (base->symbolClass == BUILTIN_SYMBOL) {
		semanticError(line, "identifier \"%s\" is reserved for a builtin", base->name);
	}
	else {
		semanticError(line, "cannot declare an element of \"%s\": \"%s\" is %s declared at line %d, not an array",
			base->name, base->name, symbolClassNameWithArticle(base->symbolClass), base->line);
	}
	return NULL;
}

/**
 * The integer square root of a number, rounded down (bit by bit).
 */
static unsigned long long _squareRoot(const unsigned long long number) {
	unsigned long long remainder = number;
	unsigned long long root = 0;
	unsigned long long bit = 1ULL << 62;
	while (remainder < bit) {
		bit >>= 2;
	}
	while (bit != 0) {
		if (root + bit <= remainder) {
			remainder -= root + bit;
			root = (root >> 1) + bit;
		}
		else {
			root >>= 1;
		}
		bit >>= 2;
	}
	return root;
}

static ValueType _valueTypeOfFundamentalType(const FundamentalType type) {
	switch (type) {
		case BOOLEAN_FUNDAMENTAL_TYPE: return BOOLEAN_VALUE_TYPE;
		case DISTANCE_FUNDAMENTAL_TYPE: return DISTANCE_VALUE_TYPE;
		case DURATION_FUNDAMENTAL_TYPE: return DURATION_VALUE_TYPE;
		case INTEGER_FUNDAMENTAL_TYPE: return INTEGER_VALUE_TYPE;
		case SPEED_FUNDAMENTAL_TYPE: return SPEED_VALUE_TYPE;
		case STRING_FUNDAMENTAL_TYPE: return STRING_VALUE_TYPE;
		default: return ERROR_VALUE_TYPE;
	}
}

static ValueType _valueTypeOfParameterType(const ParameterType type) {
	switch (type) {
		case BOOLEAN_PARAMETER_TYPE: return BOOLEAN_VALUE_TYPE;
		case DISTANCE_PARAMETER_TYPE: return DISTANCE_VALUE_TYPE;
		case DURATION_PARAMETER_TYPE: return DURATION_VALUE_TYPE;
		case FLOW_PARAMETER_TYPE: return FLOW_VALUE_TYPE;
		case INTEGER_PARAMETER_TYPE: return INTEGER_VALUE_TYPE;
		case INTERSECTION_PARAMETER_TYPE: return INTERSECTION_VALUE_TYPE;
		case LIGHT_PARAMETER_TYPE: return LIGHT_VALUE_TYPE;
		case ROAD_PARAMETER_TYPE: return ROAD_VALUE_TYPE;
		case ROUTE_PARAMETER_TYPE: return ROUTE_VALUE_TYPE;
		case SPEED_PARAMETER_TYPE: return SPEED_VALUE_TYPE;
		case STRING_PARAMETER_TYPE: return STRING_VALUE_TYPE;
		default: return ERROR_VALUE_TYPE;
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	const Program * program = compilerState->abstractSyntaxTree;
	_errors = 0;
	_model = createModel();
	_symbolTable = createSymbolTable();
	_analyzeSimulation(program->simulation);
	_analyzeDeclarations(program->declarations);
	logSymbolTable(_symbolTable);
	destroySymbolTable(_symbolTable);
	_symbolTable = NULL;
	_destroyReportedErrors();
	if (0 < _errors) {
		logDebugging(_logger, "The program has %u semantic error(s).", _errors);
		destroyModel(_model);
		_model = NULL;
		return FAILED;
	}
	logModel(_model);
	compilerState->model = _model;
	_model = NULL;
	return SUCCEEDED;
}

void semanticError(const int line, const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	char * message = calloc(1 + length, sizeof(char));
	va_start(arguments, format);
	vsnprintf(message, 1 + length, format, arguments);
	va_end(arguments);
	for (const ReportedError * reported = _reportedErrors; reported != NULL; reported = reported->next) {
		if (reported->line == line && strcmp(reported->message, message) == 0) {
			free(message);
			return;
		}
	}
	logError(_logger, "Semantic error at line %d: %s", line, message);
	ReportedError * reported = calloc(1, sizeof(ReportedError));
	reported->line = line;
	reported->message = message;
	reported->next = _reportedErrors;
	_reportedErrors = reported;
	++_errors;
}
