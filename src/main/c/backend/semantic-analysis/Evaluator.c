#include "Evaluator.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownEvaluatorModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Evaluator...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeEvaluatorModule() {
	_logger = createLogger("Evaluator");
	return _shutdownEvaluatorModule;
}

/* PRIVATE FUNCTIONS */

static bool _areEqual(const Value * left, const Value * right);
static Value * _evaluateBinary(SymbolTable * symbolTable, const Expression * expression, const char * owner);
static Value * _evaluateCall(SymbolTable * symbolTable, const Call * call, const char * owner);
static Value * _evaluateFactor(SymbolTable * symbolTable, const Factor * factor, const char * owner);
static Value * _evaluateMember(SymbolTable * symbolTable, const Factor * factor, const char * owner);
static Value * _evaluateQuantity(const Factor * factor, const char * owner);
static Value * _evaluateReference(SymbolTable * symbolTable, const Reference * reference, const char * owner);
static Value * _evaluateString(SymbolTable * symbolTable, const StringLiteral * stringLiteral, const char * owner);
static Value * _evaluateUnary(SymbolTable * symbolTable, const Expression * expression, const char * owner);
static Value * _fold(const Expression * expression, const ValueType type, const Value * left, const Value * right, const char * owner);
static char * _interpolate(const SymbolTable * symbolTable, const Value * value);
static Value * _overflow(const Expression * expression, const char * owner);

static bool _areEqual(const Value * left, const Value * right) {
	if (left->type == STRING_VALUE_TYPE) {
		return strcmp(left->string, right->string) == 0;
	}
	if (isEntityType(left->type)) {
		return strcmp(left->identifier, right->identifier) == 0;
	}
	if (left->type == BOOLEAN_VALUE_TYPE) {
		return left->boolean == right->boolean;
	}
	return left->integer == right->integer;
}

/**
 * There is no short-circuit at compile time: both operands are evaluated, so
 * the errors of both are reported.
 */
static Value * _evaluateBinary(SymbolTable * symbolTable, const Expression * expression, const char * owner) {
	Value * left = evaluateExpression(symbolTable, expression->leftExpression, owner);
	Value * right = evaluateExpression(symbolTable, expression->rightExpression, owner);
	Value * result = NULL;
	if (left->type == ERROR_VALUE_TYPE || right->type == ERROR_VALUE_TYPE) {
		result = createErrorValue();
	}
	else {
		const ValueType type = binaryOperationType(expression->type, left->type, right->type);
		if (type == ERROR_VALUE_TYPE) {
			semanticError(expression->line, "operator \"%s\" cannot be applied to %s and %s in %s", operationSymbol(expression->type),
				valueTypeNameWithArticle(left->type), valueTypeNameWithArticle(right->type), owner);
			result = createErrorValue();
		}
		else {
			result = _fold(expression, type, left, right, owner);
		}
	}
	destroyValue(left);
	destroyValue(right);
	return result;
}

/**
 * No call is valid at compile time. The arguments are evaluated anyway, to
 * report their own errors.
 */
static Value * _evaluateCall(SymbolTable * symbolTable, const Call * call, const char * owner) {
	const Symbol * symbol = lookupSymbol(symbolTable, call->identifier);
	if (symbol == NULL) {
		semanticError(call->line, "%s calls unknown function \"%s\"", owner, call->identifier);
	}
	else if (symbol->symbolClass == POLICY_SYMBOL) {
		semanticError(call->line, "%s uses policy \"%s\" as an expression", owner, call->identifier);
	}
	else if (symbol->symbolClass == BUILTIN_SYMBOL && isAggregator(call->identifier)) {
		semanticError(call->line, "%s uses aggregator \"%s\" outside a metric", owner, call->identifier);
	}
	else if (symbol->symbolClass == BUILTIN_SYMBOL && (strcmp(call->identifier, "queue") == 0 || strcmp(call->identifier, "occupancy") == 0)) {
		semanticError(call->line, "%s uses builtin \"%s\", which is not available at compile time", owner, call->identifier);
	}
	else {
		semanticError(call->line, "%s calls %s \"%s\", which is not a function",
			owner, symbolClassName(symbol->symbolClass), call->identifier);
	}
	for (const Argument * argument = call->arguments->first; argument != NULL; argument = argument->next) {
		destroyValue(evaluateExpression(symbolTable, argument->expression, owner));
	}
	return createErrorValue();
}

static Value * _evaluateFactor(SymbolTable * symbolTable, const Factor * factor, const char * owner) {
	switch (factor->type) {
		case BOOLEAN_FACTOR:
			return createBooleanValue(factor->boolean);
		case CALL_FACTOR:
			return _evaluateCall(symbolTable, factor->call, owner);
		case EXPRESSION_FACTOR:
			return evaluateExpression(symbolTable, factor->expression, owner);
		case INTEGER_FACTOR:
			return createIntegerValue(INTEGER_VALUE_TYPE, factor->integer);
		case MEMBER_ACCESS_FACTOR:
			return _evaluateMember(symbolTable, factor, owner);
		case QUANTITY_FACTOR:
			return _evaluateQuantity(factor, owner);
		case REFERENCE_FACTOR:
			return _evaluateReference(symbolTable, factor->reference, owner);
		case STRING_FACTOR:
			return _evaluateString(symbolTable, factor->stringLiteral, owner);
		default:
			return createErrorValue();
	}
}

static Value * _evaluateMember(SymbolTable * symbolTable, const Factor * factor, const char * owner) {
	if (isBuiltinReference(symbolTable, factor->factor, "trip")) {
		if (strcmp(factor->member, "travel_time") == 0) {
			semanticError(factor->line, "%s uses builtin \"trip\", which is not available at compile time", owner);
		}
		else {
			semanticError(factor->line, "%s accesses unknown member \"%s\" of \"trip\"", owner, factor->member);
		}
		return createErrorValue();
	}
	Value * value = _evaluateFactor(symbolTable, factor->factor, owner);
	if (value->type != ERROR_VALUE_TYPE) {
		semanticError(factor->line, "%s accesses unknown member \"%s\" of %s",
			owner, factor->member, valueTypeNameWithArticle(value->type));
	}
	destroyValue(value);
	return createErrorValue();
}

static Value * _evaluateQuantity(const Factor * factor, const char * owner) {
	ValueType type = ERROR_VALUE_TYPE;
	int scale = 1;
	const char * unit = "";
	switch (factor->quantity->unit) {
		case HOURS_UNIT: type = DURATION_VALUE_TYPE; scale = 3600; unit = "h"; break;
		case KILOMETERS_PER_HOUR_UNIT: type = SPEED_VALUE_TYPE; scale = 1000; unit = "km/h"; break;
		case KILOMETERS_UNIT: type = DISTANCE_VALUE_TYPE; scale = 1000; unit = "km"; break;
		case METERS_PER_SECOND_UNIT: type = SPEED_VALUE_TYPE; scale = 3600; unit = "m/s"; break;
		case METERS_UNIT: type = DISTANCE_VALUE_TYPE; scale = 1; unit = "m"; break;
		case MINUTES_UNIT: type = DURATION_VALUE_TYPE; scale = 60; unit = "min"; break;
		case SECONDS_UNIT: type = DURATION_VALUE_TYPE; scale = 1; unit = "s"; break;
	}
	int value = 0;
	if (__builtin_mul_overflow(factor->quantity->value, scale, &value)) {
		semanticError(factor->line, "quantity \"%d %s\" overflows an integer in %s", factor->quantity->value, unit, owner);
		return createErrorValue();
	}
	return createIntegerValue(type, value);
}

static Value * _evaluateReference(SymbolTable * symbolTable, const Reference * reference, const char * owner) {
	const Symbol * symbol = resolveReference(symbolTable, reference, owner, "identifier");
	if (symbol == NULL) {
		return createErrorValue();
	}
	switch (symbol->symbolClass) {
		case CONSTANT_SYMBOL:
		case LOOP_VARIABLE_SYMBOL:
			return copyValue(symbol->value);
		case BUILTIN_SYMBOL:
			semanticError(reference->line, "%s uses builtin \"%s\", which is not available at compile time", owner, symbol->name);
			return createErrorValue();
		case PARAMETER_SYMBOL:
			semanticError(reference->line, "%s uses parameter \"%s\" where a value known at compile time is needed", owner, symbol->name);
			return createErrorValue();
		default:
			return createEntityValue(symbolClassValueType(symbol->symbolClass), symbol->name);
	}
}

static Value * _evaluateString(SymbolTable * symbolTable, const StringLiteral * stringLiteral, const char * owner) {
	char * string = concatenate(1, "");
	bool failed = false;
	for (const StringPart * part = stringLiteral->parts->first; part != NULL; part = part->next) {
		char * text = NULL;
		if (part->type == FRAGMENT_STRING_PART) {
			text = concatenate(1, part->fragment);
		}
		else {
			Value * value = evaluateExpression(symbolTable, part->expression, owner);
			if (value->type == ERROR_VALUE_TYPE) {
				failed = true;
			}
			else {
				text = _interpolate(symbolTable, value);
			}
			destroyValue(value);
		}
		if (text != NULL) {
			char * extended = concatenate(2, string, text);
			free(string);
			free(text);
			string = extended;
		}
	}
	if (failed) {
		free(string);
		return createErrorValue();
	}
	return createStringValue(string);
}

static Value * _evaluateUnary(SymbolTable * symbolTable, const Expression * expression, const char * owner) {
	Value * operand = evaluateExpression(symbolTable, expression->expression, owner);
	Value * result = NULL;
	if (operand->type == ERROR_VALUE_TYPE) {
		result = createErrorValue();
	}
	else {
		const ValueType type = unaryOperationType(expression->type, operand->type);
		int integer = 0;
		if (type == ERROR_VALUE_TYPE) {
			semanticError(expression->line, "operator \"%s\" cannot be applied to %s in %s",
				operationSymbol(expression->type), valueTypeNameWithArticle(operand->type), owner);
			result = createErrorValue();
		}
		else if (expression->type == LOGICAL_NEGATION) {
			result = createBooleanValue(!operand->boolean);
		}
		else {
			result = __builtin_sub_overflow(0, operand->integer, &integer)
				? _overflow(expression, owner) : createIntegerValue(type, integer);
		}
	}
	destroyValue(operand);
	return result;
}

/** The operands are already valid for the operation. A division truncates towards zero. */
static Value * _fold(const Expression * expression, const ValueType type, const Value * left, const Value * right, const char * owner) {
	int integer = 0;
	switch (expression->type) {
		case ADDITION:
			return __builtin_add_overflow(left->integer, right->integer, &integer)
				? _overflow(expression, owner) : createIntegerValue(type, integer);
		case SUBTRACTION:
			return __builtin_sub_overflow(left->integer, right->integer, &integer)
				? _overflow(expression, owner) : createIntegerValue(type, integer);
		case MULTIPLICATION:
			return __builtin_mul_overflow(left->integer, right->integer, &integer)
				? _overflow(expression, owner) : createIntegerValue(type, integer);
		case DIVISION:
			if (right->integer == 0) {
				semanticError(expression->line, "division by zero in %s", owner);
				return createErrorValue();
			}
			if (left->integer == INT_MIN && right->integer == -1) {
				return _overflow(expression, owner);
			}
			return createIntegerValue(type, left->integer / right->integer);
		case CONJUNCTION:
			return createBooleanValue(left->boolean && right->boolean);
		case DISJUNCTION:
			return createBooleanValue(left->boolean || right->boolean);
		case EQUALITY:
			return createBooleanValue(_areEqual(left, right));
		case INEQUALITY:
			return createBooleanValue(!_areEqual(left, right));
		case GREATER_THAN:
			return createBooleanValue(left->integer > right->integer);
		case GREATER_THAN_OR_EQUAL_TO:
			return createBooleanValue(left->integer >= right->integer);
		case LESS_THAN:
			return createBooleanValue(left->integer < right->integer);
		case LESS_THAN_OR_EQUAL_TO:
			return createBooleanValue(left->integer <= right->integer);
		default:
			return createErrorValue();
	}
}

static char * _interpolate(const SymbolTable * symbolTable, const Value * value) {
	const char * label = NULL;
	if (isEntityType(value->type)) {
		const Symbol * symbol = lookupSymbol(symbolTable, value->identifier);
		label = symbol != NULL ? symbolLabel(symbol) : NULL;
	}
	return formatValue(value, label);
}

static Value * _overflow(const Expression * expression, const char * owner) {
	semanticError(expression->line, "operator \"%s\" overflows an integer in %s", operationSymbol(expression->type), owner);
	return createErrorValue();
}

/* PUBLIC FUNCTIONS */

ValueType binaryOperationType(const ExpressionType operation, const ValueType left, const ValueType right) {
	const bool numeric = left == INTEGER_VALUE_TYPE || isMagnitudeType(left);
	switch (operation) {
		case ADDITION:
		case SUBTRACTION:
			return numeric && left == right ? left : ERROR_VALUE_TYPE;
		case MULTIPLICATION:
			if (left == INTEGER_VALUE_TYPE && (right == INTEGER_VALUE_TYPE || isMagnitudeType(right))) {
				return right;
			}
			return isMagnitudeType(left) && right == INTEGER_VALUE_TYPE ? left : ERROR_VALUE_TYPE;
		case DIVISION:
			return numeric && right == INTEGER_VALUE_TYPE ? left : ERROR_VALUE_TYPE;
		case GREATER_THAN:
		case GREATER_THAN_OR_EQUAL_TO:
		case LESS_THAN:
		case LESS_THAN_OR_EQUAL_TO:
			return numeric && left == right ? BOOLEAN_VALUE_TYPE : ERROR_VALUE_TYPE;
		case EQUALITY:
		case INEQUALITY:
			return left == right && left != ERROR_VALUE_TYPE ? BOOLEAN_VALUE_TYPE : ERROR_VALUE_TYPE;
		case CONJUNCTION:
		case DISJUNCTION:
			return left == BOOLEAN_VALUE_TYPE && right == BOOLEAN_VALUE_TYPE ? BOOLEAN_VALUE_TYPE : ERROR_VALUE_TYPE;
		default:
			return ERROR_VALUE_TYPE;
	}
}

ValueType unaryOperationType(const ExpressionType operation, const ValueType operand) {
	switch (operation) {
		case ARITHMETIC_NEGATION:
			return operand == INTEGER_VALUE_TYPE || isMagnitudeType(operand) ? operand : ERROR_VALUE_TYPE;
		case LOGICAL_NEGATION:
			return operand == BOOLEAN_VALUE_TYPE ? BOOLEAN_VALUE_TYPE : ERROR_VALUE_TYPE;
		default:
			return ERROR_VALUE_TYPE;
	}
}

bool isAggregator(const char * identifier) {
	return strcmp(identifier, "count") == 0
		|| strcmp(identifier, "sum") == 0
		|| strcmp(identifier, "average") == 0
		|| strcmp(identifier, "minimum") == 0
		|| strcmp(identifier, "maximum") == 0;
}

bool isBuiltinReference(const SymbolTable * symbolTable, const Factor * factor, const char * identifier) {
	if (factor->type != REFERENCE_FACTOR || factor->reference->index != NULL) {
		return false;
	}
	if (strcmp(factor->reference->identifier, identifier) != 0) {
		return false;
	}
	const Symbol * symbol = lookupSymbol(symbolTable, identifier);
	return symbol != NULL && symbol->symbolClass == BUILTIN_SYMBOL;
}

const char * operationSymbol(const ExpressionType operation) {
	switch (operation) {
		case ADDITION: return "+";
		case ARITHMETIC_NEGATION: return "-";
		case CONJUNCTION: return "and";
		case DISJUNCTION: return "or";
		case DIVISION: return "/";
		case EQUALITY: return "=";
		case GREATER_THAN: return ">";
		case GREATER_THAN_OR_EQUAL_TO: return ">=";
		case INEQUALITY: return "!=";
		case LESS_THAN: return "<";
		case LESS_THAN_OR_EQUAL_TO: return "<=";
		case LOGICAL_NEGATION: return "not";
		case MULTIPLICATION: return "*";
		case SUBTRACTION: return "-";
		default: return "?";
	}
}

Value * evaluateExpression(SymbolTable * symbolTable, Expression * expression, const char * owner) {
	Value * value = NULL;
	switch (expression->type) {
		case FACTOR:
			value = _evaluateFactor(symbolTable, expression->factor, owner);
			break;
		case ARITHMETIC_NEGATION:
		case LOGICAL_NEGATION:
			value = _evaluateUnary(symbolTable, expression, owner);
			break;
		default:
			value = _evaluateBinary(symbolTable, expression, owner);
			break;
	}
	expression->staticType = value->type;
	return value;
}

bool evaluateIndex(SymbolTable * symbolTable, const Reference * reference, const char * owner, int * index) {
	Value * value = evaluateExpression(symbolTable, reference->index, owner);
	bool succeeded = false;
	if (value->type == INTEGER_VALUE_TYPE) {
		*index = value->integer;
		succeeded = true;
	}
	else if (value->type != ERROR_VALUE_TYPE) {
		semanticError(reference->line, "the index of \"%s\" in %s must be an integer, but it is %s",
			reference->identifier, owner, valueTypeNameWithArticle(value->type));
	}
	destroyValue(value);
	return succeeded;
}

Symbol * resolveReference(SymbolTable * symbolTable, const Reference * reference, const char * owner, const char * expected) {
	const bool indexed = reference->index != NULL;
	int index = 0;
	const bool validIndex = indexed && evaluateIndex(symbolTable, reference, owner, &index);
	Symbol * base = lookupSymbol(symbolTable, reference->identifier);
	if (base == NULL) {
		char * name = resolvedName(reference->identifier, validIndex, index);
		semanticError(reference->line, "%s references undeclared %s \"%s\"", owner, expected, name);
		free(name);
		return NULL;
	}
	if (base->symbolClass == ARRAY_SYMBOL && !indexed) {
		semanticError(reference->line, "%s references array \"%s\" without an index", owner, base->name);
		return NULL;
	}
	if (base->symbolClass != ARRAY_SYMBOL && indexed) {
		semanticError(reference->line, "%s indexes \"%s\", which is %s, not an array",
			owner, base->name, symbolClassNameWithArticle(base->symbolClass));
		return NULL;
	}
	if (!indexed) {
		return base;
	}
	if (!validIndex) {
		return NULL;
	}
	char * name = resolvedName(reference->identifier, true, index);
	Symbol * symbol = lookupSymbol(symbolTable, name);
	if (symbol == NULL && !base->incomplete) {
		semanticError(reference->line, "%s references undeclared %s \"%s\"", owner, expected, name);
	}
	free(name);
	return symbol;
}

Symbol * resolveEntityReference(SymbolTable * symbolTable, const Reference * reference, const SymbolClass expected, const char * owner) {
	Symbol * symbol = resolveReference(symbolTable, reference, owner, symbolClassName(expected));
	if (symbol == NULL) {
		return NULL;
	}
	if (symbol->symbolClass != expected) {
		semanticError(reference->line, "%s references %s \"%s\" where %s is expected",
			owner, symbolClassName(symbol->symbolClass), symbol->name, symbolClassNameWithArticle(expected));
		return NULL;
	}
	return symbol;
}

char * resolvedName(const char * identifier, const bool indexed, const int index) {
	if (!indexed) {
		return concatenate(1, identifier);
	}
	char suffix[16];
	snprintf(suffix, sizeof(suffix), "[%d]", index);
	return concatenate(2, identifier, suffix);
}

char * describeSymbol(const SymbolClass symbolClass, const char * name) {
	return concatenate(4, symbolClassName(symbolClass), " \"", name, "\"");
}
