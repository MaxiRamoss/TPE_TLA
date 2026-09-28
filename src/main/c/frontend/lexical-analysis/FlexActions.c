#include "FlexActions.h"

/* MODULE INTERNAL STATE */

/**
 * The line where each open Flex context begins, from the outermost to the
 * innermost, to report an unclosed context at the end of the input.
 */
static unsigned int * _contextLines = NULL;
static unsigned int _contextLinesLength = 0;
static bool _exceptionThrown = false;
static bool _logIgnoredLexemes = true;
static LexicalAnalyzer * _lexicalAnalyzer = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownFlexActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: FlexActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	free(_contextLines);
	_contextLines = NULL;
	_contextLinesLength = 0;
	_lexicalAnalyzer = NULL;
}

ModuleDestructor initializeFlexActionsModule(LexicalAnalyzer * lexicalAnalyzer) {
	_lexicalAnalyzer = lexicalAnalyzer;
	_logger = createLogger("FlexActions");
	_logIgnoredLexemes = getBooleanOrDefault("LOG_IGNORED_LEXEMES", _logIgnoredLexemes);
	return _shutdownFlexActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _enterContext(const FlexContext context);
static bool _isInvisibleCodePoint(const unsigned int codePoint);
static void _leaveContext();
static void _logTokenAction(const char * actionName, Token * token);
static CompilationStatus _pushTokenAction(const char * actionName, Token * token);
static CompilationStatus _throw();
static CompilationStatus _throwLexicalError(const char * actionName, const char * message);
static CompilationStatus _throwLexicalErrorAtLine(const unsigned int line, const char * message);
static char * _toCharacterString(const Token * token);
static unsigned int _toCodePoint(const Token * token);
static const char * _toConstructString(const FlexContext context);
static const char * _toContextString(const FlexContext context);
static char * _unescapeStringFragment(const char * lexeme);

/* PUBLIC FUNCTIONS THAT OTHER ACTIONS REUSE */

CompilationStatus IgnoredLexemeAction();
CompilationStatus UnknownLexemeAction();

/**
 * Enters a Flex context, and records the line where it begins.
 */
static void _enterContext(const FlexContext context) {
	_contextLines = realloc(_contextLines, (1 + _contextLinesLength) * sizeof(unsigned int));
	_contextLines[_contextLinesLength++] = currentLexicalAnalyzerLine(_lexicalAnalyzer);
	enterLexicalAnalyzerContext(_lexicalAnalyzer, context);
}

/**
 * Leaves the current Flex context, and discards the line where it begins.
 */
static void _leaveContext() {
	--_contextLinesLength;
	leaveLexicalAnalyzerContext(_lexicalAnalyzer);
}

/**
 * Get the context string of the specified Flex context. The identifiers
 * follow the order of declaration in "FlexPatterns.l".
 */
static const char * _toContextString(const FlexContext context) {
	switch (context) {
		case 0: return "INITIAL";
		case 1: return "MULTILINE_COMMENT";
		case 2: return "STRING";
		case 3: return "INTERPOLATION";
		default:
			logError(_logger, "The specified Flex context is unknown: %d", context);
			return "<UNKNOWN CONTEXT>";
	}
}

/**
 * Get the name of the construct that the specified Flex context scans, to
 * report it in error messages. The identifiers follow the order of
 * declaration in "FlexPatterns.l".
 */
static const char * _toConstructString(const FlexContext context) {
	switch (context) {
		case 1: return "comment";
		case 2: return "string";
		case 3: return "interpolation";
		default:
			logError(_logger, "The specified Flex context has no construct: %d", context);
			return "<UNKNOWN CONSTRUCT>";
	}
}

/**
 * Logs a lexical-analyzer action over a token in DEBUGGING level.
 */
static void _logTokenAction(const char * actionName, Token * token) {
	char * _lexeme = escape(token->lexeme);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": Token(context=%s, label=%d, length=%d, lexeme=%s\"%s\"%s, line=%d, semanticValue=%p)",
		actionName,
		_toContextString(token->context),
		token->label,
		token->length,
		INFORMATION_COLOR, _lexeme, DEFAULT_COLOR,
		token->line,
		token->semanticValue);
	free(_lexeme);
	_lexeme = NULL;
}

/**
 * Logs, pushes and destroys a token, and returns the status of the parser.
 */
static CompilationStatus _pushTokenAction(const char * actionName, Token * token) {
	_logTokenAction(actionName, token);
	CompilationStatus status = pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return status;
}

/**
 * Instructs the parser to halt execution unrecoverably, after a lexical error
 * was reported. The parser rejects the EXCEPTION token as a syntax error, but
 * it doesn't report it, because "isExceptionThrown" is already true.
 */
CompilationStatus _throw() {
	logDebugging(_logger, "An exception is thrown.");
	_exceptionThrown = true;
	Token * token = createToken(_lexicalAnalyzer, EXCEPTION);
	pushToken(_lexicalAnalyzer, token);
	destroyToken(token);
	return FAILED;
}

/**
 * Reports a lexical error over the current lexeme, and halts the parser. Flex
 * counts the line-breaks of the lexeme before running the action, so they
 * are discounted to report the line where the lexeme begins.
 */
static CompilationStatus _throwLexicalError(const char * actionName, const char * message) {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	_logTokenAction(actionName, token);
	unsigned int line = token->line;
	for (unsigned int k = 0; k < token->length; ++k) {
		if (token->lexeme[k] == '\n') {
			--line;
		}
	}
	destroyToken(token);
	return _throwLexicalErrorAtLine(line, message);
}

/**
 * Reports a lexical error at the specified line, and halts the parser.
 */
static CompilationStatus _throwLexicalErrorAtLine(const unsigned int line, const char * message) {
	logError(_logger, "Lexical error at line %d: %s", line, message);
	return _throw();
}

/**
 * Determines if a code point is invisible or not printable on its own: a
 * control, a white-space, a combining diacritical mark, a default-ignorable
 * code point (e.g., the zero-width space or the byte order mark), a
 * private-use character or a non-character. The ranges are inclusive, and
 * sorted.
 *
 * @see https://www.unicode.org/reports/tr44/#Default_Ignorable_Code_Point
 * @see https://www.unicode.org/reports/tr44/#White_Space
 */
static bool _isInvisibleCodePoint(const unsigned int codePoint) {
	static const unsigned int ranges[][2] = {
		{ 0x0000, 0x0020 },		// C0 controls and space.
		{ 0x007F, 0x00A0 },		// Delete, C1 controls and no-break space.
		{ 0x00AD, 0x00AD },		// Soft hyphen.
		{ 0x0300, 0x036F },		// Combining diacritical marks (e.g., an accent in NFD).
		{ 0x061C, 0x061C },		// Arabic letter mark.
		{ 0x115F, 0x1160 },		// Hangul fillers.
		{ 0x1680, 0x1680 },		// Ogham space mark.
		{ 0x17B4, 0x17B5 },		// Khmer inherent vowels.
		{ 0x180B, 0x180F },		// Mongolian variation selectors and vowel separator.
		{ 0x2000, 0x200F },		// Spaces, zero-width characters and directional marks.
		{ 0x2028, 0x202F },		// Line and paragraph separators, directional formatting and narrow no-break space.
		{ 0x205F, 0x206F },		// Medium mathematical space, word joiner, invisible operators and directional isolates.
		{ 0x3000, 0x3000 },		// Ideographic space.
		{ 0x3164, 0x3164 },		// Hangul filler.
		{ 0xE000, 0xF8FF },		// Private use area.
		{ 0xFDD0, 0xFDEF },		// Non-characters.
		{ 0xFE00, 0xFE0F },		// Variation selectors.
		{ 0xFEFF, 0xFEFF },		// Zero-width no-break space (i.e., byte order mark).
		{ 0xFFA0, 0xFFA0 },		// Halfwidth hangul filler.
		{ 0xFFF0, 0xFFF8 },		// Unassigned specials.
		{ 0x1BCA0, 0x1BCA3 },	// Shorthand format controls.
		{ 0x1D173, 0x1D17A },	// Musical symbol format controls.
		{ 0xE0000, 0xE0FFF },	// Tags and variation selectors supplement.
		{ 0xF0000, 0x10FFFF }	// Supplementary private use areas.
	};
	if ((codePoint & 0xFFFE) == 0xFFFE) {
		// The last two code points of every plane are non-characters.
		return true;
	}
	for (unsigned int k = 0; k < sizeof(ranges) / sizeof(ranges[0]); ++k) {
		if (codePoint < ranges[k][0]) {
			return false;
		}
		if (codePoint <= ranges[k][1]) {
			return true;
		}
	}
	return false;
}

/**
 * Decodes the code point of a lexeme that is a single character in valid
 * UTF-8 (see "multibyteCharacter" in "FlexPatterns.l").
 *
 * @see https://www.rfc-editor.org/rfc/rfc3629#section-3
 */
static unsigned int _toCodePoint(const Token * token) {
	const unsigned char * bytes = (const unsigned char *) token->lexeme;
	if (token->length == 1) {
		return bytes[0];
	}
	unsigned int codePoint = bytes[0] & (0xFF >> (1 + token->length));
	for (unsigned int k = 1; k < token->length; ++k) {
		codePoint = (codePoint << 6) | (bytes[k] & 0x3F);
	}
	return codePoint;
}

/**
 * Creates a new string (using heap-memory) that shows an unknown lexeme in an
 * error message. The lexeme is either a character in valid UTF-8, or a single
 * byte that is not. A visible character is shown between quotes; an invisible
 * or non-printable character, by its code point (e.g., U+FEFF); and an invalid
 * byte, by its hexadecimal code (e.g., \x80).
 */
static char * _toCharacterString(const Token * token) {
	char string[16];
	const unsigned char byte = token->lexeme[0];
	if (token->length == 1 && 0x80 <= byte) {
		snprintf(string, sizeof(string), "\\x%02X", byte);
		return strdup(string);
	}
	const unsigned int codePoint = _toCodePoint(token);
	if (_isInvisibleCodePoint(codePoint)) {
		snprintf(string, sizeof(string), "U+%04X", codePoint);
		return strdup(string);
	}
	return concatenate(3, "'", token->lexeme, "'");
}

/**
 * Creates a new string (using heap-memory) with the escape sequences of the
 * string fragment resolved. The lexeme is a valid fragment, so every "\" is
 * followed by the escaped character.
 */
static char * _unescapeStringFragment(const char * lexeme) {
	char * string = calloc(1 + strlen(lexeme), sizeof(char));
	unsigned int length = 0;
	for (unsigned int k = 0; lexeme[k] != '\0'; ++k) {
		if (lexeme[k] == '\\') {
			++k;
		}
		string[length++] = lexeme[k];
	}
	return string;
}

/* PUBLIC FUNCTIONS */

CompilationStatus ArithmeticOperatorLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus BeginInterpolationLexemeAction(FlexContext context) {
	CompilationStatus status = _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, INTERPOLATION_BEGIN));
	_enterContext(context);
	return status;
}

CompilationStatus BeginStringLexemeAction(FlexContext context) {
	CompilationStatus status = _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, STRING_BEGIN));
	_enterContext(context);
	return status;
}

CompilationStatus BooleanLexemeAction(const bool value) {
	Token * token = createToken(_lexicalAnalyzer, BOOLEAN);
	token->semanticValue->boolean = value;
	return _pushTokenAction(__FUNCTION__, token);
}

/**
 * The byte order mark is ignored only at the beginning of the input. Its rule
 * only matches at the beginning of a line, so it's the beginning of the input
 * if it's the first line; anywhere else, it's an unknown character.
 */
CompilationStatus ByteOrderMarkLexemeAction() {
	if (currentLexicalAnalyzerLine(_lexicalAnalyzer) == 1) {
		return IgnoredLexemeAction();
	}
	return UnknownLexemeAction();
}

CompilationStatus DelimiterLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus EndInterpolationLexemeAction() {
	CompilationStatus status = _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, INTERPOLATION_END));
	_leaveContext();
	return status;
}

CompilationStatus EndStringLexemeAction() {
	CompilationStatus status = _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, STRING_END));
	_leaveContext();
	return status;
}

CompilationStatus EnterMultilineCommentLexemeAction(FlexContext context) {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, OPEN_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	_enterContext(context);
	return IN_PROGRESS;
}

/**
 * An unclosed context is reported at the line where it begins, before the
 * parser receives the end of the input (otherwise, the parser could report a
 * syntax error too).
 */
CompilationStatus EOFLexemeAction() {
	CompilationStatus status = IN_PROGRESS;
	Token * token = createToken(_lexicalAnalyzer, 0);
	_logTokenAction(__FUNCTION__, token);
	FlexContext context = currentLexicalAnalyzerContext(_lexicalAnalyzer);
	if (0 != context) {
		char * message = concatenate(2, "unterminated ", _toConstructString(context));
		status = _throwLexicalErrorAtLine(_contextLines[_contextLinesLength - 1], message);
		free(message);
	}
	else if (!popInputBuffer(_lexicalAnalyzer)) {
		status = pushToken(_lexicalAnalyzer, token);
	}
	destroyToken(token);
	return status;
}

CompilationStatus IdentifierLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, ID);
	token->semanticValue->string = strdup(token->lexeme);
	return _pushTokenAction(__FUNCTION__, token);
}

CompilationStatus IgnoredLexemeAction() {
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, IGNORED);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus IntegerLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, INTEGER);
	errno = 0;
	const long value = strtol(token->lexeme, NULL, 10);
	if (errno == ERANGE || INT_MAX < value) {
		destroyToken(token);
		return _throwLexicalError(__FUNCTION__, "the integer exceeds INT_MAX");
	}
	token->semanticValue->integer = (int) value;
	return _pushTokenAction(__FUNCTION__, token);
}

CompilationStatus InvalidEscapeLexemeAction() {
	return _throwLexicalError(__FUNCTION__, "invalid escape sequence (only \\\", \\\\ and \\{ are allowed)");
}

bool isExceptionThrown() {
	return _exceptionThrown;
}

CompilationStatus KeywordLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus LeaveMultilineCommentLexemeAction() {
	_leaveContext();
	if (_logIgnoredLexemes) {
		Token * token = createToken(_lexicalAnalyzer, CLOSE_COMMENT);
		_logTokenAction(__FUNCTION__, token);
		destroyToken(token);
	}
	return IN_PROGRESS;
}

CompilationStatus PunctuationLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus RelationalOperatorLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus StringFragmentLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, STRING_FRAGMENT);
	token->semanticValue->string = _unescapeStringFragment(token->lexeme);
	char * _value = escape(token->semanticValue->string);
	logDebugging(_logger, WARNING_COLOR "%s" DEFAULT_COLOR ": unescaped value=%s\"%s\"%s",
		__FUNCTION__,
		INFORMATION_COLOR, _value, DEFAULT_COLOR);
	free(_value);
	_value = NULL;
	return _pushTokenAction(__FUNCTION__, token);
}

CompilationStatus UnitLexemeAction(TokenLabel label) {
	return _pushTokenAction(__FUNCTION__, createToken(_lexicalAnalyzer, label));
}

CompilationStatus UnknownLexemeAction() {
	Token * token = createToken(_lexicalAnalyzer, UNKNOWN);
	char * character = _toCharacterString(token);
	const char * hint = strcmp(token->lexeme, ";") == 0 ? " (Vial statements don't need ';')" : "";
	char * message = concatenate(3, "unexpected character ", character, hint);
	destroyToken(token);
	CompilationStatus status = _throwLexicalError(__FUNCTION__, message);
	free(character);
	free(message);
	return status;
}

CompilationStatus UnterminatedStringLexemeAction() {
	return _throwLexicalError(__FUNCTION__, "unterminated string (a string cannot contain line breaks)");
}
