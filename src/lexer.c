#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "token.h"
#include "lexer.h"
static const char *src;
static int current_line;

void lexer_init(const char *source)
{
	src = source;
	current_line = 1;
}

static char peek(void)
{
	return *src;
}

static char advance(void)
{
	return *src++;
}

static int is_ident_start(char c)
{
	return isalpha(c);
}

static int is_ident(char c)
{
	return isalnum(c) || c == '_';
}

Token next_token(void)
{
	while (isspace(peek())) 
	{
		if (peek() == '\n')
        	current_line++;
		advance();
	} 

	Token tok = {0};
	tok.start = src;
	tok.length = 0;
	tok.value = 0;
	tok.line = current_line;

	char c = advance();
       	
	if (c == '\0') 
	{
		tok.type = TOK_EOF;
		return tok;
	}

	
	if (c == '\'') 
	{
		char ch = 0;

		if (peek() == '\'') 
		{
			fprintf(stderr, "Error: empty char literal at position %ld\n", src - tok.start);
			advance(); 
			tok.type = TOK_NUMBER;
			tok.value = 0;
			tok.length = src - tok.start;
			return tok;
		}

		if (peek() == '\\') 
		{
			advance(); 
			char esc = advance();
			switch (esc) 
			{
				case 'n':  ch = '\n'; break;
				case 'r':  ch = '\r'; break;
				case 't':  ch = '\t'; break;
				case '0':  ch = '\0'; break;
				case '\\': ch = '\\'; break;
				case '\'': ch = '\''; break;
				default:
					fprintf(stderr, "Warning: unknown escape sequence \\%c at position %ld\n", esc, src - tok.start);
					ch = esc; 
			}
		} 
		else 
		{
			ch = advance();
		}

		if (advance() != '\'') 
		{
			fprintf(stderr, "Error: missing closing single quote for char literal at position %ld\n", src - tok.start);
			while (*src && *src != '\'' && !isspace(*src)) advance();
			if (*src == '\'') advance(); 
		}

		tok.type = TOK_NUMBER;        
		tok.value = (long)ch;         
		tok.length = src - tok.start;
		tok.suffix = "ch";
		tok.suffix_length = 2;

		return tok;
	}


	if (isdigit(c) || (c == '0' && strchr("xXbBoO", peek()))) 
	{
		long long val = 0;
		long long base = 10;

		if (c == '0') 
		{
			char p = peek();
			if (p == 'x' || p == 'X') { advance(); base = 16; }
			else if (p == 'b' || p == 'B') { advance(); base = 2; }
			else if (p == 'o' || p == 'O') { advance(); base = 8; }
		}

		char digit = c;
		do
		{
			
			long long n = -1;
			if (base == 16) 
			{
				if (isdigit(digit)) n = digit - '0';
				else if (digit >= 'a' && digit <= 'f') n = digit - 'a' + 10;
				else if (digit >= 'A' && digit <= 'F') n = digit - 'A' + 10;
			} 
			else if (base == 10) 
			{
				if (isdigit(digit)) n = digit - '0';
			} 
			else if (base == 2 || base == 8) 
			{
				if (digit >= '0' && digit <= ((base==2)?'1':'7')) n = digit - '0';
			}
			if (n == -1) break;
			val = val * base + n;
		} while((digit=advance()));
		src--;
		const char *suffix_start = src;
		while (isalnum(peek())) advance();
		int suffix_len = src - suffix_start;
		tok.type = TOK_NUMBER;
		tok.value = val;
		tok.length = src - tok.start - suffix_len;
		tok.suffix = suffix_start;
		tok.suffix_length = suffix_len;

		return tok;
	}


	if (is_ident_start(c))
	{
		while (is_ident(peek())) advance();
		tok.length = src - tok.start;

		if (tok.length == 2 && !strncmp(tok.start,"fn",2))
			tok.type = TOK_FN;
		else if (tok.length == 6 && !strncmp(tok.start, "return", 6))
			tok.type = TOK_RETURN;
		else if (tok.length == 5 && !strncmp(tok.start, "while", 5))
			tok.type = TOK_WHILE;
		else if (tok.length == 3 && !strncmp(tok.start, "for", 3))
			tok.type = TOK_FOR;
		else if (tok.length == 5 && !strncmp(tok.start, "print", 5))
			tok.type = TOK_PRINT;
		else if (tok.length == 2 && !strncmp(tok.start, "if", 2))
			tok.type = TOK_IF;
		else if (tok.length == 4 && !strncmp(tok.start, "else", 4))
			tok.type = TOK_ELSE;
		else if (tok.length == 4 && !strncmp(tok.start, "cast", 4))
        	tok.type = TOK_CAST;
		else if (tok.length == 7 && !strncmp(tok.start, "bitcast", 7))
			tok.type = TOK_BITCAST;
		else if (tok.length == 6 && !strncmp(tok.start, "struct", 6))
			tok.type = TOK_STRUCT;
		else 
			tok.type = TOK_IDENT;

		return tok;
	}

	tok.length = 1;
	switch (c) 
	{
		case '<':
			if (peek() == '=')
        	{
            	advance();
            	tok.type = TOK_LE;
            	tok.length = 2;
        	}
			else if (peek() == '<')
			{
				advance();
				tok.type = TOK_BIT_SHL;
				tok.length = 2;
			}
        	else
        	{
            	tok.type = TOK_LT;
        	}
        	return tok;
		case '>':
			if (peek() == '=')
			{
            	advance();
            	tok.type = TOK_GE;
            	tok.length = 2;
        	}
			else if (peek() == '>')
			{
				advance();
				tok.type = TOK_BIT_SHR;
				tok.length = 2;
			}
        	else
        	{
            	tok.type = TOK_GT;
        	}
        	return tok;
		case '!':
			if (peek() == '=')
    		{
        		advance();
        		tok.type = TOK_NE;
        		tok.length = 2;
    		}
    		else
    		{
        		tok.type = TOK_NOT;
    		}
    		return tok;
		case '&':
			if (peek() == '&')
			{
				advance();
				tok.type = TOK_AND;
				tok.length = 2;
			}
			else
			{
				tok.type = TOK_BIT_AND;
			}
			return tok;
		case '|':
			if (peek() == '|')
			{
				advance();
				tok.type = TOK_OR;
				tok.length = 2;
			}
			else
			{
				tok.type = TOK_BIT_OR;
			}
			return tok;	
		case '^':
			if (peek() == '^')
			{
				advance();
				tok.type = TOK_XOR;
				tok.length = 2;
			}
			else
			{
				tok.type = TOK_BIT_XOR;
			}
			return tok;
		case '~': tok.type = TOK_BIT_NOT; return tok;
		case '(': tok.type = TOK_LPAREN; return tok;
		case ')': tok.type = TOK_RPAREN; return tok;
		case '{': tok.type = TOK_LBRACE; return tok;
		case '}': tok.type = TOK_RBRACE; return tok;
		case '[': tok.type = TOK_LSQRBRACE; return tok;
		case ']': tok.type = TOK_RSQRBRACE; return tok;
		case '+': tok.type = TOK_PLUS; 	 return tok;
		case '-': 
			if(peek() == '>')
			{
				advance();
				tok.type = TOK_ARROW;
				tok.length = 2;
			}
			else
			{
				tok.type = TOK_MINUS; 	
			}
			return tok;
		case '*': tok.type = TOK_STAR;   return tok;
		case '/': tok.type = TOK_SLASH;  return tok;
		case '%': tok.type = TOK_PERCENT;return tok;
		case '@': tok.type = TOK_AT;     return tok;
		case '=':
			if (peek() == '=')
    		{
        		advance();
        		tok.type = TOK_IE;
        		tok.length = 2;
    		}
    		else
    		{
        		tok.type = TOK_EQUAL;
    		}
    		return tok;
		case ',': tok.type = TOK_COMMA; return tok;
		case '.': tok.type = TOK_DOT;   return tok;
		case ':': tok.type = TOK_COLON; return tok;
		case ';': tok.type = TOK_SEMI;   return tok;
	}

	tok.type = TOK_EOF;
	return tok;
}


			
