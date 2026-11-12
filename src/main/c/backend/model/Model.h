#ifndef MODEL_HEADER
#define MODEL_HEADER

#include <stdlib.h>

/**
 * The resolved model of a program: its entities, with every value already
 * folded, in order of declaration. The semantic-analysis phase builds it, and
 * the generator consumes it.
 */
typedef struct Model Model;

struct Model {
	/** A placeholder, until the model has its entities. */
	int placeholder;
};

/**
 * Creates an empty model.
 */
Model * createModel();

/**
 * Destroys a model and its resources.
 */
void destroyModel(Model * model);

#endif
