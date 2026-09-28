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
static void _leaveContext();
static void _logTokenAction(const char * actionName, Token * token);
static CompilationStatus _pushTokenAction(const char * actionName, Token * token);
static CompilationStatus _throw();
static CompilationStatus _throwLexicalError(const char * actionName, const char * message);
static CompilationStatus _throwLexicalErrorAtLine(const unsigned int line, const char * message);
static char * _toCharacterString(const char * lexeme);
static const char * _toConstructString(const FlexContext context);
static const char * _toContextString(const FlexContext context);
static char * _unescapeStringFragment(const char * lexeme);

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
 * Creates a new string (using heap-memory) that shows an unknown lexeme in an
 * error message. A single byte that is not a printable ASCII character (i.e.,
 * a control character or a byte of an invalid UTF-8 sequence) is shown by its
 * hexadecimal code.
 */
static char * _toCharacterString(const char * lexeme) {
	const unsigned char byte = lexeme[0];
	if (lexeme[1] == '\0' && !isprint(byte)) {
		char * string = calloc(5, sizeof(char));
		snprintf(string, 5, "\\x%02X", byte);
		return string;
	}
	return strdup(lexeme);
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
	char * character = _toCharacterString(token->lexeme);
	const char * hint = strcmp(token->lexeme, ";") == 0 ? " (Vial statements don't need ';')" : "";
	char * message = concatenate(4, "unexpected character '", character, "'", hint);
	destroyToken(token);
	CompilationStatus status = _throwLexicalError(__FUNCTION__, message);
	free(character);
	free(message);
	return status;
}

CompilationStatus UnterminatedStringLexemeAction() {
	return _throwLexicalError(__FUNCTION__, "unterminated string (a string cannot contain line breaks)");
}
