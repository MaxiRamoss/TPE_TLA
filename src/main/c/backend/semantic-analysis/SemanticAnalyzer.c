#include "SemanticAnalyzer.h"

/* MODULE INTERNAL STATE */

static unsigned int _errors = 0;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_errors = 0;
}

ModuleDestructor initializeSemanticAnalyzerModule() {
	_logger = createLogger("SemanticAnalyzer");
	_errors = 0;
	return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE FUNCTIONS */

static void _analyzeDeclarations(const Declarations * declarations);

/**
 * The name of each type of declaration, for logging purposes.
 */
static const char * const _declarationTypeNames[] = {
	[CONSTANT_DECLARATION] = "CONSTANT",
	[FLOW_DECLARATION] = "FLOW",
	[FOR_DECLARATION] = "FOR",
	[INTERSECTION_DECLARATION] = "INTERSECTION",
	[LIGHT_DECLARATION] = "LIGHT",
	[METRIC_DECLARATION] = "METRIC",
	[POLICY_DECLARATION] = "POLICY",
	[ROAD_DECLARATION] = "ROAD",
	[ROUTE_DECLARATION] = "ROUTE"
};

/**
 * Analyzes a list of declarations, in order of appearance.
 */
static void _analyzeDeclarations(const Declarations * declarations) {
	for (const Declaration * declaration = declarations->first; declaration != NULL; declaration = declaration->next) {
		logDebugging(_logger, "Declaration %s at line %d", _declarationTypeNames[declaration->type], declaration->line);
	}
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(CompilerState * compilerState) {
	const Program * program = compilerState->abstractSyntaxTree;
	_errors = 0;
	_analyzeDeclarations(program->declarations);
	if (0 < _errors) {
		logDebugging(_logger, "The program has %u semantic error(s).", _errors);
		return FAILED;
	}
	compilerState->model = createModel();
	return SUCCEEDED;
}

void semanticError(const int line, const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	char * message = calloc(1 + length, sizeof(char));
	va_start(arguments, format);
	vsnprintf(message, 1 + length, format, arguments);
	va_end(arguments);
	logError(_logger, "Semantic error at line %d: %s", line, message);
	free(message);
	++_errors;
}
