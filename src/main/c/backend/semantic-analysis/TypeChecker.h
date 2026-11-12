#ifndef TYPE_CHECKER_HEADER
#define TYPE_CHECKER_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include "../model/Model.h"
#include "Evaluator.h"
#include "SemanticAnalyzer.h"
#include "SymbolTable.h"
#include "Value.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeTypeCheckerModule();

typedef enum ExecutionContext ExecutionContext;

/**
 * The contexts of execution (spec-v3 §3.5): the body of a policy, and the
 * argument of the aggregator of a metric, after each event. Every one of them
 * can use "queue", "occupancy" and "elapsed", and only the metrics on arrival
 * can use "trip".
 */
enum ExecutionContext {
	ARRIVAL_CONTEXT,
	POLICY_CONTEXT,
	TICK_CONTEXT
};

/**
 * Checks the statements of the body of a policy, whose parameters must be
 * already declared in the innermost scope (spec-v3 §3.2). Its errors are
 * reported with "semanticError", mentioning the owner (e.g., 'policy "P"').
 */
void checkBlock(SymbolTable * symbolTable, Block * block, const char * owner);

/**
 * Checks an expression in a context of execution (spec-v3 §3.4 to §3.6), and
 * returns its static type: ERROR if it has an error, which never produces
 * more errors. Nothing is folded and no V05 is reported, because a division
 * by zero is resolved at run time (spec-v3 D21).
 *
 * It annotates the static type of every expression, and appends to every
 * reference what it resolves to in this instance (spec-v3 §5.3). The
 * reference "trip" of "trip.travel_time" is not annotated: its member access
 * already identifies it.
 */
ValueType checkExpression(SymbolTable * symbolTable, Expression * expression, const ExecutionContext context, const char * owner);

/**
 * Logs at DEBUGGING level the annotations of the body of every policy, and of
 * the argument of every metric of the model (for its instance).
 */
void logAnnotations(const Model * model);

#endif
