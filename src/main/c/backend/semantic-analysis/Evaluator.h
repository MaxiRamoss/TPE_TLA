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
 * Evaluates an expression in the compilation context (spec-v3 §3.4 and §3.5),
 * folding literals, constants and loop variables, and annotates its static
 * type in the node. Every error is reported with "semanticError", mentioning
 * the owner (the declaration being analyzed, e.g. 'road "AB"'), and produces
 * an ERROR value, which never produces more errors. The returned value must
 * be destroyed.
 */
Value * evaluateExpression(SymbolTable * symbolTable, Expression * expression, const char * owner);

/**
 * The type of the result of an operation (spec-v3 §3.4), or ERROR if the
 * operands are not valid for it. The operands must not be ERROR.
 */
ValueType binaryOperationType(const ExpressionType operation, const ValueType left, const ValueType right);
ValueType unaryOperationType(const ExpressionType operation, const ValueType operand);

/**
 * The symbol of an operation (e.g., "+" or "and"), for messages.
 */
const char * operationSymbol(const ExpressionType operation);

/**
 * True if the identifier is the name of an aggregator (spec-v3 §3.6).
 */
bool isAggregator(const char * identifier);

/**
 * True if the factor is a plain reference to that builtin (e.g., "trip").
 */
bool isBuiltinReference(const SymbolTable * symbolTable, const Factor * factor, const char * identifier);

/**
 * Evaluates the index of a reference, which must be an integer. Returns false
 * if it has an error (already reported, if it had to be).
 */
bool evaluateIndex(SymbolTable * symbolTable, const Reference * reference, const char * owner, int * index);

/**
 * Resolves a reference to a visible symbol, evaluating its index. Reports a
 * reference to an undeclared name or index (V02), and a mix of scalars and
 * arrays (V03). The "expected" description is used in the message of an
 * undeclared name (e.g., "intersection"). Returns NULL if there is an error,
 * which was already reported if it had to be.
 */
Symbol * resolveReference(SymbolTable * symbolTable, const Reference * reference, const char * owner, const char * expected);

/**
 * Resolves a reference to an entity of the expected class. Besides the
 * errors of "resolveReference", it reports a symbol of another class (V03).
 * Returns NULL if there is an error.
 */
Symbol * resolveEntityReference(SymbolTable * symbolTable, const Reference * reference, const SymbolClass expected, const char * owner);

/**
 * The resolved name of an element of an array (e.g., "Rivadavia[3]"), or a
 * copy of the identifier if it is a scalar. Returns a new string.
 */
char * resolvedName(const char * identifier, const bool indexed, const int index);

/**
 * A description of a symbol for messages, such as 'road "Tramo[5]"'. Returns
 * a new string.
 */
char * describeSymbol(const SymbolClass symbolClass, const char * name);

#endif
