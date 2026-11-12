#include "SymbolTable.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownSymbolTableModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SymbolTable...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeSymbolTableModule() {
	_logger = createLogger("SymbolTable");
	return _shutdownSymbolTableModule;
}

/* PRIVATE FUNCTIONS */

static Scope * _createScope(Scope * parent);
static Symbol * _declareSymbol(Scope * scope, const char * name, const SymbolClass symbolClass, const int line);
static void _destroyScope(Scope * scope);
static void _destroySymbol(Symbol * symbol);
static unsigned int _hash(const char * name);
static void _logScope(const Scope * scope, const unsigned int depth);
static Symbol * _lookupInScope(const Scope * scope, const char * name);
static void _rehash(Scope * scope);

/** The initial number of buckets of each scope. */
static const unsigned int _initialCapacity = 16;

/**
 * The names reserved for the builtins (spec-v3 §3.6). Declaring an entity, a
 * constant or a parameter with one of these names is a duplicate.
 */
static const char * const _builtins[] = {
	"queue", "occupancy", "elapsed", "trip", "count", "sum", "average", "minimum", "maximum", "tick", "arrival"
};

static Scope * _createScope(Scope * parent) {
	Scope * scope = calloc(1, sizeof(Scope));
	scope->capacity = _initialCapacity;
	scope->buckets = calloc(scope->capacity, sizeof(Symbol *));
	scope->parent = parent;
	return scope;
}

/**
 * Inserts a new symbol in a scope, keeping the order of declaration. The load
 * factor of the table stays under 3/4.
 */
static Symbol * _declareSymbol(Scope * scope, const char * name, const SymbolClass symbolClass, const int line) {
	if (4 * (scope->size + 1) > 3 * scope->capacity) {
		_rehash(scope);
	}
	Symbol * symbol = calloc(1, sizeof(Symbol));
	symbol->name = calloc(1 + strlen(name), sizeof(char));
	strcpy(symbol->name, name);
	symbol->symbolClass = symbolClass;
	symbol->line = line;
	const unsigned int bucket = _hash(name) % scope->capacity;
	symbol->nextInBucket = scope->buckets[bucket];
	scope->buckets[bucket] = symbol;
	if (scope->last == NULL) {
		scope->first = symbol;
	}
	else {
		scope->last->nextInScope = symbol;
	}
	scope->last = symbol;
	++scope->size;
	return symbol;
}

static void _destroyScope(Scope * scope) {
	Symbol * symbol = scope->first;
	while (symbol != NULL) {
		Symbol * next = symbol->nextInScope;
		_destroySymbol(symbol);
		symbol = next;
	}
	free(scope->buckets);
	free(scope);
}

/**
 * Destroys a symbol. The entities belong to the model, so they survive.
 */
static void _destroySymbol(Symbol * symbol) {
	if (symbol->symbolClass == CONSTANT_SYMBOL || symbol->symbolClass == LOOP_VARIABLE_SYMBOL) {
		destroyValue(symbol->value);
	}
	free(symbol->name);
	free(symbol);
}

/**
 * The 32-bit FNV-1a hash of a name, byte by byte (so it works with UTF-8).
 *
 * @see http://www.isthe.com/chongo/tech/comp/fnv/
 */
static unsigned int _hash(const char * name) {
	unsigned int hash = 2166136261u;
	for (const unsigned char * byte = (const unsigned char *) name; *byte != '\0'; ++byte) {
		hash ^= *byte;
		hash *= 16777619u;
	}
	return hash;
}

static void _logScope(const Scope * scope, const unsigned int depth) {
	if (scope->parent != NULL) {
		_logScope(scope->parent, depth - 1);
	}
	unsigned int builtins = 0;
	for (const Symbol * symbol = scope->first; symbol != NULL; symbol = symbol->nextInScope) {
		builtins += symbol->symbolClass == BUILTIN_SYMBOL ? 1 : 0;
	}
	logDebugging(_logger, "Scope %u (%s): %u symbol(s), besides %u reserved builtin(s)",
		depth, scope->parent == NULL ? "global" : "for", scope->size - builtins, builtins);
	for (const Symbol * symbol = scope->first; symbol != NULL; symbol = symbol->nextInScope) {
		switch (symbol->symbolClass) {
			case BUILTIN_SYMBOL:
				break;
			case ARRAY_SYMBOL:
				logDebugging(_logger, "    \"%s\": array of %ss (line %d)%s", symbol->name, symbol->elementClass == CONSTANT_SYMBOL
					? valueTypeName(symbol->elementType) : symbolClassName(symbol->elementClass), symbol->line, symbol->incomplete ? ", incomplete" : "");
				break;
			case CONSTANT_SYMBOL:
			case LOOP_VARIABLE_SYMBOL: {
				const Value * value = symbol->value;
				char * text = formatValue(value, NULL);
				if (isMagnitudeType(value->type)) {
					logDebugging(_logger, "    \"%s\": %s (line %d) = %s %d (\"%s\")", symbol->name, symbolClassName(symbol->symbolClass),
						symbol->line, valueTypeName(value->type), value->integer, text);
				}
				else {
					const char * quote = value->type == STRING_VALUE_TYPE ? "\"" : "";
					logDebugging(_logger, "    \"%s\": %s (line %d) = %s %s%s%s", symbol->name, symbolClassName(symbol->symbolClass),
						symbol->line, valueTypeName(value->type), quote, text, quote);
				}
				free(text);
				break;
			}
			default: {
				const char * label = symbolLabel(symbol);
				logDebugging(_logger, "    \"%s\": %s (line %d)%s%s%s", symbol->name, symbolClassName(symbol->symbolClass),
					symbol->line, label == NULL ? "" : ", label \"", label == NULL ? "" : label, label == NULL ? "" : "\"");
				break;
			}
		}
	}
}

static Symbol * _lookupInScope(const Scope * scope, const char * name) {
	const unsigned int bucket = _hash(name) % scope->capacity;
	for (Symbol * symbol = scope->buckets[bucket]; symbol != NULL; symbol = symbol->nextInBucket) {
		if (strcmp(symbol->name, name) == 0) {
			return symbol;
		}
	}
	return NULL;
}

/**
 * Doubles the number of buckets of a scope, and redistributes its symbols.
 */
static void _rehash(Scope * scope) {
	const unsigned int capacity = 2 * scope->capacity;
	Symbol ** buckets = calloc(capacity, sizeof(Symbol *));
	for (Symbol * symbol = scope->first; symbol != NULL; symbol = symbol->nextInScope) {
		const unsigned int bucket = _hash(symbol->name) % capacity;
		symbol->nextInBucket = buckets[bucket];
		buckets[bucket] = symbol;
	}
	free(scope->buckets);
	scope->buckets = buckets;
	scope->capacity = capacity;
}

/* PUBLIC FUNCTIONS */

SymbolTable * createSymbolTable() {
	SymbolTable * symbolTable = calloc(1, sizeof(SymbolTable));
	symbolTable->global = _createScope(NULL);
	symbolTable->innermost = symbolTable->global;
	for (unsigned int k = 0; k < sizeof(_builtins) / sizeof(_builtins[0]); ++k) {
		_declareSymbol(symbolTable->global, _builtins[k], BUILTIN_SYMBOL, 0);
	}
	return symbolTable;
}

void destroySymbolTable(SymbolTable * symbolTable) {
	if (symbolTable != NULL) {
		while (symbolTable->innermost != NULL) {
			Scope * parent = symbolTable->innermost->parent;
			_destroyScope(symbolTable->innermost);
			symbolTable->innermost = parent;
		}
		free(symbolTable);
	}
}

void pushScope(SymbolTable * symbolTable) {
	symbolTable->innermost = _createScope(symbolTable->innermost);
}

void popScope(SymbolTable * symbolTable) {
	Scope * scope = symbolTable->innermost;
	if (scope->parent != NULL) {
		symbolTable->innermost = scope->parent;
		_destroyScope(scope);
	}
}

Symbol * declareGlobalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line) {
	return _declareSymbol(symbolTable->global, name, symbolClass, line);
}

Symbol * declareLocalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line) {
	return _declareSymbol(symbolTable->innermost, name, symbolClass, line);
}

Symbol * lookupSymbol(const SymbolTable * symbolTable, const char * name) {
	for (const Scope * scope = symbolTable->innermost; scope != NULL; scope = scope->parent) {
		Symbol * symbol = _lookupInScope(scope, name);
		if (symbol != NULL) {
			return symbol;
		}
	}
	return NULL;
}

const char * symbolLabel(const Symbol * symbol) {
	switch (symbol->symbolClass) {
		case FLOW_SYMBOL: return symbol->flow->label;
		case INTERSECTION_SYMBOL: return symbol->intersection->label;
		case LIGHT_SYMBOL: return symbol->light->label;
		case METRIC_SYMBOL: return symbol->metric->label;
		case POLICY_SYMBOL: return symbol->policy->label;
		case ROAD_SYMBOL: return symbol->road->label;
		case ROUTE_SYMBOL: return symbol->route->label;
		default: return NULL;
	}
}

const char * symbolClassName(const SymbolClass symbolClass) {
	switch (symbolClass) {
		case ARRAY_SYMBOL: return "array";
		case BUILTIN_SYMBOL: return "builtin";
		case CONSTANT_SYMBOL: return "constant";
		case FLOW_SYMBOL: return "flow";
		case INTERSECTION_SYMBOL: return "intersection";
		case LIGHT_SYMBOL: return "light";
		case LOOP_VARIABLE_SYMBOL: return "loop variable";
		case METRIC_SYMBOL: return "metric";
		case PARAMETER_SYMBOL: return "parameter";
		case POLICY_SYMBOL: return "policy";
		case ROAD_SYMBOL: return "road";
		case ROUTE_SYMBOL: return "route";
		default: return "symbol";
	}
}

const char * symbolClassNameWithArticle(const SymbolClass symbolClass) {
	switch (symbolClass) {
		case ARRAY_SYMBOL: return "an array";
		case BUILTIN_SYMBOL: return "a builtin";
		case CONSTANT_SYMBOL: return "a constant";
		case FLOW_SYMBOL: return "a flow";
		case INTERSECTION_SYMBOL: return "an intersection";
		case LIGHT_SYMBOL: return "a light";
		case LOOP_VARIABLE_SYMBOL: return "a loop variable";
		case METRIC_SYMBOL: return "a metric";
		case PARAMETER_SYMBOL: return "a parameter";
		case POLICY_SYMBOL: return "a policy";
		case ROAD_SYMBOL: return "a road";
		case ROUTE_SYMBOL: return "a route";
		default: return "a symbol";
	}
}

ValueType symbolClassValueType(const SymbolClass symbolClass) {
	switch (symbolClass) {
		case FLOW_SYMBOL: return FLOW_VALUE_TYPE;
		case INTERSECTION_SYMBOL: return INTERSECTION_VALUE_TYPE;
		case LIGHT_SYMBOL: return LIGHT_VALUE_TYPE;
		case METRIC_SYMBOL: return METRIC_VALUE_TYPE;
		case POLICY_SYMBOL: return POLICY_VALUE_TYPE;
		case ROAD_SYMBOL: return ROAD_VALUE_TYPE;
		case ROUTE_SYMBOL: return ROUTE_VALUE_TYPE;
		default: return ERROR_VALUE_TYPE;
	}
}

void logSymbolTable(const SymbolTable * symbolTable) {
	if (DEBUGGING < _logger->loggingLevel) {
		return;
	}
	unsigned int depth = 0;
	for (const Scope * scope = symbolTable->innermost->parent; scope != NULL; scope = scope->parent) {
		++depth;
	}
	_logScope(symbolTable->innermost, depth);
}
