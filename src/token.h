#pragma once
typedef enum {
	TOK_EOF,
	TOK_IDENT,
	TOK_NUMBER,

	TOK_FN,
	TOK_RETURN,
	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_FOR,
	TOK_STRUCT,

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_RSQRBRACE,
	TOK_LSQRBRACE,

	TOK_PLUS,
	TOK_MINUS,
	TOK_STAR,
	TOK_SLASH,
	TOK_PERCENT,

	TOK_IE,			//is equal
	TOK_NE,			//not equal
	TOK_LT,			//smaller than
	TOK_LE,			//smaller or equal
	TOK_GT,			//greater than
	TOK_GE,			//greater equal

	TOK_AND,
	TOK_OR,
	TOK_XOR,
	TOK_NOT,

	TOK_BIT_AND,
	TOK_BIT_OR,
	TOK_BIT_XOR,
	TOK_BIT_NOT,
	TOK_BIT_SHL,
	TOK_BIT_SHR,

	TOK_EQUAL,
	TOK_AT,
	TOK_COMMA,
	TOK_DOT,
	TOK_COLON,
	TOK_SEMI,
	TOK_ARROW,
	TOK_PRINT,
	TOK_CAST,
	TOK_BITCAST
} TokenType;

typedef struct {
	TokenType type;
	const char *start;
	int length;
	long long value;
	const char *suffix;    
    int suffix_length;     
	int line;
} Token;


static inline const char *token_type_name(TokenType type)
{
    switch (type)
    {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "IDENT";
        case TOK_NUMBER: return "NUMBER";

        case TOK_FN: return "FN";
        case TOK_RETURN: return "RETURN";
        case TOK_IF: return "IF";
        case TOK_ELSE: return "ELSE";
        case TOK_WHILE: return "WHILE";
        case TOK_FOR: return "FOR";
		case TOK_STRUCT: return "STRUCT";

        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
		case TOK_LSQRBRACE: return "LSQRBRACE";
		case TOK_RSQRBRACE: return "RSQRBRACE";

        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";

        case TOK_IE: return "IE";           // is equal
        case TOK_NE: return "NE";           // not equal
        case TOK_LT: return "LT";           // smaller than
        case TOK_LE: return "LE";           // smaller or equal
        case TOK_GT: return "GT";           // greater than
        case TOK_GE: return "GE";           // greater equal
		case TOK_AND: return "AND";
		case TOK_OR: return "OR";
		case TOK_XOR: return "XOR";
		case TOK_NOT: return "NOT";
		case TOK_BIT_AND: return "BIT_AND";
		case TOK_BIT_OR: return "BIT_OR";
		case TOK_BIT_XOR: return "BIT_XOR";
		case TOK_BIT_NOT: return "BIT_NOT";
		case TOK_BIT_SHL: return "BIT_SHL";
		case TOK_BIT_SHR: return "BIT_SHR";

        case TOK_EQUAL: return "EQUAL";
        case TOK_COMMA: return "COMMA";
		case TOK_DOT: return "DOT";
		case TOK_COLON: return "COLON";
		case TOK_ARROW: return "ARROW";
		case TOK_AT: return "AT";
        case TOK_SEMI: return "SEMI";
		case TOK_PRINT: return "PRINT";
		case TOK_CAST: return "CAST";
		case TOK_BITCAST: return "BITCAST";
    }
    return "UNKNOWN";
}