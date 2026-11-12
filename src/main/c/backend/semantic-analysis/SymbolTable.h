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
 * The class of a symbol (spec-v3 §3.2). An array is not a class of entity: it
 * is the base name of a group of indexed symbols (e.g., "Rivadavia" for
 * "Rivadavia[0]", "Rivadavia[1]", ...), each one with its own class.
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
 * A symbol, indexed by its resolved name: "A" for a scalar, "Rivadavia[3]"
 * for an element of an array, and "Rivadavia" for the base name of that
 * array. A base name is either a scalar or an array, never both.
 */
struct Symbol {
	/** The resolved name, which is the key of the symbol in its scope. */
	char * name;
	SymbolClass symbolClass;
	/** The line of its declaration (0 for a builtin). */
	int line;
	union {
		/** CONSTANT_SYMBOL and LOOP_VARIABLE_SYMBOL (ERROR if it failed). */
		Value * value;
		/**
		 * ARRAY_SYMBOL: arrays are homogeneous, so every element has the
		 * same class (and, if they are constants, the same type). It is
		 * incomplete if the index of some declaration of an element failed,
		 * so a reference to an undeclared element produces no error.
		 */
		struct {
			SymbolClass elementClass;
			ValueType elementType;
			bool incomplete;
		};
		/** The entity of the model, for the classes of entities. */
		Flow * flow;
		Intersection * intersection;
		Light * light;
		Metric * metric;
		Policy * policy;
		Road * road;
		Route * route;
	};
	/** The next symbol in the same bucket of the hash table. */
	Symbol * nextInBucket;
	/** The next symbol of the same scope, in order of declaration. */
	Symbol * nextInScope;
};

/**
 * A scope: a hash table with separate chaining, that also keeps its symbols
 * in order of declaration.
 */
struct Scope {
	Symbol ** buckets;
	unsigned int capacity;
	unsigned int size;
	Symbol * first;
	Symbol * last;
	/** The enclosing scope (NULL for the global scope). */
	Scope * parent;
};

/**
 * A stack of scopes: global, and one more for each "for" being unrolled (and,
 * from step 4 on, for the parameters of a policy).
 */
struct SymbolTable {
	Scope * global;
	Scope * innermost;
};

/**
 * Creates a symbol table with only the global scope, where the builtins
 * (spec-v3 §3.6) are already reserved.
 */
SymbolTable * createSymbolTable();

/** Destroys a symbol table, its scopes and its symbols (not the entities). */
void destroySymbolTable(SymbolTable * symbolTable);

/** Pushes a new innermost scope. */
void pushScope(SymbolTable * symbolTable);

/** Pops the innermost scope, destroying its symbols. */
void popScope(SymbolTable * symbolTable);

/**
 * Declares a new symbol in the global scope, or in the innermost one. The
 * name must not be visible yet (the caller must check it with
 * "lookupSymbol"). The symbol takes a copy of the name.
 */
Symbol * declareGlobalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line);
Symbol * declareLocalSymbol(SymbolTable * symbolTable, const char * name, const SymbolClass symbolClass, const int line);

/**
 * Looks up a visible symbol, from the innermost scope to the global one.
 * Returns NULL if there is none.
 */
Symbol * lookupSymbol(const SymbolTable * symbolTable, const char * name);

/** The label of the entity of a symbol, or NULL if it has none. */
const char * symbolLabel(const Symbol * symbol);

/** The name of a class (e.g., "loop variable"), for messages. */
const char * symbolClassName(const SymbolClass symbolClass);

/** The name of a class with its indefinite article (e.g., "an array"). */
const char * symbolClassNameWithArticle(const SymbolClass symbolClass);

/** The type of the value of a reference to an entity of that class. */
ValueType symbolClassValueType(const SymbolClass symbolClass);

/**
 * Logs the symbols of every scope at DEBUGGING level, in order of declaration
 * (except the builtins).
 */
void logSymbolTable(const SymbolTable * symbolTable);

#endif
