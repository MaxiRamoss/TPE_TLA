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
Path * AppendWaypointSemanticAction(Path * path, Reference * reference);
Expression * BinaryExpressionSemanticAction(Expression * leftExpression, Expression * rightExpression, ExpressionType type);
Factor * BooleanFactorSemanticAction(const bool value);
Factor * CallFactorSemanticAction(Call * call);
Call * CallSemanticAction(char * identifier, Arguments * arguments);
Declaration * ConstantDeclarationDeclarationSemanticAction(ConstantDeclaration * constantDeclaration);
ConstantDeclaration * ConstantDeclarationSemanticAction(FundamentalType type, Reference * reference, Expression * expression);
Arguments * EmptyArgumentsSemanticAction();
Declarations * EmptyDeclarationsSemanticAction();
Path * EmptyPathSemanticAction();
StringParts * EmptyStringPartsSemanticAction();
Factor * ExpressionFactorSemanticAction(Expression * expression);
Expression * FactorExpressionSemanticAction(Factor * factor);
Declaration * FlowDeclarationDeclarationSemanticAction(FlowDeclaration * flowDeclaration);
FlowDeclaration * FlowDeclarationSemanticAction(Reference * reference, Reference * route, Expression * label, Expression * amount, Expression * period, Expression * from, Expression * to);
Reference * IdentifierReferenceSemanticAction(char * identifier);
Reference * IndexedReferenceSemanticAction(char * identifier, Expression * index);
Factor * IntegerFactorSemanticAction(const int value);
Declaration * IntersectionDeclarationDeclarationSemanticAction(IntersectionDeclaration * intersectionDeclaration);
IntersectionDeclaration * IntersectionDeclarationSemanticAction(Reference * reference, Position * position, Expression * label);
Factor * MemberAccessFactorSemanticAction(Factor * object, char * member);
Program * ProgramSemanticAction(Simulation * simulation, Declarations * declarations);
Factor * QuantityFactorSemanticAction(const int value, const Unit unit);
Factor * ReferenceFactorSemanticAction(Reference * reference);
Position * RelativePositionSemanticAction(Expression * distance, const Direction direction, Reference * origin);
Declaration * RoadDeclarationDeclarationSemanticAction(RoadDeclaration * roadDeclaration);
RoadDeclaration * RoadDeclarationSemanticAction(Reference * reference, Reference * from, Reference * to, Expression * length, Expression * limit, Expression * label);
Declaration * RouteDeclarationDeclarationSemanticAction(RouteDeclaration * routeDeclaration);
RouteDeclaration * RouteDeclarationSemanticAction(Reference * reference, Path * path, Expression * label);
Simulation * SimulationSemanticAction(char * identifier, Expression * label, Expression * duration);
Factor * StringFactorSemanticAction(StringLiteral * stringLiteral);
StringLiteral * StringLiteralSemanticAction(StringParts * stringParts);
Expression * UnaryExpressionSemanticAction(Expression * operand, ExpressionType type);

#endif
