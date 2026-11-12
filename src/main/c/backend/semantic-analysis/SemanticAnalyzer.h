#ifndef SEMANTIC_ANALYZER_HEADER
#define SEMANTIC_ANALYZER_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../model/Model.h"
#include <stdarg.h>

/** Initialize module's internal state. */
ModuleDestructor initializeSemanticAnalyzerModule();

/**
 * If the program is valid, it leaves the resolved model in the compiler state.
 * Otherwise, it reports every semantic error.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

/**
 * For the semantic-analysis phase only. An error identical to one already
 * reported (same line and message) is ignored.
 */
void semanticError(const int line, const char * format, ...) __attribute__((format(printf, 2, 3)));

#endif
