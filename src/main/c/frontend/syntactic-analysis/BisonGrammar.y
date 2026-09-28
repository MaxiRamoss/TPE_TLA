%{

#include "../../support/logging/Logger.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"
#include <string.h>

/**
 * The error reporting function for Bison parser. It logs the message of Bison,
 * without its "syntax error, " prefix, along with the line of the token that
 * provoked the error ("pushToken" sets the location of each token). The
 * lexical-analyzer pushes an EXCEPTION only to halt the parser (so it releases
 * its stack) after reporting a lexical error, which is not reported again.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {
	const char * prefix = "syntax error, ";
	if (strncmp(message, prefix, strlen(prefix)) == 0) {
		message += strlen(prefix);
	}
	if (strstr(message, "unexpected EXCEPTION") == NULL) {
		Logger * logger = createLogger("SyntacticAnalyzer");
		logError(logger, "Syntax error at line %d: %s", location->first_line, message);
		destroyLogger(logger);
	}
}

%}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	bool boolean;
	signed int integer;
	char * string;
	TokenLabel token;

	/** Non-terminals. */

	Arguments * arguments;
	Call * call;
	ConstantDeclaration * constantDeclaration;
	Declaration * declaration;
	Declarations * declarations;
	Direction direction;
	Expression * expression;
	Factor * factor;
	FlowDeclaration * flowDeclaration;
	FundamentalType fundamentalType;
	IntersectionDeclaration * intersectionDeclaration;
	Path * path;
	Position * position;
	Program * program;
	Reference * reference;
	RoadDeclaration * roadDeclaration;
	RouteDeclaration * routeDeclaration;
	Simulation * simulation;
	StringLiteral * stringLiteral;
	StringParts * stringParts;
	Unit unit;

	/**
	 * The bounds of the window of a flow (both NULL if there is no window).
	 * They are not a node of the tree: the flow declaration stores them.
	 */
	struct {
		Expression * from;
		Expression * to;
	} window;
}

/**
 * Destructors. This functions are executed after the parsing ends, so if the
 * AST must be used in the following phases of the compiler you shouldn't used
 * this approach for the AST root node ("program" non-terminal, in this
 * grammar), or it will drop the entire tree even if the parsing succeeds.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Destructor-Decl.html
 */
%destructor { free($$); } <string>
%destructor { destroyArguments($$); } <arguments>
%destructor { destroyCall($$); } <call>
%destructor { destroyConstantDeclaration($$); } <constantDeclaration>
%destructor { destroyDeclaration($$); } <declaration>
%destructor { destroyDeclarations($$); } <declarations>
%destructor { destroyExpression($$); } <expression>
%destructor { destroyFactor($$); } <factor>
%destructor { destroyFlowDeclaration($$); } <flowDeclaration>
%destructor { destroyIntersectionDeclaration($$); } <intersectionDeclaration>
%destructor { destroyPath($$); } <path>
%destructor { destroyPosition($$); } <position>
%destructor { destroyReference($$); } <reference>
%destructor { destroyRoadDeclaration($$); } <roadDeclaration>
%destructor { destroyRouteDeclaration($$); } <routeDeclaration>
%destructor { destroySimulation($$); } <simulation>
%destructor { destroyStringLiteral($$); } <stringLiteral>
%destructor { destroyStringParts($$); } <stringParts>
%destructor { destroyExpression($$.from); destroyExpression($$.to); } <window>

/** Terminals with a semantic value. */
%token <boolean> BOOLEAN "boolean literal"
%token <integer> INTEGER "integer literal"
%token <string> ID "identifier"
%token <string> STRING_FRAGMENT "string fragment"

/** Keywords: declarations. */
%token <token> SIMULATION "simulation"
%token <token> INTERSECTION "intersection"
%token <token> ROAD "road"
%token <token> LIGHT "light"
%token <token> ROUTE "route"
%token <token> FLOW "flow"
%token <token> POLICY "policy"
%token <token> METRIC "metric"

/** Keywords: fundamental types. */
%token <token> INTEGER_TYPE "integer"
%token <token> BOOLEAN_TYPE "boolean"
%token <token> STRING_TYPE "string"
%token <token> DURATION "duration"
%token <token> DISTANCE "distance"
%token <token> SPEED "speed"

/** Keywords: positions and relations. */
%token <token> AT "at"
%token <token> OF "of"
%token <token> FROM "from"
%token <token> TO "to"
%token <token> ALONG "along"
%token <token> LABEL "label"
%token <token> LENGTH "length"
%token <token> LIMIT "limit"
%token <token> NORTH "north"
%token <token> SOUTH "south"
%token <token> EAST "east"
%token <token> WEST "west"

/** Keywords: lights and rules. */
%token <token> PHASE "phase"
%token <token> GREEN "green"
%token <token> FOR "for"
%token <token> DURING "during"
%token <token> APPLY "apply"
%token <token> IF "if"
%token <token> ELSE "else"
%token <token> EXTEND "extend"
%token <token> BY "by"
%token <token> KEEP "keep"
%token <token> LOG "log"

/** Keywords: flows, metrics and iteration. */
%token <token> SPAWN "spawn"
%token <token> EVERY "every"
%token <token> ON "on"
%token <token> IN "in"

/** Keywords: logical operators. */
%token <token> AND "and"
%token <token> OR "or"
%token <token> NOT "not"

/** Units. */
%token <token> METERS "m"
%token <token> KILOMETERS "km"
%token <token> SECONDS "s"
%token <token> MINUTES "min"
%token <token> HOURS "h"
%token <token> KILOMETERS_PER_HOUR "km/h"
%token <token> METERS_PER_SECOND "m/s"

/** Operators. */
%token <token> ADD "+"
%token <token> SUB "-"
%token <token> MUL "*"
%token <token> DIV "/"
%token <token> EQUAL "="
%token <token> NOT_EQUAL "!="
%token <token> LESS "<"
%token <token> LESS_EQUAL "<="
%token <token> GREATER ">"
%token <token> GREATER_EQUAL ">="

/** Punctuation. */
%token <token> ARROW "->"
%token <token> RANGE ".."
%token <token> DOT "."
%token <token> COMMA ","

/** Delimiters. */
%token <token> OPEN_PARENTHESIS "("
%token <token> CLOSE_PARENTHESIS ")"
%token <token> OPEN_BRACE "{"
%token <token> CLOSE_BRACE "}"
%token <token> OPEN_BRACKET "["
%token <token> CLOSE_BRACKET "]"

/** Strings. */
%token <token> STRING_BEGIN "string begin"
%token <token> STRING_END "string end"
%token <token> INTERPOLATION_BEGIN "interpolation begin"
%token <token> INTERPOLATION_END "interpolation end"

/** Comments (only for logging purposes, the parser never receives them). */
%token <token> OPEN_COMMENT "/*"
%token <token> CLOSE_COMMENT "*/"

%token <token> EXCEPTION
%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <arguments> arguments argumentsOpt
%type <call> call
%type <constantDeclaration> constantDeclaration
%type <declaration> declaration
%type <declarations> declarations
%type <direction> direction
%type <expression> expression labelOpt lengthOpt limitOpt
%type <factor> factor
%type <flowDeclaration> flowDeclaration
%type <fundamentalType> fundamentalType
%type <intersectionDeclaration> intersectionDeclaration
%type <path> path
%type <position> position
%type <program> program
%type <reference> reference
%type <roadDeclaration> roadDeclaration
%type <routeDeclaration> routeDeclaration
%type <simulation> simulation
%type <stringLiteral> string
%type <stringParts> stringParts
%type <unit> unit
%type <window> windowOpt

/**
 * Precedence and associativity.
 *
 * @see https://en.cppreference.com/w/cpp/language/operator_precedence.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Precedence.html
 */
%left OR
%left AND
%right NOT
%nonassoc EQUAL NOT_EQUAL
%nonassoc LESS LESS_EQUAL GREATER GREATER_EQUAL
%left ADD SUB
%left MUL DIV
%right UNARY_MINUS

%%

// IMPORTANT: To use λ in the following grammar, use the %empty symbol.

program: simulation declarations								{ $$ = ProgramSemanticAction($1, $2); }
	;

simulation: SIMULATION ID labelOpt OPEN_BRACE DURATION expression CLOSE_BRACE	{ $$ = SimulationSemanticAction($2, $3, $6); }
	;

declarations: %empty											{ $$ = EmptyDeclarationsSemanticAction(); }
	| declarations declaration									{ $$ = AppendDeclarationSemanticAction($1, $2); }
	;

declaration: constantDeclaration								{ $$ = ConstantDeclarationDeclarationSemanticAction($1); }
	| intersectionDeclaration									{ $$ = IntersectionDeclarationDeclarationSemanticAction($1); }
	| roadDeclaration											{ $$ = RoadDeclarationDeclarationSemanticAction($1); }
	| routeDeclaration											{ $$ = RouteDeclarationDeclarationSemanticAction($1); }
	| flowDeclaration											{ $$ = FlowDeclarationDeclarationSemanticAction($1); }
	;

reference: ID													{ $$ = IdentifierReferenceSemanticAction($1); }
	| ID OPEN_BRACKET expression CLOSE_BRACKET					{ $$ = IndexedReferenceSemanticAction($1, $3); }
	;

labelOpt: %empty												{ $$ = NULL; }
	| LABEL expression											{ $$ = $2; }
	;

constantDeclaration: fundamentalType reference EQUAL expression	{ $$ = ConstantDeclarationSemanticAction($1, $2, $4); }
	;

fundamentalType: INTEGER_TYPE									{ $$ = INTEGER_FUNDAMENTAL_TYPE; }
	| BOOLEAN_TYPE												{ $$ = BOOLEAN_FUNDAMENTAL_TYPE; }
	| STRING_TYPE												{ $$ = STRING_FUNDAMENTAL_TYPE; }
	| DURATION													{ $$ = DURATION_FUNDAMENTAL_TYPE; }
	| DISTANCE													{ $$ = DISTANCE_FUNDAMENTAL_TYPE; }
	| SPEED														{ $$ = SPEED_FUNDAMENTAL_TYPE; }
	;

intersectionDeclaration: INTERSECTION reference position labelOpt	{ $$ = IntersectionDeclarationSemanticAction($2, $3, $4); }
	;

position: AT OPEN_PARENTHESIS expression COMMA expression CLOSE_PARENTHESIS	{ $$ = AbsolutePositionSemanticAction($3, $5); }
	| AT expression direction OF reference						{ $$ = RelativePositionSemanticAction($2, $3, $5); }
	;

direction: NORTH												{ $$ = NORTH_DIRECTION; }
	| SOUTH														{ $$ = SOUTH_DIRECTION; }
	| EAST														{ $$ = EAST_DIRECTION; }
	| WEST														{ $$ = WEST_DIRECTION; }
	;

roadDeclaration: ROAD reference FROM reference TO reference lengthOpt limitOpt labelOpt	{ $$ = RoadDeclarationSemanticAction($2, $4, $6, $7, $8, $9); }
	;

lengthOpt: %empty												{ $$ = NULL; }
	| LENGTH expression											{ $$ = $2; }
	;

limitOpt: %empty												{ $$ = NULL; }
	| LIMIT expression											{ $$ = $2; }
	;

routeDeclaration: ROUTE reference EQUAL path labelOpt			{ $$ = RouteDeclarationSemanticAction($2, $4, $5); }
	;

path: reference ARROW reference									{ $$ = AppendWaypointSemanticAction(AppendWaypointSemanticAction(EmptyPathSemanticAction(), $1), $3); }
	| path ARROW reference										{ $$ = AppendWaypointSemanticAction($1, $3); }
	;

flowDeclaration: FLOW reference ALONG reference labelOpt OPEN_BRACE SPAWN expression EVERY expression windowOpt CLOSE_BRACE	{ $$ = FlowDeclarationSemanticAction($2, $4, $5, $8, $10, $11.from, $11.to); }
	;

windowOpt: %empty												{ $$.from = NULL; $$.to = NULL; }
	| FROM expression TO expression								{ $$.from = $2; $$.to = $4; }
	;

expression: expression[left] OR expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, DISJUNCTION); }
	| expression[left] AND expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, CONJUNCTION); }
	| NOT expression											{ $$ = UnaryExpressionSemanticAction($2, LOGICAL_NEGATION); }
	| expression[left] EQUAL expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, EQUALITY); }
	| expression[left] NOT_EQUAL expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, INEQUALITY); }
	| expression[left] LESS expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_THAN); }
	| expression[left] LESS_EQUAL expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, LESS_THAN_OR_EQUAL_TO); }
	| expression[left] GREATER expression[right]				{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_THAN); }
	| expression[left] GREATER_EQUAL expression[right]			{ $$ = BinaryExpressionSemanticAction($left, $right, GREATER_THAN_OR_EQUAL_TO); }
	| expression[left] ADD expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, ADDITION); }
	| expression[left] SUB expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, SUBTRACTION); }
	| expression[left] MUL expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, MULTIPLICATION); }
	| expression[left] DIV expression[right]					{ $$ = BinaryExpressionSemanticAction($left, $right, DIVISION); }
	| SUB expression %prec UNARY_MINUS							{ $$ = UnaryExpressionSemanticAction($2, ARITHMETIC_NEGATION); }
	| factor													{ $$ = FactorExpressionSemanticAction($1); }
	;

factor: OPEN_PARENTHESIS expression CLOSE_PARENTHESIS			{ $$ = ExpressionFactorSemanticAction($2); }
	| INTEGER													{ $$ = IntegerFactorSemanticAction($1); }
	| INTEGER unit												{ $$ = QuantityFactorSemanticAction($1, $2); }
	| BOOLEAN													{ $$ = BooleanFactorSemanticAction($1); }
	| string													{ $$ = StringFactorSemanticAction($1); }
	| reference													{ $$ = ReferenceFactorSemanticAction($1); }
	| call														{ $$ = CallFactorSemanticAction($1); }
	| factor DOT ID												{ $$ = MemberAccessFactorSemanticAction($1, $3); }
	;

call: ID OPEN_PARENTHESIS argumentsOpt CLOSE_PARENTHESIS		{ $$ = CallSemanticAction($1, $3); }
	;

argumentsOpt: %empty											{ $$ = EmptyArgumentsSemanticAction(); }
	| arguments													{ $$ = $1; }
	;

arguments: expression											{ $$ = AppendArgumentSemanticAction(EmptyArgumentsSemanticAction(), $1); }
	| arguments COMMA expression								{ $$ = AppendArgumentSemanticAction($1, $3); }
	;

unit: METERS													{ $$ = METERS_UNIT; }
	| KILOMETERS												{ $$ = KILOMETERS_UNIT; }
	| SECONDS													{ $$ = SECONDS_UNIT; }
	| MINUTES													{ $$ = MINUTES_UNIT; }
	| HOURS														{ $$ = HOURS_UNIT; }
	| KILOMETERS_PER_HOUR										{ $$ = KILOMETERS_PER_HOUR_UNIT; }
	| METERS_PER_SECOND											{ $$ = METERS_PER_SECOND_UNIT; }
	;

string: STRING_BEGIN stringParts STRING_END						{ $$ = StringLiteralSemanticAction($2); }
	;

stringParts: %empty												{ $$ = EmptyStringPartsSemanticAction(); }
	| stringParts STRING_FRAGMENT								{ $$ = AppendFragmentSemanticAction($1, $2); }
	| stringParts INTERPOLATION_BEGIN expression INTERPOLATION_END	{ $$ = AppendInterpolationSemanticAction($1, $3); }
	;

%%
