#include "Model.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownModelModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: Model...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeModelModule() {
	_logger = createLogger("Model");
	return _shutdownModelModule;
}

/* PRIVATE FUNCTIONS */

static void * _ensureCapacity(void * items, const unsigned int count, unsigned int * capacity);
static char * _labelOrEmpty(const char * label);

/** The initial capacity of each array of entities. */
static const unsigned int _initialCapacity = 8;

/**
 * Grows an array of pointers if it is full, doubling its capacity. Returns
 * the array, which may have moved.
 */
static void * _ensureCapacity(void * items, const unsigned int count, unsigned int * capacity) {
	if (count < *capacity) {
		return items;
	}
	*capacity = (*capacity == 0) ? _initialCapacity : 2 * *capacity;
	return realloc(items, *capacity * sizeof(void *));
}

/**
 * A label for logging purposes: an empty string if there is no label.
 */
static char * _labelOrEmpty(const char * label) {
	if (label == NULL) {
		return concatenate(1, "");
	}
	return concatenate(3, " label \"", label, "\"");
}

/** The identifier of a referenced entity, or "?" if it was not resolved. */
#define ID_OF(entity) ((entity) != NULL ? (entity)->id : "?")

/* PUBLIC FUNCTIONS */

Model * createModel() {
	return calloc(1, sizeof(Model));
}

void addFlow(Model * model, Flow * flow) {
	model->flows = _ensureCapacity(model->flows, model->flowCount, &model->flowCapacity);
	model->flows[model->flowCount++] = flow;
}

void addIntersection(Model * model, Intersection * intersection) {
	model->intersections = _ensureCapacity(model->intersections, model->intersectionCount, &model->intersectionCapacity);
	model->intersections[model->intersectionCount++] = intersection;
}

void addLight(Model * model, Light * light) {
	model->lights = _ensureCapacity(model->lights, model->lightCount, &model->lightCapacity);
	model->lights[model->lightCount++] = light;
}

void addMetric(Model * model, Metric * metric) {
	model->metrics = _ensureCapacity(model->metrics, model->metricCount, &model->metricCapacity);
	model->metrics[model->metricCount++] = metric;
}

void addPolicy(Model * model, Policy * policy) {
	model->policies = _ensureCapacity(model->policies, model->policyCount, &model->policyCapacity);
	model->policies[model->policyCount++] = policy;
}

void addRoad(Model * model, Road * road) {
	model->roads = _ensureCapacity(model->roads, model->roadCount, &model->roadCapacity);
	model->roads[model->roadCount++] = road;
}

void addRoute(Model * model, Route * route) {
	model->routes = _ensureCapacity(model->routes, model->routeCount, &model->routeCapacity);
	model->routes[model->routeCount++] = route;
}

void destroyFlow(Flow * flow) {
	if (flow != NULL) {
		free(flow->id);
		free(flow->label);
		free(flow);
	}
}

void destroyIntersection(Intersection * intersection) {
	if (intersection != NULL) {
		free(intersection->id);
		free(intersection->label);
		free(intersection);
	}
}

void destroyLight(Light * light) {
	if (light != NULL) {
		free(light->id);
		free(light->label);
		for (unsigned int k = 0; k < light->phaseCount; ++k) {
			free(light->phases[k].roads);
		}
		free(light->phases);
		for (unsigned int k = 0; k < light->applicationCount; ++k) {
			const PolicyApplication * application = &light->applications[k];
			for (unsigned int j = 0; j < application->argumentCount; ++j) {
				destroyValue(application->arguments[j]);
			}
			free(application->arguments);
		}
		free(light->applications);
		free(light);
	}
}

void destroyMetric(Metric * metric) {
	if (metric != NULL) {
		free(metric->id);
		free(metric->label);
		free(metric);
	}
}

void destroyModel(Model * model) {
	if (model != NULL) {
		free(model->simulation.id);
		free(model->simulation.label);
		for (unsigned int k = 0; k < model->intersectionCount; ++k) {
			destroyIntersection(model->intersections[k]);
		}
		free(model->intersections);
		for (unsigned int k = 0; k < model->roadCount; ++k) {
			destroyRoad(model->roads[k]);
		}
		free(model->roads);
		for (unsigned int k = 0; k < model->lightCount; ++k) {
			destroyLight(model->lights[k]);
		}
		free(model->lights);
		for (unsigned int k = 0; k < model->routeCount; ++k) {
			destroyRoute(model->routes[k]);
		}
		free(model->routes);
		for (unsigned int k = 0; k < model->flowCount; ++k) {
			destroyFlow(model->flows[k]);
		}
		free(model->flows);
		for (unsigned int k = 0; k < model->policyCount; ++k) {
			destroyPolicy(model->policies[k]);
		}
		free(model->policies);
		for (unsigned int k = 0; k < model->metricCount; ++k) {
			destroyMetric(model->metrics[k]);
		}
		free(model->metrics);
		free(model);
	}
}

void destroyPolicy(Policy * policy) {
	if (policy != NULL) {
		free(policy->id);
		free(policy->label);
		free(policy->parameterTypes);
		free(policy);
	}
}

void destroyRoad(Road * road) {
	if (road != NULL) {
		free(road->id);
		free(road->label);
		free(road);
	}
}

void destroyRoute(Route * route) {
	if (route != NULL) {
		free(route->id);
		free(route->label);
		free(route->waypoints);
		free(route->roads);
		free(route);
	}
}

void logModel(const Model * model) {
	if (DEBUGGING < _logger->loggingLevel) {
		return;
	}
	char * label = _labelOrEmpty(model->simulation.label);
	logDebugging(_logger, "Simulation \"%s\"%s: duration %d s", model->simulation.id, label, model->simulation.duration);
	free(label);
	for (unsigned int k = 0; k < model->intersectionCount; ++k) {
		const Intersection * intersection = model->intersections[k];
		label = _labelOrEmpty(intersection->label);
		logDebugging(_logger, "Intersection \"%s\"%s (line %d): at (%d m, %d m)%s, light: %s",
			intersection->id, label, intersection->line, intersection->x, intersection->y,
			intersection->validPosition ? "" : " (invalid)",
			intersection->light != NULL ? intersection->light->id : "none");
		free(label);
	}
	for (unsigned int k = 0; k < model->roadCount; ++k) {
		const Road * road = model->roads[k];
		label = _labelOrEmpty(road->label);
		logDebugging(_logger, "Road \"%s\"%s (line %d): from \"%s\" to \"%s\", length %d m, cells %d, limit %d m/h%s",
			road->id, label, road->line, ID_OF(road->from), ID_OF(road->to), road->length, road->cells, road->limit,
			road->validGeometry ? "" : " (invalid geometry)");
		free(label);
	}
	for (unsigned int k = 0; k < model->lightCount; ++k) {
		const Light * light = model->lights[k];
		label = _labelOrEmpty(light->label);
		logDebugging(_logger, "Light \"%s\"%s (line %d): at \"%s\", %u phase(s), %u policy application(s)",
			light->id, label, light->line, ID_OF(light->at), light->phaseCount, light->applicationCount);
		free(label);
		for (unsigned int j = 0; j < light->phaseCount; ++j) {
			const Phase * phase = &light->phases[j];
			logDebugging(_logger, "    Phase %u (line %d): %d s", 1 + j, phase->line, phase->duration);
			for (unsigned int i = 0; i < phase->roadCount; ++i) {
				logDebugging(_logger, "        Road \"%s\"", ID_OF(phase->roads[i]));
			}
		}
		for (unsigned int j = 0; j < light->applicationCount; ++j) {
			const PolicyApplication * application = &light->applications[j];
			logDebugging(_logger, "    Apply \"%s\" (line %d)", ID_OF(application->policy), application->line);
			for (unsigned int i = 0; i < application->argumentCount; ++i) {
				const Value * argument = application->arguments[i];
				char * text = formatValue(argument, NULL);
				logDebugging(_logger, "        Argument %u: %s %s", 1 + i, valueTypeName(argument->type), text);
				free(text);
			}
		}
	}
	for (unsigned int k = 0; k < model->routeCount; ++k) {
		const Route * route = model->routes[k];
		label = _labelOrEmpty(route->label);
		logDebugging(_logger, "Route \"%s\"%s (line %d): %u waypoint(s)", route->id, label, route->line, route->waypointCount);
		free(label);
		for (unsigned int j = 0; j < route->waypointCount; ++j) {
			logDebugging(_logger, "    Waypoint \"%s\"", ID_OF(route->waypoints[j]));
		}
		for (unsigned int j = 0; j < route->roadCount; ++j) {
			logDebugging(_logger, "    Leg %u: road \"%s\"", 1 + j, ID_OF(route->roads[j]));
		}
	}
	for (unsigned int k = 0; k < model->flowCount; ++k) {
		const Flow * flow = model->flows[k];
		label = _labelOrEmpty(flow->label);
		logDebugging(_logger, "Flow \"%s\"%s (line %d): along \"%s\", spawn %d every %d s, from %d s to %d s",
			flow->id, label, flow->line, ID_OF(flow->route), flow->spawn, flow->every, flow->from, flow->to);
		free(label);
	}
	for (unsigned int k = 0; k < model->policyCount; ++k) {
		const Policy * policy = model->policies[k];
		logDebugging(_logger, "Policy \"%s\" (line %d): %u parameter(s)", policy->id, policy->line, policy->parameterCount);
		for (unsigned int j = 0; j < policy->parameterCount; ++j) {
			logDebugging(_logger, "    Parameter %u: %s", 1 + j, valueTypeName(policy->parameterTypes[j]));
		}
	}
	for (unsigned int k = 0; k < model->metricCount; ++k) {
		const Metric * metric = model->metrics[k];
		label = _labelOrEmpty(metric->label);
		logDebugging(_logger, "Metric \"%s\"%s (line %d)", metric->id, label, metric->line);
		free(label);
	}
}
