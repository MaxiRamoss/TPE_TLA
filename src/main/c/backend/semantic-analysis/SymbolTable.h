#ifndef SYMBOL_TABLE_HEADER
#define SYMBOL_TABLE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include "../model/Model.h"
#include "Value.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSymbolTableModule();

typedef enum SymbolClass SymbolClass;
typedef struct Scope Scope;
typedef struct Symbol Symbol;
typedef struct SymbolTable SymbolTable;

/**
 * An ARRAY_SYMBOL is the base name of indexed symbols (e.g., "Rivadavia" for
 * "Rivadavia[3]"), and each one of them has its own class.
 */
enum SymbolClass {
	ARRAY_SYMBOL,
	BUILTIN_SYMBOL,
	CONSTANT_SYMBOL,
	FLOW_SYMBOL,
	INTERSECTION_SYMBOL,
	LIGHT_SYMBOL,
	LOOP_VARIABLE_SYMBOL,
	METRIC_SYMBOL,
	PARAMETER_SYMBOL,
	POLICY_SYMBOL,
	ROAD_SYMBOL,
	ROUTE_SYMBOL
};

/**
 * Its name is the resolved one (e.g., "Rivadavia[3]"). A base name is either
 * a scalar or an array, never both.
 */
struct Symbol {
	char * name;
	SymbolClass symbolClass;
	/** 0 for a builtin. */
	int line;
	union {
		/** Of a constant or a loop variable: an ERROR value if it failed. */
		Value * value;
		/**
		 * Arrays are homogeneous. An array is incomplete if the index of some
		 * element failed, and then a missing index produces no error.
		 */
		struct {
			SymbolClass elementClass;
			ValueType elementType;
			bool incomplete;
		};
		struct {
			ValueType parameterType;
			unsigned int parameterIndex;
		};
		Flow * flow;
		Intersection * intersection;
		Light * light;
		Metric * metric;
		Policy * policy;
		Road * road;
		Route * route;
	};
	Symbol * nextInBucket;
	Symbol * nextInScope;
};

struct Scope {
	Symbol ** buckets;
	unsigned int capacity;
	unsigned int size;
	Symbol * first;
	Symbol * last;
	Scope * parent;
};

/**
 * The global scope, then one more for each "for" being unrolled, and one for
 * the parameters of the policy being analyzed.
 */
struct SymbolTable {
	Scope * global;
	Scope * innermost;
};

/** The builtins are already reserved in its global scope. */
SymbolTable * createSymbolTable();

/** The entities of its symbols belong to the model, so they survive. */
void destroySymbolTable(SymbolTable * symbolTable);

void pushScope(SymbolTable * symbolTable);
void popScope(SymbolTable * symbolTable);

/**
 * The name must not be visible yet (the caller checks it with "lookupSymbol").
 * The symbol takes a copy of the name.
 */
Symbol * declareGlobalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line);
Symbol * declareLocalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line);

/** NULL if no visible symbol has that name. */
Symbol * lookupSymbol(const SymbolTable * symbolTable, const char * name);

const char * symbolLabel(const Symbol * symbol);
const char * symbolClassName(const SymbolClass symbolClass);
const char * symbolClassNameWithArticle(const SymbolClass symbolClass);
ValueType symbolClassValueType(const SymbolClass symbolClass);

void logSymbolTable(const SymbolTable * symbolTable);

#endif
