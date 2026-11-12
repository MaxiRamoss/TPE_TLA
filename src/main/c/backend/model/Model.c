#include "Model.h"

/* PUBLIC FUNCTIONS */

Model * createModel() {
	return calloc(1, sizeof(Model));
}

void destroyModel(Model * model) {
	if (model != NULL) {
		free(model);
	}
}
