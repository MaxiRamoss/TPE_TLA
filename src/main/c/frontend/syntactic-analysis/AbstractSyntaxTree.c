#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
		destroyLogger(_logger);
		_logger = NULL;
	}
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
	_logger = createLogger("AbstractSyntaxTree");
	return _shutdownAbstractSyntaxTreeModule;
}

/* PUBLIC FUNCTIONS */

void destroyArgument(Argument * argument) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (argument != NULL) {
		destroyExpression(argument->expression);
		free(argument);
	}
}

void destroyArguments(Arguments * arguments) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (arguments != NULL) {
		Argument * argument = arguments->first;
		while (argument != NULL) {
			Argument * next = argument->next;
			destroyArgument(argument);
			argument = next;
		}
		free(arguments);
	}
}

void destroyCall(Call * call) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (call != NULL) {
		free(call->identifier);
		destroyArguments(call->arguments);
		free(call);
	}
}

void destroyConstantDeclaration(ConstantDeclaration * constantDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (constantDeclaration != NULL) {
		destroyReference(constantDeclaration->reference);
		destroyExpression(constantDeclaration->expression);
		free(constantDeclaration);
	}
}

void destroyDeclaration(Declaration * declaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (declaration != NULL) {
		switch (declaration->type) {
			case CONSTANT_DECLARATION:
				destroyConstantDeclaration(declaration->constantDeclaration);
				break;
			case FLOW_DECLARATION:
				destroyFlowDeclaration(declaration->flowDeclaration);
				break;
			case INTERSECTION_DECLARATION:
				destroyIntersectionDeclaration(declaration->intersectionDeclaration);
				break;
			case ROAD_DECLARATION:
				destroyRoadDeclaration(declaration->roadDeclaration);
				break;
			case ROUTE_DECLARATION:
				destroyRouteDeclaration(declaration->routeDeclaration);
				break;
			default:
				logError(_logger, "The specified declaration type is unknown: %d", declaration->type);
				break;
		}
		free(declaration);
	}
}

void destroyDeclarations(Declarations * declarations) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (declarations != NULL) {
		Declaration * declaration = declarations->first;
		while (declaration != NULL) {
			Declaration * next = declaration->next;
			destroyDeclaration(declaration);
			declaration = next;
		}
		free(declarations);
	}
}

void destroyExpression(Expression * expression) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (expression != NULL) {
		switch (expression->type) {
			case ADDITION:
			case CONJUNCTION:
			case DISJUNCTION:
			case DIVISION:
			case EQUALITY:
			case GREATER_THAN:
			case GREATER_THAN_OR_EQUAL_TO:
			case INEQUALITY:
			case LESS_THAN:
			case LESS_THAN_OR_EQUAL_TO:
			case MULTIPLICATION:
			case SUBTRACTION:
				destroyExpression(expression->leftExpression);
				destroyExpression(expression->rightExpression);
				break;
			case ARITHMETIC_NEGATION:
			case LOGICAL_NEGATION:
				destroyExpression(expression->expression);
				break;
			case FACTOR:
				destroyFactor(expression->factor);
				break;
			default:
				logError(_logger, "The specified expression type is unknown: %d", expression->type);
				break;
		}
		free(expression);
	}
}

void destroyFactor(Factor * factor) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (factor != NULL) {
		switch (factor->type) {
			case BOOLEAN_FACTOR:
			case INTEGER_FACTOR:
				break;
			case CALL_FACTOR:
				destroyCall(factor->call);
				break;
			case EXPRESSION_FACTOR:
				destroyExpression(factor->expression);
				break;
			case MEMBER_ACCESS_FACTOR:
				destroyFactor(factor->factor);
				free(factor->member);
				break;
			case QUANTITY_FACTOR:
				destroyQuantity(factor->quantity);
				break;
			case REFERENCE_FACTOR:
				destroyReference(factor->reference);
				break;
			case STRING_FACTOR:
				destroyStringLiteral(factor->stringLiteral);
				break;
			default:
				logError(_logger, "The specified factor type is unknown: %d", factor->type);
				break;
		}
		free(factor);
	}
}

void destroyFlowDeclaration(FlowDeclaration * flowDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (flowDeclaration != NULL) {
		destroyReference(flowDeclaration->reference);
		destroyReference(flowDeclaration->route);
		destroyExpression(flowDeclaration->label);
		destroyExpression(flowDeclaration->amount);
		destroyExpression(flowDeclaration->period);
		destroyExpression(flowDeclaration->from);
		destroyExpression(flowDeclaration->to);
		free(flowDeclaration);
	}
}

void destroyIntersectionDeclaration(IntersectionDeclaration * intersectionDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (intersectionDeclaration != NULL) {
		destroyReference(intersectionDeclaration->reference);
		destroyPosition(intersectionDeclaration->position);
		destroyExpression(intersectionDeclaration->label);
		free(intersectionDeclaration);
	}
}

void destroyPath(Path * path) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (path != NULL) {
		Waypoint * waypoint = path->first;
		while (waypoint != NULL) {
			Waypoint * next = waypoint->next;
			destroyWaypoint(waypoint);
			waypoint = next;
		}
		free(path);
	}
}

void destroyPosition(Position * position) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (position != NULL) {
		switch (position->type) {
			case ABSOLUTE_POSITION:
				destroyExpression(position->x);
				destroyExpression(position->y);
				break;
			case RELATIVE_POSITION:
				destroyExpression(position->distance);
				destroyReference(position->origin);
				break;
			default:
				logError(_logger, "The specified position type is unknown: %d", position->type);
				break;
		}
		free(position);
	}
}

void destroyProgram(Program * program) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (program != NULL) {
		destroySimulation(program->simulation);
		destroyDeclarations(program->declarations);
		free(program);
	}
}

void destroyQuantity(Quantity * quantity) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (quantity != NULL) {
		free(quantity);
	}
}

void destroyReference(Reference * reference) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (reference != NULL) {
		free(reference->identifier);
		destroyExpression(reference->index);
		free(reference);
	}
}

void destroyRoadDeclaration(RoadDeclaration * roadDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (roadDeclaration != NULL) {
		destroyReference(roadDeclaration->reference);
		destroyReference(roadDeclaration->from);
		destroyReference(roadDeclaration->to);
		destroyExpression(roadDeclaration->length);
		destroyExpression(roadDeclaration->limit);
		destroyExpression(roadDeclaration->label);
		free(roadDeclaration);
	}
}

void destroyRouteDeclaration(RouteDeclaration * routeDeclaration) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (routeDeclaration != NULL) {
		destroyReference(routeDeclaration->reference);
		destroyPath(routeDeclaration->path);
		destroyExpression(routeDeclaration->label);
		free(routeDeclaration);
	}
}

void destroySimulation(Simulation * simulation) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (simulation != NULL) {
		free(simulation->identifier);
		destroyExpression(simulation->label);
		destroyExpression(simulation->duration);
		free(simulation);
	}
}

void destroyStringLiteral(StringLiteral * stringLiteral) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (stringLiteral != NULL) {
		destroyStringParts(stringLiteral->parts);
		free(stringLiteral);
	}
}

void destroyStringPart(StringPart * stringPart) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (stringPart != NULL) {
		switch (stringPart->type) {
			case FRAGMENT_STRING_PART:
				free(stringPart->fragment);
				break;
			case INTERPOLATION_STRING_PART:
				destroyExpression(stringPart->expression);
				break;
			default:
				logError(_logger, "The specified string part type is unknown: %d", stringPart->type);
				break;
		}
		free(stringPart);
	}
}

void destroyStringParts(StringParts * stringParts) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (stringParts != NULL) {
		StringPart * stringPart = stringParts->first;
		while (stringPart != NULL) {
			StringPart * next = stringPart->next;
			destroyStringPart(stringPart);
			stringPart = next;
		}
		free(stringParts);
	}
}

void destroyWaypoint(Waypoint * waypoint) {
	logDebugging(_logger, "Executing destructor: %s", __FUNCTION__);
	if (waypoint != NULL) {
		destroyReference(waypoint->reference);
		free(waypoint);
	}
}
