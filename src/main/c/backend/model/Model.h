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
 * Magnitudes are in base units: m, s and m/h. A reference that could not be
 * resolved is NULL, which only happens in a program with semantic errors.
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

enum Aggregator {
	AVERAGE_AGGREGATOR,
	COUNT_AGGREGATOR,
	MAXIMUM_AGGREGATOR,
	MINIMUM_AGGREGATOR,
	SUM_AGGREGATOR
};

enum Event {
	ARRIVAL_EVENT,
	TICK_EVENT
};

struct Intersection {
	char * id;
	char * label;
	int line;
	/** Y grows towards the north. */
	int x;
	int y;
	/**
	 * False if the position has an error or is relative to an invalid one, so
	 * it takes no part in any geometric calculation.
	 */
	bool validPosition;
	Light * light;
};

struct Road {
	char * id;
	char * label;
	int line;
	Intersection * from;
	Intersection * to;
	/** The explicit length, or else the euclidean distance between its ends. */
	int length;
	int cells;
	int limit;
	/**
	 * False if an endpoint is unresolved or has an invalid position, if both
	 * are the same, or if the length has an error. Then its cells are 0.
	 */
	bool validGeometry;
};

struct Phase {
	Road ** roads;
	unsigned int roadCount;
	int duration;
	int line;
};

/** An "apply" of a light, with its arguments already evaluated. */
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
	Intersection * at;
	Phase * phases;
	unsigned int phaseCount;
	PolicyApplication * applications;
	unsigned int applicationCount;
};

struct Route {
	char * id;
	char * label;
	int line;
	Intersection ** waypoints;
	unsigned int waypointCount;
	/** The road between each pair of consecutive waypoints. */
	Road ** roads;
	unsigned int roadCount;
};

/**
 * It spawns "spawn" vehicles every "every" seconds within [from, to), which
 * is the entire simulation if the program gives no window.
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
 * Its body is not copied: the expressions of the AST already carry their
 * static types and what each reference resolves to. Its label is always NULL.
 */
struct Policy {
	char * id;
	char * label;
	int line;
	const PolicyDeclaration * declaration;
	ValueType * parameterTypes;
	unsigned int parameterCount;
};

struct Metric {
	char * id;
	char * label;
	int line;
	const MetricDeclaration * declaration;
	Aggregator aggregator;
	Event event;
	/** NULL for "count". */
	const Expression * argument;
	/** "s", "m" or "km/h" for a magnitude, or else NULL. A static string. */
	const char * unit;
	/**
	 * The iteration of the unrolling that declared it (0 outside a "for"):
	 * the references of its argument resolve to "resolutions[instance]".
	 */
	unsigned int instance;
};

struct Model {
	struct {
		char * id;
		char * label;
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

Model * createModel();

/** The model takes ownership of the entity. */
void addFlow(Model * model, Flow * flow);
void addIntersection(Model * model, Intersection * intersection);
void addLight(Model * model, Light * light);
void addMetric(Model * model, Metric * metric);
void addPolicy(Model * model, Policy * policy);
void addRoad(Model * model, Road * road);
void addRoute(Model * model, Route * route);

/**
 * "destroyModel" destroys its entities too: the other destructors are only for
 * an entity that was never added to a model.
 */
void destroyFlow(Flow * flow);
void destroyIntersection(Intersection * intersection);
void destroyLight(Light * light);
void destroyMetric(Metric * metric);
void destroyModel(Model * model);
void destroyPolicy(Policy * policy);
void destroyRoad(Road * road);
void destroyRoute(Route * route);

const char * aggregatorName(const Aggregator aggregator);
const char * eventName(const Event event);

void logModel(const Model * model);

#endif
