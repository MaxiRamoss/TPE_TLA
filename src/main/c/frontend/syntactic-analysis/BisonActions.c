#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* IMPORTED FUNCTIONS */

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);

/**
 * The parameter type of each fundamental type.
 */
static const ParameterType _fundamentalParameterTypes[] = {
	[BOOLEAN_FUNDAMENTAL_TYPE] = BOOLEAN_PARAMETER_TYPE,
	[DISTANCE_FUNDAMENTAL_TYPE] = DISTANCE_PARAMETER_TYPE,
	[DURATION_FUNDAMENTAL_TYPE] = DURATION_PARAMETER_TYPE,
	[INTEGER_FUNDAMENTAL_TYPE] = INTEGER_PARAMETER_TYPE,
	[SPEED_FUNDAMENTAL_TYPE] = SPEED_PARAMETER_TYPE,
	[STRING_FUNDAMENTAL_TYPE] = STRING_PARAMETER_TYPE
};

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

/* PUBLIC FUNCTIONS */

Position * AbsolutePositionSemanticAction(Expression * x, Expression * y) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Position * position = calloc(1, sizeof(Position));
	position->x = x;
	position->y = y;
	position->type = ABSOLUTE_POSITION;
	return position;
}

Arguments * AppendArgumentSemanticAction(Arguments * arguments, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Argument * argument = calloc(1, sizeof(Argument));
	argument->expression = expression;
	if (arguments->last == NULL) {
		arguments->first = argument;
	}
	else {
		arguments->last->next = argument;
	}
	arguments->last = argument;
	return arguments;
}

Declarations * AppendDeclarationSemanticAction(Declarations * declarations, Declaration * declaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (declarations->last == NULL) {
		declarations->first = declaration;
	}
	else {
		declarations->last->next = declaration;
	}
	declarations->last = declaration;
	return declarations;
}

StringParts * AppendFragmentSemanticAction(StringParts * stringParts, char * fragment) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StringPart * stringPart = calloc(1, sizeof(StringPart));
	stringPart->fragment = fragment;
	stringPart->type = FRAGMENT_STRING_PART;
	if (stringParts->last == NULL) {
		stringParts->first = stringPart;
	}
	else {
		stringParts->last->next = stringPart;
	}
	stringParts->last = stringPart;
	return stringParts;
}

StringParts * AppendInterpolationSemanticAction(StringParts * stringParts, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StringPart * stringPart = calloc(1, sizeof(StringPart));
	stringPart->expression = expression;
	stringPart->type = INTERPOLATION_STRING_PART;
	if (stringParts->last == NULL) {
		stringParts->first = stringPart;
	}
	else {
		stringParts->last->next = stringPart;
	}
	stringParts->last = stringPart;
	return stringParts;
}

LightItems * AppendLightItemSemanticAction(LightItems * lightItems, LightItem * lightItem) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (lightItems->last == NULL) {
		lightItems->first = lightItem;
	}
	else {
		lightItems->last->next = lightItem;
	}
	lightItems->last = lightItem;
	return lightItems;
}

Parameters * AppendParameterSemanticAction(Parameters * parameters, Parameter * parameter) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (parameters->last == NULL) {
		parameters->first = parameter;
	}
	else {
		parameters->last->next = parameter;
	}
	parameters->last = parameter;
	return parameters;
}

References * AppendReferenceSemanticAction(References * references, Reference * reference) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ReferenceItem * referenceItem = calloc(1, sizeof(ReferenceItem));
	referenceItem->reference = reference;
	if (references->last == NULL) {
		references->first = referenceItem;
	}
	else {
		references->last->next = referenceItem;
	}
	references->last = referenceItem;
	return references;
}

Block * AppendStatementSemanticAction(Block * block, Statement * statement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (block->last == NULL) {
		block->first = statement;
	}
	else {
		block->last->next = statement;
	}
	block->last = statement;
	return block;
}

Path * AppendWaypointSemanticAction(Path * path, Reference * reference) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Waypoint * waypoint = calloc(1, sizeof(Waypoint));
	waypoint->reference = reference;
	if (path->last == NULL) {
		path->first = waypoint;
	}
	else {
		path->last->next = waypoint;
	}
	path->last = waypoint;
	return path;
}

LightItem * ApplyLightItemSemanticAction(Call * call) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LightItem * lightItem = calloc(1, sizeof(LightItem));
	lightItem->call = call;
	lightItem->type = APPLY_LIGHT_ITEM;
	return lightItem;
}

Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->leftExpression = leftExpression;
	expression->rightExpression = rightExpression;
	expression->type = type;
	return expression;
}

Factor * BooleanFactorSemanticAction(const bool value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->boolean = value;
	factor->type = BOOLEAN_FACTOR;
	return factor;
}

Factor * CallFactorSemanticAction(Call * call) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->call = call;
	factor->type = CALL_FACTOR;
	return factor;
}

Call * CallSemanticAction(char * identifier, Arguments * arguments) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Call * call = calloc(1, sizeof(Call));
	call->identifier = identifier;
	call->arguments = arguments;
	return call;
}

Declaration * ConstantDeclarationDeclarationSemanticAction(ConstantDeclaration * constantDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->constantDeclaration = constantDeclaration;
	declaration->type = CONSTANT_DECLARATION;
	return declaration;
}

ConstantDeclaration * ConstantDeclarationSemanticAction(FundamentalType type, Reference * reference, Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ConstantDeclaration * constantDeclaration = calloc(1, sizeof(ConstantDeclaration));
	constantDeclaration->type = type;
	constantDeclaration->reference = reference;
	constantDeclaration->expression = expression;
	return constantDeclaration;
}

Arguments * EmptyArgumentsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Arguments));
}

Block * EmptyBlockSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Block));
}

Declarations * EmptyDeclarationsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Declarations));
}

LightItems * EmptyLightItemsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(LightItems));
}

Parameters * EmptyParametersSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Parameters));
}

Path * EmptyPathSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Path));
}

References * EmptyReferencesSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(References));
}

StringParts * EmptyStringPartsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(StringParts));
}

Factor * ExpressionFactorSemanticAction(Expression * expression) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->expression = expression;
	factor->type = EXPRESSION_FACTOR;
	return factor;
}

Statement * ExtendStatementSemanticAction(Reference * road, Expression * duration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->road = road;
	statement->duration = duration;
	statement->type = EXTEND_STATEMENT;
	return statement;
}

Expression * FactorExpressionSemanticAction(Factor * factor) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->factor = factor;
	expression->type = FACTOR;
	return expression;
}

Declaration * FlowDeclarationDeclarationSemanticAction(FlowDeclaration * flowDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->flowDeclaration = flowDeclaration;
	declaration->type = FLOW_DECLARATION;
	return declaration;
}

FlowDeclaration * FlowDeclarationSemanticAction(Reference * reference, Reference * route, Expression * label, Expression * amount, Expression * period, Expression * from, Expression * to) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	FlowDeclaration * flowDeclaration = calloc(1, sizeof(FlowDeclaration));
	flowDeclaration->reference = reference;
	flowDeclaration->route = route;
	flowDeclaration->label = label;
	flowDeclaration->amount = amount;
	flowDeclaration->period = period;
	flowDeclaration->from = from;
	flowDeclaration->to = to;
	return flowDeclaration;
}

ParameterType FundamentalParameterTypeSemanticAction(const FundamentalType fundamentalType) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return _fundamentalParameterTypes[fundamentalType];
}

Reference * IdentifierReferenceSemanticAction(char * identifier) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Reference * reference = calloc(1, sizeof(Reference));
	reference->identifier = identifier;
	return reference;
}

IfStatement * IfElseIfStatementSemanticAction(Expression * condition, Block * thenBlock, IfStatement * elseIfStatement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IfStatement * ifStatement = calloc(1, sizeof(IfStatement));
	ifStatement->condition = condition;
	ifStatement->thenBlock = thenBlock;
	ifStatement->elseIfStatement = elseIfStatement;
	ifStatement->elseBranchType = IF_ELSE_BRANCH;
	return ifStatement;
}

IfStatement * IfElseStatementSemanticAction(Expression * condition, Block * thenBlock, Block * elseBlock) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IfStatement * ifStatement = calloc(1, sizeof(IfStatement));
	ifStatement->condition = condition;
	ifStatement->thenBlock = thenBlock;
	ifStatement->elseBlock = elseBlock;
	ifStatement->elseBranchType = BLOCK_ELSE_BRANCH;
	return ifStatement;
}

IfStatement * IfStatementSemanticAction(Expression * condition, Block * thenBlock) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IfStatement * ifStatement = calloc(1, sizeof(IfStatement));
	ifStatement->condition = condition;
	ifStatement->thenBlock = thenBlock;
	ifStatement->elseBranchType = NO_ELSE_BRANCH;
	return ifStatement;
}

Statement * IfStatementStatementSemanticAction(IfStatement * ifStatement) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->ifStatement = ifStatement;
	statement->type = IF_STATEMENT;
	return statement;
}

Reference * IndexedReferenceSemanticAction(char * identifier, Expression * index) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Reference * reference = calloc(1, sizeof(Reference));
	reference->identifier = identifier;
	reference->index = index;
	return reference;
}

Factor * IntegerFactorSemanticAction(const int value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->integer = value;
	factor->type = INTEGER_FACTOR;
	return factor;
}

Declaration * IntersectionDeclarationDeclarationSemanticAction(IntersectionDeclaration * intersectionDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->intersectionDeclaration = intersectionDeclaration;
	declaration->type = INTERSECTION_DECLARATION;
	return declaration;
}

IntersectionDeclaration * IntersectionDeclarationSemanticAction(Reference * reference, Position * position, Expression * label) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	IntersectionDeclaration * intersectionDeclaration = calloc(1, sizeof(IntersectionDeclaration));
	intersectionDeclaration->reference = reference;
	intersectionDeclaration->position = position;
	intersectionDeclaration->label = label;
	return intersectionDeclaration;
}

Statement * KeepStatementSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->type = KEEP_STATEMENT;
	return statement;
}

Declaration * LightDeclarationDeclarationSemanticAction(LightDeclaration * lightDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->lightDeclaration = lightDeclaration;
	declaration->type = LIGHT_DECLARATION;
	return declaration;
}

LightDeclaration * LightDeclarationSemanticAction(Reference * reference, Reference * intersection, Expression * label, LightItems * items) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LightDeclaration * lightDeclaration = calloc(1, sizeof(LightDeclaration));
	lightDeclaration->reference = reference;
	lightDeclaration->intersection = intersection;
	lightDeclaration->label = label;
	lightDeclaration->items = items;
	return lightDeclaration;
}

Statement * LogStatementSemanticAction(StringLiteral * message) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Statement * statement = calloc(1, sizeof(Statement));
	statement->message = message;
	statement->type = LOG_STATEMENT;
	return statement;
}

Factor * MemberAccessFactorSemanticAction(Factor * object, char * member) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->factor = object;
	factor->member = member;
	factor->type = MEMBER_ACCESS_FACTOR;
	return factor;
}

Parameter * ParameterSemanticAction(const ParameterType type, char * identifier) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Parameter * parameter = calloc(1, sizeof(Parameter));
	parameter->type = type;
	parameter->identifier = identifier;
	return parameter;
}

LightItem * PhaseLightItemSemanticAction(References * roads, Expression * duration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	LightItem * lightItem = calloc(1, sizeof(LightItem));
	lightItem->roads = roads;
	lightItem->duration = duration;
	lightItem->type = PHASE_LIGHT_ITEM;
	return lightItem;
}

Declaration * PolicyDeclarationDeclarationSemanticAction(PolicyDeclaration * policyDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->policyDeclaration = policyDeclaration;
	declaration->type = POLICY_DECLARATION;
	return declaration;
}

PolicyDeclaration * PolicyDeclarationSemanticAction(char * identifier, Parameters * parameters, Block * block) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	PolicyDeclaration * policyDeclaration = calloc(1, sizeof(PolicyDeclaration));
	policyDeclaration->identifier = identifier;
	policyDeclaration->parameters = parameters;
	policyDeclaration->block = block;
	return policyDeclaration;
}

Program * ProgramSemanticAction(Simulation * simulation, Declarations * declarations) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->simulation = simulation;
	program->declarations = declarations;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

Factor * QuantityFactorSemanticAction(const int value, const Unit unit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Quantity * quantity = calloc(1, sizeof(Quantity));
	quantity->value = value;
	quantity->unit = unit;
	Factor * factor = calloc(1, sizeof(Factor));
	factor->quantity = quantity;
	factor->type = QUANTITY_FACTOR;
	return factor;
}

Factor * ReferenceFactorSemanticAction(Reference * reference) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->reference = reference;
	factor->type = REFERENCE_FACTOR;
	return factor;
}

Position * RelativePositionSemanticAction(Expression * distance, const Direction direction, Reference * origin) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Position * position = calloc(1, sizeof(Position));
	position->distance = distance;
	position->direction = direction;
	position->origin = origin;
	position->type = RELATIVE_POSITION;
	return position;
}

Declaration * RoadDeclarationDeclarationSemanticAction(RoadDeclaration * roadDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->roadDeclaration = roadDeclaration;
	declaration->type = ROAD_DECLARATION;
	return declaration;
}

RoadDeclaration * RoadDeclarationSemanticAction(Reference * reference, Reference * from, Reference * to, Expression * length, Expression * limit, Expression * label) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	RoadDeclaration * roadDeclaration = calloc(1, sizeof(RoadDeclaration));
	roadDeclaration->reference = reference;
	roadDeclaration->from = from;
	roadDeclaration->to = to;
	roadDeclaration->length = length;
	roadDeclaration->limit = limit;
	roadDeclaration->label = label;
	return roadDeclaration;
}

Declaration * RouteDeclarationDeclarationSemanticAction(RouteDeclaration * routeDeclaration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Declaration * declaration = calloc(1, sizeof(Declaration));
	declaration->routeDeclaration = routeDeclaration;
	declaration->type = ROUTE_DECLARATION;
	return declaration;
}

RouteDeclaration * RouteDeclarationSemanticAction(Reference * reference, Path * path, Expression * label) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	RouteDeclaration * routeDeclaration = calloc(1, sizeof(RouteDeclaration));
	routeDeclaration->reference = reference;
	routeDeclaration->path = path;
	routeDeclaration->label = label;
	return routeDeclaration;
}

Simulation * SimulationSemanticAction(char * identifier, Expression * label, Expression * duration) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Simulation * simulation = calloc(1, sizeof(Simulation));
	simulation->identifier = identifier;
	simulation->label = label;
	simulation->duration = duration;
	return simulation;
}

Factor * StringFactorSemanticAction(StringLiteral * stringLiteral) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Factor * factor = calloc(1, sizeof(Factor));
	factor->stringLiteral = stringLiteral;
	factor->type = STRING_FACTOR;
	return factor;
}

StringLiteral * StringLiteralSemanticAction(StringParts * stringParts) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	StringLiteral * stringLiteral = calloc(1, sizeof(StringLiteral));
	stringLiteral->parts = stringParts;
	return stringLiteral;
}

Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Expression * expression = calloc(1, sizeof(Expression));
	expression->expression = operand;
	expression->type = type;
	return expression;
}
