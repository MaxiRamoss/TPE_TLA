#ifndef EVALUATOR_HEADER
#define EVALUATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include "SemanticAnalyzer.h"
#include "SymbolTable.h"
#include "Value.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeEvaluatorModule();

/**
 * Folds an expression known at compile time, and annotates its type. The owner
 * names it in the messages (e.g., 'road "AB"'). The value must be destroyed.
 */
Value * evaluateExpression(SymbolTable * symbolTable, Expression * expression, const char * owner);

/** ERROR if the operation does not accept those operands, which must not be ERROR. */
ValueType binaryOperationType(const ExpressionType operation, const ValueType left, const ValueType right);
ValueType unaryOperationType(const ExpressionType operation, const ValueType operand);

/** As written in Vial (e.g., "and"), for messages. */
const char * operationSymbol(const ExpressionType operation);

bool isAggregator(const char * identifier);

bool isBuiltinReference(const SymbolTable * symbolTable, const Factor * factor, const char * identifier);

/** Returns false if the index has an error, which is already reported if it had to be. */
bool evaluateIndex(SymbolTable * symbolTable, const Reference * reference, const char * owner, int * index);

/**
 * Returns NULL if there is an error, which is already reported if it had to
 * be. The expected kind (e.g., "intersection") names an undeclared symbol.
 */
Symbol * resolveReference(SymbolTable * symbolTable, const Reference * reference, const char * owner, const char * expected);

/** Also reports a symbol of another class. Returns NULL if there is an error. */
Symbol * resolveEntityReference(SymbolTable * symbolTable, const Reference * reference, const SymbolClass expected, const char * owner);

/** "Rivadavia[3]", or the identifier alone if it is not indexed. Returns a new string. */
char * resolvedName(const char * identifier, const bool indexed, const int index);

/** For messages, such as 'road "Tramo[5]"'. Returns a new string. */
char * describeSymbol(const SymbolClass symbolClass, const char * name);

#endif
