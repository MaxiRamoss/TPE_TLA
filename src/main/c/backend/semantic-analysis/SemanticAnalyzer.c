#include "SemanticAnalyzer.h"
#include "Evaluator.h"
#include "SymbolTable.h"
#include "TypeChecker.h"

/* MODULE INTERNAL STATE */

/**
 * The unrolling analyzes a declaration once per iteration, so an identical
 * error (same line and message) is reported only once.
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

static void _analyzeAggregator(Call * call, const ExecutionContext context, Metric * metric, const char * owner);
static void _analyzeConstant(const ConstantDeclaration * declaration, const int line);
static void _analyzeDeclaration(const Declaration * declaration);
static void _analyzeDeclarations(const Declarations * declarations);
static void _analyzeFlow(const FlowDeclaration * declaration, const int line);
static void _analyzeFor(const ForDeclaration * declaration, const int line);
static void _analyzeIntersection(const IntersectionDeclaration * declaration, const int line);
static void _analyzeLight(const LightDeclaration * declaration, const int line);
static void _analyzeMetric(MetricDeclaration * declaration, const int line);
static void _analyzePolicy(PolicyDeclaration * declaration, const int line);
static bool _analyzePosition(const Position * position, Intersection * intersection, const char * owner);
static void _analyzeRoad(const RoadDeclaration * declaration, const int line);
static void _analyzeRoute(const RouteDeclaration * declaration, const int line);
static void _analyzeSimulation(const Simulation * simulation);
static int _cells(const int length);
static void _checkApplication(const PolicyApplication * application, const Call * call, const char * owner);
static bool _checkArrayElement(const Symbol * base, const char * subject, const SymbolClass symbolClass, const ValueType type, const int line);
static bool _checkUndeclared(const char * name, const int line);
static Symbol * _declare(const Reference * reference, const char * name, const SymbolClass symbolClass, const ValueType type, const int line);
static void _declareParameter(const Parameter * parameter, const unsigned int index, const char * owner, const int line);
static char * _describe(const SymbolClass symbolClass, const char * name, const Reference * reference);
static void _destroyReportedErrors();
static bool _enables(const Light * light, const Road * road);
static bool _euclideanDistance(const Intersection * from, const Intersection * to, int * distance);
static Value * _evaluateField(Expression * expression, const ValueType expected, const char * field, const char * owner);
static bool _evaluateInteger(Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer);
static char * _evaluateLabel(Expression * label, const char * owner);
static bool _evaluatePositive(Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer);
static char * _formatMagnitude(const ValueType type, const int integer);
static char * _kindName(const SymbolClass symbolClass, const ValueType type, const bool plural);
static char * _resolveDeclaredName(const Reference * reference, const SymbolClass symbolClass, const ValueType type, const int line);
static Road * _resolveLeg(const Route * route, const Intersection * from, const Intersection * to);
static void _resolveRoutes();
static char * _roadNames(const Intersection * from, const Intersection * to, const unsigned int count);
static unsigned long long _squareRoot(const unsigned long long number);
static const char * _unitOfType(const ValueType type);
static void _validateIncomingRoads(const Light * light);
static void _validateLights();
static void _validatePhases(const Light * light);
static void _validatePositions();
static ValueType _valueTypeOfFundamentalType(const FundamentalType type);
static ValueType _valueTypeOfParameterType(const ParameterType type);

/** 40 km/h, in m/h. */
static const int _defaultLimit = 40000;

/**
 * The arguments of an unknown aggregator are checked anyway, to report their
 * own errors.
 */
static void _analyzeAggregator(Call * call, const ExecutionContext context, Metric * metric, const char * owner) {
	bool known = false;
	for (Aggregator aggregator = AVERAGE_AGGREGATOR; aggregator <= SUM_AGGREGATOR; ++aggregator) {
		if (strcmp(call->identifier, aggregatorName(aggregator)) == 0) {
			metric->aggregator = aggregator;
			known = true;
		}
	}
	if (!known) {
		semanticError(call->line, "%s uses unknown aggregator \"%s\": the aggregators are \"count\", \"sum\", \"average\", \"minimum\" and \"maximum\"",
			owner, call->identifier);
	}
	unsigned int count = 0;
	ValueType type = ERROR_VALUE_TYPE;
	for (Argument * argument = call->arguments->first; argument != NULL; argument = argument->next) {
		const ValueType argumentType = checkExpression(_symbolTable, argument->expression, context, owner);
		if (count++ == 0) {
			type = argumentType;
			metric->argument = argument->expression;
		}
	}
	if (!known) {
		return;
	}
	if (metric->aggregator == COUNT_AGGREGATOR) {
		if (count != 0) {
			semanticError(call->line, "aggregator \"count\" of %s takes no arguments, but it receives %u", owner, count);
		}
		metric->argument = NULL;
	}
	else if (count != 1) {
		semanticError(call->line, "aggregator \"%s\" of %s takes 1 argument, but it receives %u", call->identifier, owner, count);
	}
	else if (type != ERROR_VALUE_TYPE && type != INTEGER_VALUE_TYPE && !isMagnitudeType(type)) {
		semanticError(metric->argument->line, "the argument of aggregator \"%s\" of %s must be an integer or a magnitude, but it is %s",
			call->identifier, owner, valueTypeNameWithArticle(type));
	}
	else {
		metric->unit = _unitOfType(type);
	}
}

/** A constant with an error has an ERROR value, so its uses do not cascade. */
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

/** A single pass in order: a name is only visible after its declaration. */
static void _analyzeDeclarations(const Declarations * declarations) {
	for (const Declaration * declaration = declarations->first; declaration != NULL; declaration = declaration->next) {
		_analyzeDeclaration(declaration);
	}
}

static void _analyzeFlow(const FlowDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, FLOW_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(FLOW_SYMBOL, name, declaration->reference);
	Flow * flow = calloc(1, sizeof(Flow));
	flow->line = line;
	const Symbol * route = resolveEntityReference(_symbolTable, declaration->route, ROUTE_SYMBOL, owner);
	flow->route = route != NULL ? route->route : NULL;
	flow->label = _evaluateLabel(declaration->label, owner);
	_evaluatePositive(declaration->amount, INTEGER_VALUE_TYPE, "the spawn of", owner, &flow->spawn);
	_evaluatePositive(declaration->period, DURATION_VALUE_TYPE, "the period of", owner, &flow->every);
	flow->from = 0;
	flow->to = _model->simulation.duration;
	if (declaration->from != NULL) {
		bool valid = _evaluateInteger(declaration->from, DURATION_VALUE_TYPE, "the start of the window of", owner, &flow->from);
		valid = _evaluateInteger(declaration->to, DURATION_VALUE_TYPE, "the end of the window of", owner, &flow->to) && valid;
		if (valid && flow->to <= flow->from) {
			char * from = _formatMagnitude(DURATION_VALUE_TYPE, flow->from);
			char * to = _formatMagnitude(DURATION_VALUE_TYPE, flow->to);
			semanticError(declaration->from->line, "the window of %s must start before it ends, but it goes from %s to %s",
				owner, from, to);
			free(from);
			free(to);
		}
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
 * If the range has an error or the variable is a duplicate, the body is
 * analyzed once, with the variable bound to an ERROR value, so it does not
 * cascade.
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

static void _analyzeIntersection(const IntersectionDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, INTERSECTION_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(INTERSECTION_SYMBOL, name, declaration->reference);
	Intersection * intersection = calloc(1, sizeof(Intersection));
	intersection->line = line;
	intersection->validPosition = _analyzePosition(declaration->position, intersection, owner);
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
 * Its phases are checked against the roads that enter its intersection at the
 * end, because those roads may be declared after the light.
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
			_evaluatePositive(item->duration, DURATION_VALUE_TYPE, field, owner, &phase->duration);
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
			if (application->policy != NULL) {
				_checkApplication(application, item->call, owner);
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
 * With an unknown event, the aggregator is checked as "on arrival", the most
 * permissive context, so the error does not cascade.
 */
static void _analyzeMetric(MetricDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, METRIC_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(METRIC_SYMBOL, name, declaration->reference);
	Metric * metric = calloc(1, sizeof(Metric));
	metric->line = line;
	metric->declaration = declaration;
	metric->instance = declaration->instanceCount++;
	metric->label = _evaluateLabel(declaration->label, owner);
	if (strcmp(declaration->event, eventName(TICK_EVENT)) == 0) {
		metric->event = TICK_EVENT;
	}
	else if (strcmp(declaration->event, eventName(ARRIVAL_EVENT)) == 0) {
		metric->event = ARRIVAL_EVENT;
	}
	else {
		semanticError(line, "%s has unknown event \"%s\": the events are \"tick\" and \"arrival\"", owner, declaration->event);
		metric->event = ARRIVAL_EVENT;
	}
	_analyzeAggregator(declaration->aggregator, metric->event == TICK_EVENT ? TICK_CONTEXT : ARRIVAL_CONTEXT, metric, owner);
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
 * Its name is declared before its body is checked, so the body sees it. The
 * body is checked even if the policy is never applied.
 */
static void _analyzePolicy(PolicyDeclaration * declaration, const int line) {
	char * owner = describeSymbol(POLICY_SYMBOL, declaration->identifier);
	Policy * policy = calloc(1, sizeof(Policy));
	policy->line = line;
	policy->declaration = declaration;
	for (const Parameter * parameter = declaration->parameters->first; parameter != NULL; parameter = parameter->next) {
		++policy->parameterCount;
	}
	policy->parameterTypes = calloc(policy->parameterCount, sizeof(ValueType));
	Symbol * symbol = NULL;
	if (_checkUndeclared(declaration->identifier, line)) {
		symbol = declareGlobalSymbol(_symbolTable, declaration->identifier, POLICY_SYMBOL, line);
	}
	pushScope(_symbolTable);
	unsigned int parameterIndex = 0;
	for (const Parameter * parameter = declaration->parameters->first; parameter != NULL; parameter = parameter->next) {
		policy->parameterTypes[parameterIndex] = _valueTypeOfParameterType(parameter->type);
		_declareParameter(parameter, parameterIndex++, owner, line);
	}
	checkBlock(_symbolTable, declaration->block, owner);
	popScope(_symbolTable);
	if (symbol != NULL) {
		policy->id = concatenate(1, declaration->identifier);
		symbol->policy = policy;
		addPolicy(_model, policy);
	}
	else {
		destroyPolicy(policy);
	}
	free(owner);
}

/** A position relative to an invalid one is invalid too, with no new error. */
static bool _analyzePosition(const Position * position, Intersection * intersection, const char * owner) {
	if (position->type == ABSOLUTE_POSITION) {
		const bool validX = _evaluateInteger(position->x, DISTANCE_VALUE_TYPE, "the x coordinate of", owner, &intersection->x);
		const bool validY = _evaluateInteger(position->y, DISTANCE_VALUE_TYPE, "the y coordinate of", owner, &intersection->y);
		return validX && validY;
	}
	int distance = 0;
	const bool valid = _evaluateInteger(position->distance, DISTANCE_VALUE_TYPE, "the relative distance of", owner, &distance);
	const Symbol * origin = resolveEntityReference(_symbolTable, position->origin, INTERSECTION_SYMBOL, owner);
	if (!valid || origin == NULL || !origin->intersection->validPosition) {
		return false;
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
		return false;
	}
	intersection->x = x;
	intersection->y = y;
	return true;
}

static void _analyzeRoad(const RoadDeclaration * declaration, const int line) {
	char * name = _resolveDeclaredName(declaration->reference, ROAD_SYMBOL, ERROR_VALUE_TYPE, line);
	char * owner = _describe(ROAD_SYMBOL, name, declaration->reference);
	Road * road = calloc(1, sizeof(Road));
	road->line = line;
	const Symbol * from = resolveEntityReference(_symbolTable, declaration->from, INTERSECTION_SYMBOL, owner);
	const Symbol * to = resolveEntityReference(_symbolTable, declaration->to, INTERSECTION_SYMBOL, owner);
	road->from = from != NULL ? from->intersection : NULL;
	road->to = to != NULL ? to->intersection : NULL;
	const bool loop = road->from != NULL && road->from == road->to;
	if (loop) {
		semanticError(line, "%s goes from intersection \"%s\" to itself", owner, road->from->id);
	}
	const bool positioned = road->from != NULL && road->to != NULL && road->from->validPosition && road->to->validPosition;
	bool validLength = false;
	if (declaration->length != NULL) {
		validLength = _evaluatePositive(declaration->length, DISTANCE_VALUE_TYPE, "the length of", owner, &road->length);
	}
	else if (positioned) {
		validLength = _euclideanDistance(road->from, road->to, &road->length);
		if (!validLength) {
			semanticError(line, "the length of %s overflows an integer", owner);
		}
	}
	road->limit = _defaultLimit;
	if (declaration->limit != NULL) {
		_evaluatePositive(declaration->limit, SPEED_VALUE_TYPE, "the limit of", owner, &road->limit);
	}
	road->validGeometry = positioned && validLength && !loop;
	road->cells = road->validGeometry ? _cells(road->length) : 0;
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
 * The roads of its legs are resolved at the end, so they may be declared after
 * the route.
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

/** The simulation is not a symbol. */
static void _analyzeSimulation(const Simulation * simulation) {
	char * owner = concatenate(3, "simulation \"", simulation->identifier, "\"");
	_model->simulation.id = concatenate(1, simulation->identifier);
	_model->simulation.label = _evaluateLabel(simulation->label, owner);
	_evaluatePositive(simulation->duration, DURATION_VALUE_TYPE, "the duration of", owner, &_model->simulation.duration);
	free(owner);
}

/** The length divided by 7.5 m, rounded, and at least 1. */
static int _cells(const int length) {
	const long long cells = (4LL * length + 15) / 30;
	return cells < 1 ? 1 : (int) cells;
}

static void _checkApplication(const PolicyApplication * application, const Call * call, const char * owner) {
	const Policy * policy = application->policy;
	if (application->argumentCount != policy->parameterCount) {
		semanticError(call->line, "%s applies policy \"%s\" with %u argument%s, but it takes %u",
			owner, policy->id, application->argumentCount, application->argumentCount == 1 ? "" : "s", policy->parameterCount);
	}
	const Argument * argument = call->arguments->first;
	const Parameter * parameter = policy->declaration->parameters->first;
	for (unsigned int k = 0; argument != NULL && parameter != NULL; ++k) {
		const ValueType type = application->arguments[k]->type;
		const ValueType expected = policy->parameterTypes[k];
		if (type != ERROR_VALUE_TYPE && type != expected) {
			semanticError(argument->expression->line, "the argument for parameter \"%s\" of policy \"%s\" in %s must be %s, but it is %s",
				parameter->identifier, policy->id, owner, valueTypeNameWithArticle(expected), valueTypeNameWithArticle(type));
		}
		argument = argument->next;
		parameter = parameter->next;
	}
}

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

/** There is no shadowing: the name cannot be visible yet, even as a builtin. */
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
 * Entities and constants declared inside a "for" are global too. The type only
 * matters for constants.
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
 * A repeated parameter keeps the first declaration. One that shadows a visible
 * symbol is an error, but it is declared anyway so its uses do not cascade.
 */
static void _declareParameter(const Parameter * parameter, const unsigned int index, const char * owner, const int line) {
	const Symbol * visible = lookupSymbol(_symbolTable, parameter->identifier);
	if (visible != NULL && visible->symbolClass == PARAMETER_SYMBOL) {
		semanticError(line, "%s has more than one parameter named \"%s\"", owner, parameter->identifier);
		return;
	}
	_checkUndeclared(parameter->identifier, line);
	Symbol * symbol = declareLocalSymbol(_symbolTable, parameter->identifier, PARAMETER_SYMBOL, line);
	symbol->parameterType = _valueTypeOfParameterType(parameter->type);
	symbol->parameterIndex = index;
}

/** Uses the base name if the index of the declaration failed. */
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

/** With a NULL road, it tells whether some phase has an unresolved road. */
static bool _enables(const Light * light, const Road * road) {
	for (unsigned int k = 0; k < light->phaseCount; ++k) {
		const Phase * phase = &light->phases[k];
		for (unsigned int j = 0; j < phase->roadCount; ++j) {
			if (phase->roads[j] == road) {
				return true;
			}
		}
	}
	return false;
}

/**
 * Rounded to the nearest meter with an integer square root, so it needs no
 * libm. False if it overflows an integer.
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

static Value * _evaluateField(Expression * expression, const ValueType expected, const char * field, const char * owner) {
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

/** On an error, the integer keeps its previous value. */
static bool _evaluateInteger(Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer) {
	Value * value = _evaluateField(expression, expected, field, owner);
	if (value == NULL) {
		return false;
	}
	*integer = value->integer;
	destroyValue(value);
	return true;
}

static char * _evaluateLabel(Expression * label, const char * owner) {
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

static bool _evaluatePositive(Expression * expression, const ValueType expected, const char * field, const char * owner, int * integer) {
	Value * value = _evaluateField(expression, expected, field, owner);
	if (value == NULL) {
		return false;
	}
	*integer = value->integer;
	const bool positive = 0 < value->integer;
	if (!positive) {
		char * text = formatValue(value, NULL);
		semanticError(expression->line, "%s %s must be positive, but it is %s", field, owner, text);
		free(text);
	}
	destroyValue(value);
	return positive;
}

static char * _formatMagnitude(const ValueType type, const int integer) {
	Value * value = createIntegerValue(type, integer);
	char * text = formatValue(value, NULL);
	destroyValue(value);
	return text;
}

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
 * If the index has an error, the base name becomes an incomplete array, so a
 * reference to a missing element produces no more errors.
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
 * A leg without a road is not reported if a road with an unresolved endpoint
 * could be it, because that endpoint was already reported.
 */
static Road * _resolveLeg(const Route * route, const Intersection * from, const Intersection * to) {
	if (from == NULL || to == NULL) {
		return NULL;
	}
	Road * road = NULL;
	unsigned int count = 0;
	bool unresolved = false;
	for (unsigned int k = 0; k < _model->roadCount; ++k) {
		Road * candidate = _model->roads[k];
		if (candidate->from == from && candidate->to == to) {
			road = candidate;
			++count;
		}
		else if ((candidate->from == NULL || candidate->from == from) && (candidate->to == NULL || candidate->to == to)) {
			unresolved = true;
		}
	}
	if (count == 1) {
		return road;
	}
	if (1 < count) {
		char * names = _roadNames(from, to, count);
		semanticError(route->line, "the leg from intersection \"%s\" to intersection \"%s\" of route \"%s\" is ambiguous, with %u roads: %s",
			from->id, to->id, route->id, count, names);
		free(names);
	}
	else if (!unresolved) {
		semanticError(route->line, "the leg from intersection \"%s\" to intersection \"%s\" of route \"%s\" has no road",
			from->id, to->id, route->id);
	}
	return NULL;
}

static void _resolveRoutes() {
	for (unsigned int k = 0; k < _model->routeCount; ++k) {
		Route * route = _model->routes[k];
		route->roadCount = route->waypointCount - 1;
		route->roads = calloc(route->roadCount, sizeof(Road *));
		for (unsigned int j = 0; j < route->roadCount; ++j) {
			route->roads[j] = _resolveLeg(route, route->waypoints[j], route->waypoints[1 + j]);
		}
	}
}

static char * _roadNames(const Intersection * from, const Intersection * to, const unsigned int count) {
	char * names = concatenate(1, "");
	unsigned int index = 0;
	for (unsigned int k = 0; k < _model->roadCount; ++k) {
		const Road * road = _model->roads[k];
		if (road->from == from && road->to == to) {
			const char * separator = (index == 0) ? "" : (index + 1 == count) ? " and " : ", ";
			char * next = concatenate(5, names, separator, "\"", road->id, "\"");
			free(names);
			names = next;
			++index;
		}
	}
	return names;
}

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

static const char * _unitOfType(const ValueType type) {
	switch (type) {
		case DISTANCE_VALUE_TYPE: return "m";
		case DURATION_VALUE_TYPE: return "s";
		case SPEED_VALUE_TYPE: return "km/h";
		default: return NULL;
	}
}

/**
 * If a phase has a road that could not be resolved, it could be any of them,
 * so the light is not checked.
 */
static void _validateIncomingRoads(const Light * light) {
	if (_enables(light, NULL)) {
		return;
	}
	for (unsigned int k = 0; k < _model->roadCount; ++k) {
		const Road * road = _model->roads[k];
		if (road->to == light->at && !_enables(light, road)) {
			semanticError(light->line, "road \"%s\" enters intersection \"%s\", but no phase of light \"%s\" enables it",
				road->id, light->at->id, light->id);
		}
	}
}

static void _validateLights() {
	for (unsigned int k = 0; k < _model->lightCount; ++k) {
		Light * light = _model->lights[k];
		Intersection * at = light->at;
		if (at == NULL) {
			continue;
		}
		if (at->light == NULL) {
			at->light = light;
		}
		else {
			semanticError(light->line, "light \"%s\" is at intersection \"%s\", which already has light \"%s\"",
				light->id, at->id, at->light->id);
		}
		_validatePhases(light);
		_validateIncomingRoads(light);
	}
}

static void _validatePhases(const Light * light) {
	for (unsigned int k = 0; k < light->phaseCount; ++k) {
		const Phase * phase = &light->phases[k];
		for (unsigned int j = 0; j < phase->roadCount; ++j) {
			const Road * road = phase->roads[j];
			if (road != NULL && road->to != NULL && road->to != light->at) {
				semanticError(phase->line, "phase %u of light \"%s\" enables road \"%s\", which does not enter intersection \"%s\"",
					1 + k, light->id, road->id, light->at->id);
			}
		}
	}
}

static void _validatePositions() {
	for (unsigned int k = 0; k < _model->intersectionCount; ++k) {
		const Intersection * intersection = _model->intersections[k];
		for (unsigned int j = 0; intersection->validPosition && j < k; ++j) {
			const Intersection * first = _model->intersections[j];
			if (first->validPosition && first->x == intersection->x && first->y == intersection->y) {
				semanticError(intersection->line, "intersection \"%s\" is at (%d m, %d m), the same position as intersection \"%s\"",
					intersection->id, intersection->x, intersection->y, first->id);
				break;
			}
		}
	}
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
	_validatePositions();
	_validateLights();
	_resolveRoutes();
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
	logAnnotations(_model);
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
