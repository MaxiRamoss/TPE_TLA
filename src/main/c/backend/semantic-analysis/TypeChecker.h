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
 * Where an expression runs during the simulation: the body of a policy, or
 * the argument of a metric on each event. Only "arrival" can use "trip".
 */
enum ExecutionContext {
	ARRIVAL_CONTEXT,
	POLICY_CONTEXT,
	TICK_CONTEXT
};

/**
 * The parameters of the policy must be already declared in the innermost
 * scope. The owner names it in the messages (e.g., 'policy "P"').
 */
void checkBlock(SymbolTable * symbolTable, Block * block, const char * owner);

/**
 * Annotates the type of each expression and the resolution of each reference
 * but "trip". Nothing is folded: a division by zero returns 0 at run time.
 */
ValueType checkExpression(SymbolTable * symbolTable, Expression * expression, const ExecutionContext context, const char * owner);

void logAnnotations(const Model * model);

#endif
