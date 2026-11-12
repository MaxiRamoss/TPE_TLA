#include "TypeChecker.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownTypeCheckerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: TypeChecker...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeTypeCheckerModule() {
	_logger = createLogger("TypeChecker");
	return _shutdownTypeCheckerModule;
}

/* PRIVATE FUNCTIONS */

typedef struct Check Check;

/** The owner is the declaration being analyzed (e.g., 'policy "P"'), for the messages. */
struct Check {
	SymbolTable * symbolTable;
	ExecutionContext context;
	const char * owner;
};

static void _annotate(Reference * reference, const Symbol * symbol, const ValueType type);
static ValueType _checkBinary(const Check * check, Expression * expression);
static ValueType _checkBuiltin(const Check * check, const Reference * reference, const Symbol * symbol);
static ValueType _checkCall(const Check * check, const Call * call);
static ValueType _checkExpression(const Check * check, Expression * expression);
static ValueType _checkFactor(const Check * check, const Factor * factor);
static void _checkIf(const Check * check, const IfStatement * ifStatement);
static ValueType _checkMember(const Check * check, const Factor * factor);
static const Symbol * _checkReference(const Check * check, Reference * reference, ValueType * type);
static ValueType _checkRoadFunction(const Check * check, const Call * call);
static void _checkStatement(const Check * check, const Statement * statement);
static void _checkStatements(const Check * check, const Block * block);
static ValueType _checkString(const Check * check, const StringLiteral * stringLiteral);
static ValueType _checkUnary(const Check * check, Expression * expression);
static char * _describeResolution(const Reference * reference, const unsigned int instance);
static void _logExpression(const Expression * expression, const unsigned int instance, const unsigned int depth);
static void _logFactor(const Factor * factor, const char * type, const unsigned int instance, const unsigned int depth);
static void _logIf(const IfStatement * ifStatement, const char * keyword, const unsigned int depth);
static void _logStatements(const Block * block, const unsigned int depth);
static void _logString(const StringLiteral * stringLiteral, const unsigned int instance, const unsigned int depth);
static ValueType _quantityType(const Unit unit);
static const char * _unitSymbol(const Unit unit);

/** The two arguments of a "%*s" that indents a logged line. */
#define INDENT(depth) ((int) (4 * (depth))), ""

/** Each analysis of a declaration appends one resolution, so its index is the instance. */
static void _annotate(Reference * reference, const Symbol * symbol, const ValueType type) {
	reference->resolutions = realloc(reference->resolutions, (1 + reference->resolutionCount) * sizeof(Resolution));
	Resolution * resolution = &reference->resolutions[reference->resolutionCount++];
	*resolution = (Resolution) { .type = ERROR_RESOLUTION, .valueType = ERROR_VALUE_TYPE };
	if (symbol == NULL || type == ERROR_VALUE_TYPE) {
		return;
	}
	resolution->valueType = type;
	switch (symbol->symbolClass) {
		case BUILTIN_SYMBOL:
			resolution->type = BUILTIN_RESOLUTION;
			break;
		case CONSTANT_SYMBOL:
		case LOOP_VARIABLE_SYMBOL:
			resolution->type = symbol->symbolClass == CONSTANT_SYMBOL ? CONSTANT_RESOLUTION : LOOP_VARIABLE_RESOLUTION;
			if (type == STRING_VALUE_TYPE) {
				resolution->string = concatenate(1, symbol->value->string);
			}
			else if (type == BOOLEAN_VALUE_TYPE) {
				resolution->boolean = symbol->value->boolean;
			}
			else {
				resolution->integer = symbol->value->integer;
			}
			break;
		case PARAMETER_SYMBOL:
			resolution->type = PARAMETER_RESOLUTION;
			resolution->parameter = symbol->parameterIndex;
			break;
		default:
			resolution->type = ENTITY_RESOLUTION;
			resolution->identifier = concatenate(1, symbol->name);
			break;
	}
}

static ValueType _checkBinary(const Check * check, Expression * expression) {
	const ValueType left = _checkExpression(check, expression->leftExpression);
	const ValueType right = _checkExpression(check, expression->rightExpression);
	if (left == ERROR_VALUE_TYPE || right == ERROR_VALUE_TYPE) {
		return ERROR_VALUE_TYPE;
	}
	const ValueType type = binaryOperationType(expression->type, left, right);
	if (type == ERROR_VALUE_TYPE) {
		semanticError(expression->line, "operator \"%s\" cannot be applied to %s and %s in %s", operationSymbol(expression->type),
			valueTypeNameWithArticle(left), valueTypeNameWithArticle(right), check->owner);
	}
	return type;
}

static ValueType _checkBuiltin(const Check * check, const Reference * reference, const Symbol * symbol) {
	const char * name = symbol->name;
	if (strcmp(name, "elapsed") == 0) {
		return DURATION_VALUE_TYPE;
	}
	if (strcmp(name, "trip") == 0 && check->context != ARRIVAL_CONTEXT) {
		semanticError(reference->line, "%s uses builtin \"trip\", which is only available in metrics on arrival", check->owner);
	}
	else if (strcmp(name, "trip") == 0) {
		semanticError(reference->line, "%s uses builtin \"trip\" without its member \"travel_time\"", check->owner);
	}
	else if (strcmp(name, "tick") == 0 || strcmp(name, "arrival") == 0) {
		semanticError(reference->line, "%s uses event \"%s\" as a value", check->owner, name);
	}
	else {
		semanticError(reference->line, "%s uses builtin \"%s\" without calling it", check->owner, name);
	}
	return ERROR_VALUE_TYPE;
}

/** The arguments of an invalid call are checked anyway, to report their own errors. */
static ValueType _checkCall(const Check * check, const Call * call) {
	const Symbol * symbol = lookupSymbol(check->symbolTable, call->identifier);
	const bool builtin = symbol != NULL && symbol->symbolClass == BUILTIN_SYMBOL;
	if (builtin && (strcmp(call->identifier, "queue") == 0 || strcmp(call->identifier, "occupancy") == 0)) {
		return _checkRoadFunction(check, call);
	}
	if (symbol == NULL) {
		semanticError(call->line, "%s calls unknown function \"%s\"", check->owner, call->identifier);
	}
	else if (symbol->symbolClass == POLICY_SYMBOL) {
		semanticError(call->line, "%s uses policy \"%s\" as an expression", check->owner, call->identifier);
	}
	else if (builtin && isAggregator(call->identifier) && check->context == POLICY_CONTEXT) {
		semanticError(call->line, "%s uses aggregator \"%s\" outside a metric", check->owner, call->identifier);
	}
	else if (builtin && isAggregator(call->identifier)) {
		semanticError(call->line, "%s uses aggregator \"%s\" inside the argument of its aggregator", check->owner, call->identifier);
	}
	else {
		semanticError(call->line, "%s calls %s \"%s\", which is not a function",
			check->owner, symbolClassName(symbol->symbolClass), call->identifier);
	}
	for (const Argument * argument = call->arguments->first; argument != NULL; argument = argument->next) {
		_checkExpression(check, argument->expression);
	}
	return ERROR_VALUE_TYPE;
}

static ValueType _checkExpression(const Check * check, Expression * expression) {
	ValueType type = ERROR_VALUE_TYPE;
	switch (expression->type) {
		case FACTOR:
			type = _checkFactor(check, expression->factor);
			break;
		case ARITHMETIC_NEGATION:
		case LOGICAL_NEGATION:
			type = _checkUnary(check, expression);
			break;
		default:
			type = _checkBinary(check, expression);
			break;
	}
	expression->staticType = type;
	return type;
}

static ValueType _checkFactor(const Check * check, const Factor * factor) {
	ValueType type = ERROR_VALUE_TYPE;
	switch (factor->type) {
		case BOOLEAN_FACTOR:
			return BOOLEAN_VALUE_TYPE;
		case CALL_FACTOR:
			return _checkCall(check, factor->call);
		case EXPRESSION_FACTOR:
			return _checkExpression(check, factor->expression);
		case INTEGER_FACTOR:
			return INTEGER_VALUE_TYPE;
		case MEMBER_ACCESS_FACTOR:
			return _checkMember(check, factor);
		case QUANTITY_FACTOR:
			return _quantityType(factor->quantity->unit);
		case REFERENCE_FACTOR:
			_checkReference(check, factor->reference, &type);
			return type;
		case STRING_FACTOR:
			return _checkString(check, factor->stringLiteral);
		default:
			return ERROR_VALUE_TYPE;
	}
}

static void _checkIf(const Check * check, const IfStatement * ifStatement) {
	const ValueType condition = _checkExpression(check, ifStatement->condition);
	if (condition != ERROR_VALUE_TYPE && condition != BOOLEAN_VALUE_TYPE) {
		semanticError(ifStatement->condition->line, "the condition of \"if\" in %s must be a boolean, but it is %s",
			check->owner, valueTypeNameWithArticle(condition));
	}
	_checkStatements(check, ifStatement->thenBlock);
	switch (ifStatement->elseBranchType) {
		case BLOCK_ELSE_BRANCH:
			_checkStatements(check, ifStatement->elseBlock);
			break;
		case IF_ELSE_BRANCH:
			_checkIf(check, ifStatement->elseIfStatement);
			break;
		case NO_ELSE_BRANCH:
			break;
	}
}

static ValueType _checkMember(const Check * check, const Factor * factor) {
	if (isBuiltinReference(check->symbolTable, factor->factor, "trip")) {
		if (strcmp(factor->member, "travel_time") != 0) {
			semanticError(factor->line, "%s accesses unknown member \"%s\" of \"trip\"", check->owner, factor->member);
			return ERROR_VALUE_TYPE;
		}
		if (check->context != ARRIVAL_CONTEXT) {
			semanticError(factor->line, "%s uses builtin \"trip\", which is only available in metrics on arrival", check->owner);
			return ERROR_VALUE_TYPE;
		}
		return DURATION_VALUE_TYPE;
	}
	const ValueType type = _checkFactor(check, factor->factor);
	if (type != ERROR_VALUE_TYPE) {
		semanticError(factor->line, "%s accesses unknown member \"%s\" of %s", check->owner, factor->member, valueTypeNameWithArticle(type));
	}
	return ERROR_VALUE_TYPE;
}

static const Symbol * _checkReference(const Check * check, Reference * reference, ValueType * type) {
	const Symbol * symbol = resolveReference(check->symbolTable, reference, check->owner, "identifier");
	*type = ERROR_VALUE_TYPE;
	if (symbol != NULL) {
		switch (symbol->symbolClass) {
			case BUILTIN_SYMBOL:
				*type = _checkBuiltin(check, reference, symbol);
				break;
			case CONSTANT_SYMBOL:
			case LOOP_VARIABLE_SYMBOL:
				*type = symbol->value->type;
				break;
			case PARAMETER_SYMBOL:
				*type = symbol->parameterType;
				break;
			default:
				*type = symbolClassValueType(symbol->symbolClass);
				break;
		}
	}
	_annotate(reference, symbol, *type);
	return symbol;
}

static ValueType _checkRoadFunction(const Check * check, const Call * call) {
	unsigned int count = 0;
	ValueType type = ERROR_VALUE_TYPE;
	int line = call->line;
	for (const Argument * argument = call->arguments->first; argument != NULL; argument = argument->next) {
		const ValueType argumentType = _checkExpression(check, argument->expression);
		if (count++ == 0) {
			type = argumentType;
			line = argument->expression->line;
		}
	}
	if (count != 1) {
		semanticError(call->line, "builtin \"%s\" in %s takes 1 argument, but it receives %u", call->identifier, check->owner, count);
		return ERROR_VALUE_TYPE;
	}
	if (type == ERROR_VALUE_TYPE) {
		return ERROR_VALUE_TYPE;
	}
	if (type != ROAD_VALUE_TYPE) {
		semanticError(line, "the argument of builtin \"%s\" in %s must be a road, but it is %s",
			call->identifier, check->owner, valueTypeNameWithArticle(type));
		return ERROR_VALUE_TYPE;
	}
	return INTEGER_VALUE_TYPE;
}

static void _checkStatement(const Check * check, const Statement * statement) {
	switch (statement->type) {
		case EXTEND_STATEMENT: {
			ValueType road = ERROR_VALUE_TYPE;
			const Symbol * symbol = _checkReference(check, statement->road, &road);
			if (road != ERROR_VALUE_TYPE && road != ROAD_VALUE_TYPE) {
				semanticError(statement->road->line, "%s extends \"%s\", which is %s, not a road",
					check->owner, symbol->name, valueTypeNameWithArticle(road));
			}
			const ValueType duration = _checkExpression(check, statement->duration);
			if (duration != ERROR_VALUE_TYPE && duration != DURATION_VALUE_TYPE) {
				semanticError(statement->duration->line, "the duration of \"extend\" in %s must be a duration, but it is %s",
					check->owner, valueTypeNameWithArticle(duration));
			}
			break;
		}
		case IF_STATEMENT:
			_checkIf(check, statement->ifStatement);
			break;
		case KEEP_STATEMENT:
			break;
		case LOG_STATEMENT:
			_checkString(check, statement->message);
			break;
	}
}

static void _checkStatements(const Check * check, const Block * block) {
	for (const Statement * statement = block->first; statement != NULL; statement = statement->next) {
		_checkStatement(check, statement);
	}
}

/** An interpolation accepts any type. */
static ValueType _checkString(const Check * check, const StringLiteral * stringLiteral) {
	bool failed = false;
	for (const StringPart * part = stringLiteral->parts->first; part != NULL; part = part->next) {
		if (part->type == INTERPOLATION_STRING_PART && _checkExpression(check, part->expression) == ERROR_VALUE_TYPE) {
			failed = true;
		}
	}
	return failed ? ERROR_VALUE_TYPE : STRING_VALUE_TYPE;
}

static ValueType _checkUnary(const Check * check, Expression * expression) {
	const ValueType operand = _checkExpression(check, expression->expression);
	if (operand == ERROR_VALUE_TYPE) {
		return ERROR_VALUE_TYPE;
	}
	const ValueType type = unaryOperationType(expression->type, operand);
	if (type == ERROR_VALUE_TYPE) {
		semanticError(expression->line, "operator \"%s\" cannot be applied to %s in %s",
			operationSymbol(expression->type), valueTypeNameWithArticle(operand), check->owner);
	}
	return type;
}

static char * _describeResolution(const Reference * reference, const unsigned int instance) {
	if (reference->resolutionCount <= instance) {
		return concatenate(1, "not annotated");
	}
	const Resolution * resolution = &reference->resolutions[instance];
	switch (resolution->type) {
		case BUILTIN_RESOLUTION:
			return concatenate(3, "builtin \"", reference->identifier, "\"");
		case CONSTANT_RESOLUTION:
		case LOOP_VARIABLE_RESOLUTION: {
			Value value = { .type = resolution->valueType };
			if (resolution->valueType == STRING_VALUE_TYPE) {
				value.string = resolution->string;
			}
			else if (resolution->valueType == BOOLEAN_VALUE_TYPE) {
				value.boolean = resolution->boolean;
			}
			else {
				value.integer = resolution->integer;
			}
			char * text = formatValue(&value, NULL);
			const char * quote = resolution->valueType == STRING_VALUE_TYPE ? "\"" : "";
			char * description = concatenate(4, resolution->type == CONSTANT_RESOLUTION ? "constant " : "loop variable ", quote, text, quote);
			free(text);
			return description;
		}
		case ENTITY_RESOLUTION:
			return concatenate(4, valueTypeName(resolution->valueType), " \"", resolution->identifier, "\"");
		case PARAMETER_RESOLUTION: {
			char text[32];
			snprintf(text, sizeof(text), "parameter %u", resolution->parameter);
			return concatenate(1, text);
		}
		default:
			return concatenate(1, "error");
	}
}

static void _logExpression(const Expression * expression, const unsigned int instance, const unsigned int depth) {
	const char * type = valueTypeName(expression->staticType);
	switch (expression->type) {
		case FACTOR:
			_logFactor(expression->factor, type, instance, depth);
			break;
		case ARITHMETIC_NEGATION:
		case LOGICAL_NEGATION:
			logDebugging(_logger, "%*soperator \"%s\" (line %d): %s", INDENT(depth), operationSymbol(expression->type), expression->line, type);
			_logExpression(expression->expression, instance, 1 + depth);
			break;
		default:
			logDebugging(_logger, "%*soperator \"%s\" (line %d): %s", INDENT(depth), operationSymbol(expression->type), expression->line, type);
			_logExpression(expression->leftExpression, instance, 1 + depth);
			_logExpression(expression->rightExpression, instance, 1 + depth);
			break;
	}
}

static void _logFactor(const Factor * factor, const char * type, const unsigned int instance, const unsigned int depth) {
	switch (factor->type) {
		case BOOLEAN_FACTOR:
			logDebugging(_logger, "%*s%s (line %d): %s", INDENT(depth), factor->boolean ? "true" : "false", factor->line, type);
			break;
		case CALL_FACTOR:
			logDebugging(_logger, "%*scall \"%s\" (line %d): %s", INDENT(depth), factor->call->identifier, factor->line, type);
			for (const Argument * argument = factor->call->arguments->first; argument != NULL; argument = argument->next) {
				_logExpression(argument->expression, instance, 1 + depth);
			}
			break;
		case EXPRESSION_FACTOR:
			logDebugging(_logger, "%*sparentheses (line %d): %s", INDENT(depth), factor->line, type);
			_logExpression(factor->expression, instance, 1 + depth);
			break;
		case INTEGER_FACTOR:
			logDebugging(_logger, "%*s%d (line %d): %s", INDENT(depth), factor->integer, factor->line, type);
			break;
		case MEMBER_ACCESS_FACTOR:
			logDebugging(_logger, "%*smember \"%s\" (line %d): %s", INDENT(depth), factor->member, factor->line, type);
			break;
		case QUANTITY_FACTOR:
			logDebugging(_logger, "%*s%d %s (line %d): %s", INDENT(depth), factor->quantity->value, _unitSymbol(factor->quantity->unit), factor->line, type);
			break;
		case REFERENCE_FACTOR: {
			char * resolution = _describeResolution(factor->reference, instance);
			logDebugging(_logger, "%*s\"%s%s\" (line %d): %s -> %s", INDENT(depth), factor->reference->identifier,
				factor->reference->index != NULL ? "[...]" : "", factor->line, type, resolution);
			free(resolution);
			break;
		}
		case STRING_FACTOR:
			logDebugging(_logger, "%*sstring (line %d): %s", INDENT(depth), factor->line, type);
			_logString(factor->stringLiteral, instance, 1 + depth);
			break;
	}
}

/** The body of a policy has a single instance: 0. */
static void _logIf(const IfStatement * ifStatement, const char * keyword, const unsigned int depth) {
	logDebugging(_logger, "%*s%s (line %d)", INDENT(depth), keyword, ifStatement->condition->line);
	_logExpression(ifStatement->condition, 0, 1 + depth);
	logDebugging(_logger, "%*sthen", INDENT(depth));
	_logStatements(ifStatement->thenBlock, 1 + depth);
	switch (ifStatement->elseBranchType) {
		case BLOCK_ELSE_BRANCH:
			logDebugging(_logger, "%*selse", INDENT(depth));
			_logStatements(ifStatement->elseBlock, 1 + depth);
			break;
		case IF_ELSE_BRANCH:
			_logIf(ifStatement->elseIfStatement, "else if", depth);
			break;
		case NO_ELSE_BRANCH:
			break;
	}
}

static void _logStatements(const Block * block, const unsigned int depth) {
	for (const Statement * statement = block->first; statement != NULL; statement = statement->next) {
		switch (statement->type) {
			case EXTEND_STATEMENT: {
				char * resolution = _describeResolution(statement->road, 0);
				logDebugging(_logger, "%*sextend (line %d): \"%s%s\" -> %s", INDENT(depth), statement->line,
					statement->road->identifier, statement->road->index != NULL ? "[...]" : "", resolution);
				free(resolution);
				_logExpression(statement->duration, 0, 1 + depth);
				break;
			}
			case IF_STATEMENT:
				_logIf(statement->ifStatement, "if", depth);
				break;
			case KEEP_STATEMENT:
				logDebugging(_logger, "%*skeep (line %d)", INDENT(depth), statement->line);
				break;
			case LOG_STATEMENT:
				logDebugging(_logger, "%*slog (line %d)", INDENT(depth), statement->line);
				_logString(statement->message, 0, 1 + depth);
				break;
		}
	}
}

static void _logString(const StringLiteral * stringLiteral, const unsigned int instance, const unsigned int depth) {
	for (const StringPart * part = stringLiteral->parts->first; part != NULL; part = part->next) {
		if (part->type == FRAGMENT_STRING_PART) {
			logDebugging(_logger, "%*sfragment \"%s\"", INDENT(depth), part->fragment);
		}
		else {
			_logExpression(part->expression, instance, depth);
		}
	}
}

static ValueType _quantityType(const Unit unit) {
	switch (unit) {
		case KILOMETERS_UNIT:
		case METERS_UNIT:
			return DISTANCE_VALUE_TYPE;
		case HOURS_UNIT:
		case MINUTES_UNIT:
		case SECONDS_UNIT:
			return DURATION_VALUE_TYPE;
		case KILOMETERS_PER_HOUR_UNIT:
		case METERS_PER_SECOND_UNIT:
			return SPEED_VALUE_TYPE;
		default:
			return ERROR_VALUE_TYPE;
	}
}

static const char * _unitSymbol(const Unit unit) {
	switch (unit) {
		case HOURS_UNIT: return "h";
		case KILOMETERS_PER_HOUR_UNIT: return "km/h";
		case KILOMETERS_UNIT: return "km";
		case METERS_PER_SECOND_UNIT: return "m/s";
		case METERS_UNIT: return "m";
		case MINUTES_UNIT: return "min";
		case SECONDS_UNIT: return "s";
		default: return "?";
	}
}

/* PUBLIC FUNCTIONS */

void checkBlock(SymbolTable * symbolTable, Block * block, const char * owner) {
	const Check check = {
		.symbolTable = symbolTable,
		.context = POLICY_CONTEXT,
		.owner = owner
	};
	_checkStatements(&check, block);
}

ValueType checkExpression(SymbolTable * symbolTable, Expression * expression, const ExecutionContext context, const char * owner) {
	const Check check = {
		.symbolTable = symbolTable,
		.context = context,
		.owner = owner
	};
	return _checkExpression(&check, expression);
}

void logAnnotations(const Model * model) {
	if (DEBUGGING < _logger->loggingLevel) {
		return;
	}
	for (unsigned int k = 0; k < model->policyCount; ++k) {
		const Policy * policy = model->policies[k];
		logDebugging(_logger, "Annotations of policy \"%s\":", policy->id);
		_logStatements(policy->declaration->block, 1);
	}
	for (unsigned int k = 0; k < model->metricCount; ++k) {
		const Metric * metric = model->metrics[k];
		logDebugging(_logger, "Annotations of metric \"%s\" (instance %u):%s", metric->id, metric->instance,
			metric->argument == NULL ? " no argument" : "");
		if (metric->argument != NULL) {
			_logExpression(metric->argument, metric->instance, 1);
		}
	}
}
