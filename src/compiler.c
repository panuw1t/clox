#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isConst;
} Local;

typedef struct {
  Local* locals;
  int localCount;
  int scopeDepth;
} Compiler;

Parser parser;

Compiler* current = NULL;

Chunk* compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
  fprintf(stderr, "[line %d] Error", token->line);
  if (parser.panicMode) return;
  parser.panicMode = true;
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}

static void error(const char* message) {
  errorAt(&parser.previous, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitReturn() {
  emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler) {
  compiler->locals = NULL;
  compiler->locals = GROW_ARRAY(Local, compiler->locals, 0, UINT8_COUNT);
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  current = compiler;
}

static void endCompiler() {
  emitReturn();

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif
}

static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth >
         current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount--;
  }
}

static void expression();
static void statement();
static void declaration();
static uint8_t identifierConstant(Token* name);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void and_(bool canAssign);
static void varDeclaration();
static int resolveLocal(Compiler* compiler, Token* name);

static void binary(bool canAssign) {
  TRACE_ENTER();
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT);   break;
  case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL);            break;
  case TOKEN_GREATER:       emitByte(OP_GREATER);          break;
  case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT);    break;
  case TOKEN_LESS:          emitByte(OP_LESS);             break;
  case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
  case TOKEN_PLUS:          emitByte(OP_ADD);              break;
  case TOKEN_MINUS:         emitByte(OP_SUBTRACT);         break;
  case TOKEN_STAR:          emitByte(OP_MULTIPLY);         break;
  case TOKEN_SLASH:         emitByte(OP_DIVIDE);           break;
  default: return;              // Unreachable.
  }
  TRACE_EXIT();
}

static void literal(bool canAssign) {
  TRACE_ENTER();
  switch (parser.previous.type) {
  case TOKEN_FALSE: emitByte(OP_FALSE); break;
  case TOKEN_NIL: emitByte(OP_NIL); break;
  case TOKEN_TRUE: emitByte(OP_TRUE); break;
  default: return; // Unreachable.
  }
  TRACE_EXIT();
}

static void grouping(bool canAssign) {
  TRACE_ENTER();
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  TRACE_EXIT();
}

static void number(bool canAssign) {
  TRACE_ENTER();
  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
  TRACE_EXIT();
}

static void or_(bool canAssign) {
  TRACE_ENTER();
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
  TRACE_EXIT();
}

static void string(bool canAssign) {
  TRACE_ENTER();
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
  TRACE_EXIT();
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  bool isLong = arg > UINT8_MAX;
  if (arg != -1) {
    if (isLong) {
      getOp = OP_GET_SHORT_LOCAL;
      setOp = OP_SET_SHORT_LOCAL;
    } else {
      getOp = OP_GET_LOCAL;
      setOp = OP_SET_LOCAL;
    }
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    if (setOp == OP_SET_LOCAL && current->locals[arg].isConst) {
      error("Can not assign val variable.");
    }
    expression();
    if (isLong){
      uint8_t byte1 = (arg >> 8) & 0xff;
      uint8_t byte2 = arg & 0xff;
      emitByte(setOp);
      emitBytes(byte1, byte2);
    } else {
      emitBytes(setOp, (uint8_t)arg);
    }
  } else {
    if (isLong){
      uint8_t byte1 = (arg >> 8) & 0xff;
      uint8_t byte2 = arg & 0xff;
      emitByte(getOp);
      emitBytes(byte1, byte2);
    } else {
      emitBytes(getOp, (uint8_t)arg);
    }
  }
}

static void variable(bool canAssign) {
  TRACE_ENTER();
  namedVariable(parser.previous, canAssign);
  TRACE_EXIT();
}

static void unary(bool canAssign) {
  TRACE_ENTER();
  TokenType operatorType = parser.previous.type;

  // Compile the operand.
  parsePrecedence(PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
  case TOKEN_BANG: emitByte(OP_NOT); break;
  case TOKEN_MINUS: emitByte(OP_NEGATE); break;
  default: return; // Unreachable.
  }
  TRACE_EXIT();
}

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_NONE},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);

    if (canAssign && match(TOKEN_EQUAL)) {
      error("Invalid assignment target.");
    }
  }
}

static uint8_t identifierConstant(Token* name) {
  Chunk* chunk = currentChunk();
  Value value = OBJ_VAL(copyString(name->start, name->length));
  for (int i = 0; i < chunk->constants.count; i++) {
    if (IS_STRING(chunk->constants.values[i]) &&
        AS_OBJ(chunk->constants.values[i]) == AS_OBJ(value)){
      return i;
    }
  }
  return makeConstant(value);
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

static void addLocal(Token name, bool isConst) {
  if (current->localCount >= UINT16_COUNT) {
    error("Too many local variables in function.");
    return;
  }
  if (current->localCount >= UINT8_COUNT) {
    current->locals = GROW_ARRAY(Local, current->locals, UINT8_COUNT, UINT16_COUNT);
  }
  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isConst = isConst;
}

static void declareVariable(bool isConst) {
  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }
  addLocal(*name, isConst);
}

static uint8_t parseVariable(const char* errorMessage, bool isConst) {
  consume(TOKEN_IDENTIFIER, errorMessage);
  declareVariable(isConst);
  if (current->scopeDepth > 0) return 0;
  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global, bool isConst) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
  if (isConst) {
    emitByte(OP_TRUE);
  } else {
    emitByte(OP_FALSE);
  }
}

static void and_(bool canAssign) {
  TRACE_ENTER();
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
  TRACE_EXIT();
}

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void expression() {
  TRACE_ENTER();
  parsePrecedence(PREC_ASSIGNMENT);
  TRACE_EXIT();
}

static void block() {
  TRACE_ENTER();
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  TRACE_EXIT();
}

static void expressionStatement() {
  TRACE_ENTER();
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
  TRACE_EXIT();
}

static void forStatement() {
  TRACE_ENTER()
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }

  int loopStart = currentChunk()->count;
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Condition.
  }
  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  statement();
  emitLoop(loopStart);
  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }
  endScope();
  TRACE_EXIT();
}

static void switchStatement() {
  TRACE_ENTER()
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' after block.");
  int jump = emitJump(OP_JUMP);
  int toEnd = currentChunk()->count;
  int def = currentChunk()->count;
  int endJump = emitJump(OP_JUMP);
  patchJump(jump);
  while (match(TOKEN_CASE)) {
    emitByte(OP_DUP);
    expression();
    consume(TOKEN_COLON, "Expect ':' after expression.");
    emitByte(OP_EQUAL);
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(toEnd);
    patchJump(thenJump);
    emitByte(OP_POP);
  }
  if (match(TOKEN_DEFAULT)) {
    consume(TOKEN_COLON, "Expect ':' after expression.");
    int jump = emitJump(OP_JUMP);
    def = currentChunk()->count;
    statement();
    emitLoop(toEnd);
    patchJump(jump);
  }
  while (match(TOKEN_CASE)) {
    emitByte(OP_DUP);
    expression();
    consume(TOKEN_COLON, "Expect ':' after expression.");
    emitByte(OP_EQUAL);
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(toEnd);
    patchJump(thenJump);
    emitByte(OP_POP);
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  emitLoop(def);
  patchJump(endJump);
  emitByte(OP_POP);
  TRACE_EXIT();
}

static void ifStatement() {
  TRACE_ENTER();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE)) statement();
  patchJump(elseJump);
  TRACE_EXIT();
}

static void varDeclaration() {
  TRACE_ENTER();
  uint8_t global = parseVariable("Expect variable name.", false);

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global, false);
  TRACE_EXIT();
}

static void valDeclaration() {
  TRACE_ENTER();
  uint8_t global = parseVariable("Expect variable name.", true);

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global, true);
  TRACE_EXIT();
}

static void printStatement() {
  TRACE_ENTER();
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
  TRACE_EXIT();
}

static void whileStatement() {
  TRACE_ENTER();
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
  TRACE_EXIT();
}

static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:
      ; // Do nothing.
    }

    advance();
  }
}

static void declaration() {
  TRACE_ENTER();
  if (match(TOKEN_VAR)) {
    varDeclaration();
  }
  else if (match(TOKEN_VAL)) {
    valDeclaration();
  } else {
    statement();
  }
  TRACE_EXIT();
  if (parser.panicMode) synchronize();
}

static void statement(int loopStat) {
  TRACE_ENTER();
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
  TRACE_EXIT();
}

bool compile(const char* source, Chunk* chunk) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler);
  compilingChunk = chunk;
  parser.hadError = false;
  parser.panicMode = false;
  advance();
  while (!match(TOKEN_EOF)) {
    declaration();
  }
  endCompiler();
  return !parser.hadError;
}
