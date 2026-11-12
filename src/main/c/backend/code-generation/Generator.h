#ifndef GENERATOR_HEADER
#define GENERATOR_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/configuration/Environment.h"
#include "../../support/language/String.h"
#include "../../support/language/StringBuilder.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/CompilationStatus.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../model/Model.h"
#include "../semantic-analysis/Value.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Initialize module's internal state. */
ModuleDestructor initializeGeneratorModule();

/**
 * The HTML is built in memory and written to stdout at once, so if anything
 * fails (a missing asset, or a marker missing or repeated), stdout is empty.
 */
CompilationStatus executeGenerator(CompilerState * compilerState);

#endif
