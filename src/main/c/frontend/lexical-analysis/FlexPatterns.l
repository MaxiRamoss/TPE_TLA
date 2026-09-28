%top{

#include "../../support/type/CompilationStatus.h"
#include "../../support/type/FlexContext.h"
#include "../../support/type/TokenLabel.h"
#include "../syntactic-analysis/AbstractSyntaxTree.h"
#include "../syntactic-analysis/BisonParser.h"

}

%{

/**
 * @see https://westes.github.io/flex/manual/Definitions-Section.html#Definitions-Section
 */

extern CompilationStatus ArithmeticOperatorLexemeAction(TokenLabel label);
extern CompilationStatus BeginInterpolationLexemeAction(FlexContext context);
extern CompilationStatus BeginStringLexemeAction(FlexContext context);
extern CompilationStatus BooleanLexemeAction(const bool value);
extern CompilationStatus DelimiterLexemeAction(TokenLabel label);
extern CompilationStatus EndInterpolationLexemeAction();
extern CompilationStatus EndStringLexemeAction();
extern CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context);
extern CompilationStatus EOFLexemeAction();
extern CompilationStatus IdentifierLexemeAction();
extern CompilationStatus IgnoredLexemeAction();
extern CompilationStatus IntegerLexemeAction();
extern CompilationStatus InvalidEscapeLexemeAction();
extern CompilationStatus KeywordLexemeAction(TokenLabel label);
extern CompilationStatus LeaveMultilineCommentLexemeAction();
extern CompilationStatus PunctuationLexemeAction(TokenLabel label);
extern CompilationStatus RelationalOperatorLexemeAction(TokenLabel label);
extern CompilationStatus StringFragmentLexemeAction();
extern CompilationStatus UnitLexemeAction(TokenLabel label);
extern CompilationStatus UnknownLexemeAction();
extern CompilationStatus UnterminatedStringLexemeAction();

%}

/**
 * Flex contexts (a.k.a. start conditions). Flex numbers them in order of
 * declaration (INITIAL is 0), and "_toConstructString" and "_toContextString"
 * in "FlexActions.c" depend on that order.
 *
 * @see https://westes.github.io/flex/manual/Start-Conditions.html
 */
%x MULTILINE_COMMENT
%x STRING
%s INTERPOLATION

/**
 * Reusable patterns. A letter is either ASCII or a latin letter with
 * diacritics in the range U+00C0-U+00FF, except U+00D7 (×) and U+00F7 (÷),
 * encoded in UTF-8. A multibyte character is any non-ASCII character encoded
 * in UTF-8, so an unknown character is reported whole, not only its first
 * byte.
 *
 * @see https://westes.github.io/flex/manual/Matching.html
 * @see https://westes.github.io/flex/manual/Patterns.html
 */
digit				[0-9]
letter				[A-Za-z]|\xC3[\x80-\x96\x98-\xB6\xB8-\xBF]
identifier			{letter}({letter}|{digit}|_)*
multibyteCharacter	[\xC2-\xDF][\x80-\xBF]|[\xE0-\xEF][\x80-\xBF]{2}|[\xF0-\xF4][\x80-\xBF]{3}
whitespace			[[:space:]]+

%%

%{

/**
 * @see https://westes.github.io/flex/manual/Rules-Section.html#Rules-Section
 */

%}

"/*"								{ return EnterMultilineCommentLexemeAction(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>"*/"				{ return LeaveMultilineCommentLexemeAction(); }
<MULTILINE_COMMENT>[^*]+			{ return IgnoredLexemeAction(); }
<MULTILINE_COMMENT>.				{ return IgnoredLexemeAction(); }
"//".*								{ return IgnoredLexemeAction(); }

"\""								{ return BeginStringLexemeAction(STRING); }
<STRING>"\""						{ return EndStringLexemeAction(); }
<STRING>"{"							{ return BeginInterpolationLexemeAction(INTERPOLATION); }
<STRING>([^"\\{\n]|\\["\\{])+		{ return StringFragmentLexemeAction(); }
<STRING>\\							{ return InvalidEscapeLexemeAction(); }
<STRING>\n							{ return UnterminatedStringLexemeAction(); }

<INTERPOLATION>"}"					{ return EndInterpolationLexemeAction(); }
<INTERPOLATION>[[:space:]]*\n[[:space:]]*	{ return UnterminatedStringLexemeAction(); }

"simulation"						{ return KeywordLexemeAction(SIMULATION); }
"intersection"						{ return KeywordLexemeAction(INTERSECTION); }
"road"								{ return KeywordLexemeAction(ROAD); }
"light"								{ return KeywordLexemeAction(LIGHT); }
"route"								{ return KeywordLexemeAction(ROUTE); }
"flow"								{ return KeywordLexemeAction(FLOW); }
"policy"							{ return KeywordLexemeAction(POLICY); }
"metric"							{ return KeywordLexemeAction(METRIC); }

"integer"							{ return KeywordLexemeAction(INTEGER_TYPE); }
"boolean"							{ return KeywordLexemeAction(BOOLEAN_TYPE); }
"string"							{ return KeywordLexemeAction(STRING_TYPE); }
"duration"							{ return KeywordLexemeAction(DURATION); }
"distance"							{ return KeywordLexemeAction(DISTANCE); }
"speed"								{ return KeywordLexemeAction(SPEED); }

"at"								{ return KeywordLexemeAction(AT); }
"of"								{ return KeywordLexemeAction(OF); }
"from"								{ return KeywordLexemeAction(FROM); }
"to"								{ return KeywordLexemeAction(TO); }
"along"								{ return KeywordLexemeAction(ALONG); }
"label"								{ return KeywordLexemeAction(LABEL); }
"length"							{ return KeywordLexemeAction(LENGTH); }
"limit"								{ return KeywordLexemeAction(LIMIT); }
"north"								{ return KeywordLexemeAction(NORTH); }
"south"								{ return KeywordLexemeAction(SOUTH); }
"east"								{ return KeywordLexemeAction(EAST); }
"west"								{ return KeywordLexemeAction(WEST); }

"phase"								{ return KeywordLexemeAction(PHASE); }
"green"								{ return KeywordLexemeAction(GREEN); }
"for"								{ return KeywordLexemeAction(FOR); }
"during"							{ return KeywordLexemeAction(DURING); }
"apply"								{ return KeywordLexemeAction(APPLY); }
"if"								{ return KeywordLexemeAction(IF); }
"else"								{ return KeywordLexemeAction(ELSE); }
"extend"							{ return KeywordLexemeAction(EXTEND); }
"by"								{ return KeywordLexemeAction(BY); }
"keep"								{ return KeywordLexemeAction(KEEP); }
"log"								{ return KeywordLexemeAction(LOG); }

"spawn"								{ return KeywordLexemeAction(SPAWN); }
"every"								{ return KeywordLexemeAction(EVERY); }
"on"								{ return KeywordLexemeAction(ON); }

"in"								{ return KeywordLexemeAction(IN); }

"and"								{ return KeywordLexemeAction(AND); }
"or"								{ return KeywordLexemeAction(OR); }
"not"								{ return KeywordLexemeAction(NOT); }
"true"								{ return BooleanLexemeAction(true); }
"false"								{ return BooleanLexemeAction(false); }

"m"									{ return UnitLexemeAction(METERS); }
"km"								{ return UnitLexemeAction(KILOMETERS); }
"s"									{ return UnitLexemeAction(SECONDS); }
"min"								{ return UnitLexemeAction(MINUTES); }
"h"									{ return UnitLexemeAction(HOURS); }
"km/h"								{ return UnitLexemeAction(KILOMETERS_PER_HOUR); }
"m/s"								{ return UnitLexemeAction(METERS_PER_SECOND); }

"+"									{ return ArithmeticOperatorLexemeAction(ADD); }
"-"									{ return ArithmeticOperatorLexemeAction(SUB); }
"*"									{ return ArithmeticOperatorLexemeAction(MUL); }
"/"									{ return ArithmeticOperatorLexemeAction(DIV); }

"="									{ return RelationalOperatorLexemeAction(EQUAL); }
"!="|"≠"							{ return RelationalOperatorLexemeAction(NOT_EQUAL); }
"<"									{ return RelationalOperatorLexemeAction(LESS); }
"<="|"≤"							{ return RelationalOperatorLexemeAction(LESS_EQUAL); }
">"									{ return RelationalOperatorLexemeAction(GREATER); }
">="|"≥"							{ return RelationalOperatorLexemeAction(GREATER_EQUAL); }

"->"								{ return PunctuationLexemeAction(ARROW); }
".."								{ return PunctuationLexemeAction(RANGE); }
"."									{ return PunctuationLexemeAction(DOT); }
","									{ return PunctuationLexemeAction(COMMA); }

"("									{ return DelimiterLexemeAction(OPEN_PARENTHESIS); }
")"									{ return DelimiterLexemeAction(CLOSE_PARENTHESIS); }
"["									{ return DelimiterLexemeAction(OPEN_BRACKET); }
"]"									{ return DelimiterLexemeAction(CLOSE_BRACKET); }
"{"									{ return DelimiterLexemeAction(OPEN_BRACE); }
"}"									{ return DelimiterLexemeAction(CLOSE_BRACE); }

{digit}+							{ return IntegerLexemeAction(); }
{identifier}						{ return IdentifierLexemeAction(); }

{whitespace}						{ return IgnoredLexemeAction(); }
{multibyteCharacter}|.				{ return UnknownLexemeAction(); }

<<EOF>>								{ return EOFLexemeAction(); }

%%

/**
 * @see https://westes.github.io/flex/manual/User-Code-Section.html#User-Code-Section
 */

#include "FlexActions.h"
#include "FlexExport.h"
