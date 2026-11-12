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
 * Generates the self-contained HTML viewer of the model of an accepted
 * program (spec-v3 §5): it reads "viewer.html" and "runtime.js" from the
 * directory VIAL_RUNTIME_PATH ("src/main/runtime" by default), and replaces
 * the markers of the viewer with the "vial-config" JSON, the generated
 * VIAL_LOGIC and the runtime.
 *
 * The entire HTML is built in memory and written to stdout at once, at the
 * end. If anything fails (a missing asset, or a marker that is missing or
 * repeated), nothing is written to stdout, the error is logged, and the
 * status is FAILED.
 */
CompilationStatus executeGenerator(CompilerState * compilerState);

#endif
