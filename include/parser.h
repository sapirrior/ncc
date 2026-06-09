#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// Parse token stream and return the root ASTNode (AST_PROGRAM)
ASTNode *parse(Token *tokens);

#endif // PARSER_H
