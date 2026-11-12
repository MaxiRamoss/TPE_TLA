#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule();

/**
 * Bison semantic actions.
 */

Position * AbsolutePositionSemanticAction(Expression * x, Expression * y);
Arguments * AppendArgumentSemanticAction(Arguments * arguments, Expression * expression);
Declarations * AppendDeclarationSemanticAction(Declarations * declarations, Declaration * declaration);
StringParts * AppendFragmentSemanticAction(StringParts * stringParts, char * fragment);
StringParts * AppendInterpolationSemanticAction(StringParts * stringParts, Expression * expression);
LightItems * AppendLightItemSemanticAction(LightItems * lightItems, LightItem * lightItem);
Parameters * AppendParameterSemanticAction(Parameters * parameters, Parameter * parameter);
References * AppendReferenceSemanticAction(References * references, Reference * reference);
Block * AppendStatementSemanticAction(Block * block, Statement * statement);
Path * AppendWaypointSemanticAction(Path * path, Reference * reference);
LightItem * ApplyLightItemSemanticAction(Call * call, const int line);
Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type, const int line);
Factor * BooleanFactorSemanticAction(const bool value, const int line);
Factor * CallFactorSemanticAction(Call * call, const int line);
Call * CallSemanticAction(char * identifier, Arguments * arguments, const int line);
Declaration * ConstantDeclarationDeclarationSemanticAction(ConstantDeclaration * constantDeclaration, const int line);
ConstantDeclaration * ConstantDeclarationSemanticAction(FundamentalType type, Reference * reference, Expression * expression);
Arguments * EmptyArgumentsSemanticAction();
Block * EmptyBlockSemanticAction();
Declarations * EmptyDeclarationsSemanticAction();
LightItems * EmptyLightItemsSemanticAction();
Parameters * EmptyParametersSemanticAction();
Path * EmptyPathSemanticAction();
References * EmptyReferencesSemanticAction();
StringParts * EmptyStringPartsSemanticAction();
Factor * ExpressionFactorSemanticAction(Expression * expression, const int line);
Statement * ExtendStatementSemanticAction(Reference * road, Expression * duration, const int line);
Expression * FactorExpressionSemanticAction(Factor * factor, const int line);
Declaration * FlowDeclarationDeclarationSemanticAction(FlowDeclaration * flowDeclaration, const int line);
FlowDeclaration * FlowDeclarationSemanticAction(Reference * reference, Reference * route, Expression * label, Expression * amount, Expression * period, Expression * from, Expression * to);
Declaration * ForDeclarationDeclarationSemanticAction(ForDeclaration * forDeclaration, const int line);
ForDeclaration * ForDeclarationSemanticAction(char * variable, Expression * from, Expression * to, Declarations * declarations);
ParameterType FundamentalParameterTypeSemanticAction(const FundamentalType fundamentalType);
Reference * IdentifierReferenceSemanticAction(char * identifier, const int line);
IfStatement * IfElseIfStatementSemanticAction(Expression * condition, Block * thenBlock, IfStatement * elseIfStatement);
IfStatement * IfElseStatementSemanticAction(Expression * condition, Block * thenBlock, Block * elseBlock);
IfStatement * IfStatementSemanticAction(Expression * condition, Block * thenBlock);
Statement * IfStatementStatementSemanticAction(IfStatement * ifStatement, const int line);
Reference * IndexedReferenceSemanticAction(char * identifier, Expression * index, const int line);
Factor * IntegerFactorSemanticAction(const int value, const int line);
Declaration * IntersectionDeclarationDeclarationSemanticAction(IntersectionDeclaration * intersectionDeclaration, const int line);
IntersectionDeclaration * IntersectionDeclarationSemanticAction(Reference * reference, Position * position, Expression * label);
Statement * KeepStatementSemanticAction(const int line);
Declaration * LightDeclarationDeclarationSemanticAction(LightDeclaration * lightDeclaration, const int line);
LightDeclaration * LightDeclarationSemanticAction(Reference * reference, Reference * intersection, Expression * label, LightItems * items);
Statement * LogStatementSemanticAction(StringLiteral * message, const int line);
Factor * MemberAccessFactorSemanticAction(Factor * object, char * member, const int line);
Declaration * MetricDeclarationDeclarationSemanticAction(MetricDeclaration * metricDeclaration, const int line);
MetricDeclaration * MetricDeclarationSemanticAction(Reference * reference, Call * aggregator, char * event, Expression * label);
Parameter * ParameterSemanticAction(const ParameterType type, char * identifier);
LightItem * PhaseLightItemSemanticAction(References * roads, Expression * duration, const int line);
Declaration * PolicyDeclarationDeclarationSemanticAction(PolicyDeclaration * policyDeclaration, const int line);
PolicyDeclaration * PolicyDeclarationSemanticAction(char * identifier, Parameters * parameters, Block * block);
Program * ProgramSemanticAction(Simulation * simulation, Declarations * declarations);
Factor * QuantityFactorSemanticAction(const int value, const Unit unit, const int line);
Factor * ReferenceFactorSemanticAction(Reference * reference, const int line);
Position * RelativePositionSemanticAction(Expression * distance, const Direction direction, Reference * origin);
Declaration * RoadDeclarationDeclarationSemanticAction(RoadDeclaration * roadDeclaration, const int line);
RoadDeclaration * RoadDeclarationSemanticAction(Reference * reference, Reference * from, Reference * to, Expression * length, Expression * limit, Expression * label);
Declaration * RouteDeclarationDeclarationSemanticAction(RouteDeclaration * routeDeclaration, const int line);
RouteDeclaration * RouteDeclarationSemanticAction(Reference * reference, Path * path, Expression * label);
Simulation * SimulationSemanticAction(char * identifier, Expression * label, Expression * duration);
Factor * StringFactorSemanticAction(StringLiteral * stringLiteral, const int line);
StringLiteral * StringLiteralSemanticAction(StringParts * stringParts);
Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type, const int line);

#endif
