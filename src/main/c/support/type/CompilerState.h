#ifndef COMPILER_STATE_HEADER
#define COMPILER_STATE_HEADER

#include "../../backend/model/Model.h"

/**
 * The global state of the compiler. Should transport every data structure
 * needed across the different phases of a compilation.
 */
typedef struct {
	/**
	 * The root node of the AST.
	 */
	void * abstractSyntaxTree;

	/**
	 * The resolved model that the generator consumes. NULL until the
	 * semantic-analysis phase accepts the program.
	 */
	Model * model;
} CompilerState;

#endif
