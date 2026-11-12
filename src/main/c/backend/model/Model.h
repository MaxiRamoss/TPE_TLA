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
 * reference that could not be resolved is NULL, and a position or a geometry
 * with an error is marked as invalid; the generator only receives models
 * without errors.
 */
typedef enum Aggregator Aggregator;
typedef enum Event Event;

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

/** The aggregator of a metric (spec-v3 §3.6 and §4.6). */
enum Aggregator {
	AVERAGE_AGGREGATOR,
	COUNT_AGGREGATOR,
	MAXIMUM_AGGREGATOR,
	MINIMUM_AGGREGATOR,
	SUM_AGGREGATOR
};

/** The event after which a metric is evaluated (spec-v3 §3.6). */
enum Event {
	ARRIVAL_EVENT,
	TICK_EVENT
};

struct Intersection {
	char * id;
	char * label;
	int line;
	/** The cartesian position, in m (Y grows towards the north). */
	int x;
	int y;
	/**
	 * False if the position has an error, or is relative to an invalid one.
	 * An invalid position takes no part in any geometric calculation (V08 and
	 * implicit lengths).
	 */
	bool validPosition;
	/** The light at the intersection, or NULL if it has none. */
	Light * light;
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
	/**
	 * False if an endpoint is unresolved or has an invalid position, if the
	 * road goes from an intersection to itself, or if the length has an
	 * error (V07 included). An invalid road takes no part in any geometric
	 * calculation, and its cells are 0.
	 */
	bool validGeometry;
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
 * An "apply" of a light: its policy, and its arguments already evaluated, in
 * base units. Their arity and types match the parameters of the policy (V14).
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
 * A route: its intersections, in order, and the road of each leg between two
 * consecutive intersections (V09). A leg that could not be resolved has a
 * NULL road.
 */
struct Route {
	char * id;
	char * label;
	int line;
	Intersection ** waypoints;
	unsigned int waypointCount;
	/** The road of each leg: there are "waypointCount - 1" of them. */
	Road ** roads;
	unsigned int roadCount;
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
 * A policy: the types of its parameters, and its declaration in the AST,
 * whose body is not copied: its expressions are annotated with their static
 * types and with what each reference resolves to. A policy has no label
 * (always NULL).
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
 * A metric: its aggregator, its event, and the argument of its aggregator,
 * which points to the annotated expression of the AST (NULL for "count").
 * The unit of its result is "s", "m" or "km/h" for a magnitude, and NULL for
 * "count" and for an integer (a static string, never freed).
 *
 * Its instance is the position of its declaration among the instances that
 * the unrolling analyzed (0 if it is not in a "for"): each reference of its
 * argument resolves to "resolutions[instance]".
 */
struct Metric {
	char * id;
	char * label;
	int line;
	const MetricDeclaration * declaration;
	Aggregator aggregator;
	Event event;
	const Expression * argument;
	const char * unit;
	unsigned int instance;
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
 * The names of an aggregator and an event, as written in the source code
 * (e.g., "count" and "arrival").
 */
const char * aggregatorName(const Aggregator aggregator);
const char * eventName(const Event event);

/**
 * Logs the entire model at DEBUGGING level, including the annotations of the
 * bodies of the policies and of the arguments of the metrics.
 */
void logModel(const Model * model);

#endif
