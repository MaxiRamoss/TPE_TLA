#ifndef MODEL_HEADER
#define MODEL_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include "../semantic-analysis/Value.h"
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeModelModule();

/**
 * The resolved model of a program: its entities, with every value already
 * folded, in order of declaration. The semantic-analysis phase builds it, and
 * the generator consumes it.
 *
 * Every entity has its resolved identifier (e.g., "Rivadavia[3]"), its label
 * already formatted (NULL if it has no label), and the line of its
 * declaration. Magnitudes are in base units: m, s and m/h. A reference to
 * another entity is a pointer to it. While the analysis reports errors, a
 * reference that could not be resolved is NULL; the generator only receives
 * models without errors.
 */
typedef struct Flow Flow;
typedef struct Intersection Intersection;
typedef struct Light Light;
typedef struct Metric Metric;
typedef struct Model Model;
typedef struct Phase Phase;
typedef struct Policy Policy;
typedef struct PolicyApplication PolicyApplication;
typedef struct Road Road;
typedef struct Route Route;

struct Intersection {
	char * id;
	char * label;
	int line;
	/** The cartesian position, in m (Y grows towards the north). */
	int x;
	int y;
};

struct Road {
	char * id;
	char * label;
	int line;
	Intersection * from;
	Intersection * to;
	/** The explicit length, or else the euclidean distance, in m. */
	int length;
	/** The number of cells (spec-v3 §4.1). */
	int cells;
	/** The speed limit, in m/h (40 km/h by default). */
	int limit;
};

/**
 * A phase of a light: the roads that it enables, and its duration.
 */
struct Phase {
	Road ** roads;
	unsigned int roadCount;
	/** The duration, in s. */
	int duration;
	int line;
};

/**
 * An "apply" of a light: its policy, and its arguments already evaluated.
 * Their arity and types are checked against the policy later (step 4).
 */
struct PolicyApplication {
	Policy * policy;
	Value ** arguments;
	unsigned int argumentCount;
	int line;
};

struct Light {
	char * id;
	char * label;
	int line;
	/** The intersection where the light is. */
	Intersection * at;
	Phase * phases;
	unsigned int phaseCount;
	PolicyApplication * applications;
	unsigned int applicationCount;
};

/**
 * A route: its intersections, in order. The roads of each leg are resolved
 * later (step 3).
 */
struct Route {
	char * id;
	char * label;
	int line;
	Intersection ** waypoints;
	unsigned int waypointCount;
};

/**
 * A flow: the number of vehicles that it spawns in each period, within its
 * window [from, to). Without an explicit window, it lasts the entire
 * simulation. Times are in s.
 */
struct Flow {
	char * id;
	char * label;
	int line;
	Route * route;
	int spawn;
	int every;
	int from;
	int to;
};

/**
 * A policy: the types of its parameters, and its declaration in the AST. Its
 * body is analyzed later (step 4). A policy has no label (always NULL).
 */
struct Policy {
	char * id;
	char * label;
	int line;
	const PolicyDeclaration * declaration;
	ValueType * parameterTypes;
	unsigned int parameterCount;
};

/**
 * A metric: its declaration in the AST, whose aggregator and event are
 * analyzed later (step 4).
 */
struct Metric {
	char * id;
	char * label;
	int line;
	const MetricDeclaration * declaration;
};

struct Model {
	/** The header of the program. */
	struct {
		char * id;
		char * label;
		/** The duration, in s. */
		int duration;
	} simulation;

	Intersection ** intersections;
	unsigned int intersectionCount;
	unsigned int intersectionCapacity;

	Road ** roads;
	unsigned int roadCount;
	unsigned int roadCapacity;

	Light ** lights;
	unsigned int lightCount;
	unsigned int lightCapacity;

	Route ** routes;
	unsigned int routeCount;
	unsigned int routeCapacity;

	Flow ** flows;
	unsigned int flowCount;
	unsigned int flowCapacity;

	Policy ** policies;
	unsigned int policyCount;
	unsigned int policyCapacity;

	Metric ** metrics;
	unsigned int metricCount;
	unsigned int metricCapacity;
};

/**
 * Creates an empty model.
 */
Model * createModel();

/**
 * Appends an entity to the model, which takes ownership of it.
 */
void addFlow(Model * model, Flow * flow);
void addIntersection(Model * model, Intersection * intersection);
void addLight(Model * model, Light * light);
void addMetric(Model * model, Metric * metric);
void addPolicy(Model * model, Policy * policy);
void addRoad(Model * model, Road * road);
void addRoute(Model * model, Route * route);

/**
 * Destructors. An entity that was never added to a model is destroyed with
 * its own destructor; "destroyModel" destroys every entity of the model.
 */
void destroyFlow(Flow * flow);
void destroyIntersection(Intersection * intersection);
void destroyLight(Light * light);
void destroyMetric(Metric * metric);
void destroyModel(Model * model);
void destroyPolicy(Policy * policy);
void destroyRoad(Road * road);
void destroyRoute(Route * route);

/**
 * Logs the entire model at DEBUGGING level.
 */
void logModel(const Model * model);

#endif
