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
 * Analyzes the AST of the current compiler state. If the program is valid, it
 * builds the resolved model; otherwise, it reports every semantic error, and
 * returns FAILED.
 */
CompilationStatus executeSemanticAnalysis(CompilerState * compilerState);

/**
 * Reports a semantic error at the specified line of the source code, and
 * counts it. For internal use of the semantic-analysis phase only.
 */
void semanticError(const int line, const char * format, ...) __attribute__((format(printf, 2, 3)));

#endif
