#pragma once
#include "token.h"

void lexer_init(const char *source);
Token next_token(void);
