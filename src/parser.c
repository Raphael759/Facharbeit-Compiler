#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"
#include "ast.h"
#include "parser.h"
#include "lexer.h"
#include "type.h"
#include "debug.h"


static Ast *parse_expression(void);
static Ast *parse_logical_or(void);
static Ast *parse_logical_xor(void);
static Ast *parse_logical_and(void);
static Ast *parse_bit_or(void);
static Ast *parse_bit_xor(void);
static Ast *parse_bit_and(void);
static Ast *parse_compare(void);
static Ast *parse_shift(void);
static Ast *parse_add(void);
static Ast *parse_mul(void);
static Ast *parse_unary(void);
static Ast *parse_primary(void);

static Ast *parse_statement(void);
static Ast *parse_loop(int loopType);
static Ast **parse_block(int *count);
static Type *parse_type(void);
static Ast *parse_array_literal(void);
static Ast *parse_struct_decl(void);
Ast *parse_function(void);


static Token current;
static Token next;
static int has_peeked = 0;

static Ast ***current_block_stmts = NULL;
static int *current_block_count = NULL;


static void insert_ast(Ast *decl)
{
    if (!current_block_stmts || !current_block_count) return;
    *current_block_stmts = realloc(*current_block_stmts,sizeof(Ast*) * (*current_block_count + 1));
    (*current_block_stmts)[*current_block_count] = decl;
	(*current_block_count)++;
}

static Token advance(void)
{
    if (has_peeked)
    {
        current = next;
        has_peeked = 0;
    }
    else
    {
        current = next_token();
    }
    return current;
}

static void expect(TokenType type)
{
	if(current.type != type)
	{
		fprintf(stderr, "parser error at line %d: expected %s and not %s \n",current.line,token_type_name(type),token_type_name(current.type));
		exit(1);
	}
	advance();
}

static Token peek_token(void)
{
    if (!has_peeked)
    {
        next = next_token();
        has_peeked = 1;
    }
    return next;
}

Ast **parse_program(int *count)
{
    advance();
	Ast **functions = NULL;
	*count = 0;

	while (current.type != TOK_EOF)
	{
		Ast *node;

		if (current.type == TOK_STRUCT)
			node = parse_struct_decl();
		else
			node = parse_function();

		functions = realloc(functions, sizeof(Ast*) * (*count + 1));
		functions[*count] = node;
		(*count)++;
	}
        
	expect(TOK_EOF);
        return functions;
}


static Ast *parse_expression(void)
{
	return parse_logical_or();
}

static Ast *parse_logical_or(void)
{
	Ast *left = parse_logical_xor();
	while (current.type == TOK_OR)
	{
		advance();
		Ast *right = parse_logical_xor();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_OR;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_logical_xor(void)
{
	Ast *left = parse_logical_and();
	while (current.type == TOK_XOR)
	{
		advance();
		Ast *right = parse_logical_and();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_XOR;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_logical_and(void)
{
	Ast *left = parse_bit_or();
	while (current.type == TOK_AND)
	{
		advance();
		Ast *right = parse_bit_or();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_AND;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_bit_or(void)
{
	Ast *left = parse_bit_xor();
	while (current.type == TOK_BIT_OR)
	{
		advance();
		Ast *right = parse_bit_xor();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_BIT_OR;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_bit_xor(void)
{
	Ast *left = parse_bit_and();
	while (current.type == TOK_BIT_XOR)
	{
		advance();
		Ast *right = parse_bit_and();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_BIT_XOR;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_bit_and(void)
{
	Ast *left = parse_compare();
	while (current.type == TOK_BIT_AND)
	{
		advance();
		Ast *right = parse_compare();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = OP_BIT_AND;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_compare(void)
{
	Ast *left = parse_shift();
	while (current.type == TOK_LT || current.type == TOK_GT || current.type == TOK_LE || current.type == TOK_GE || current.type == TOK_IE || current.type == TOK_NE)
	{
		BinOp op;
		switch (current.type)
		{
			case TOK_LT: op = OP_LT; break;
			case TOK_GT: op = OP_GT; break;
			case TOK_LE: op = OP_LE; break;
			case TOK_GE: op = OP_GE; break;
			case TOK_IE: op = OP_IE; break;
			case TOK_NE: op = OP_NE; break;
			default:
				fprintf(stderr, "parser error at line %d: unexpected token in comparison\n", current.line);
				exit(1);
		}

		advance();
		Ast *right = parse_shift();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = op;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_shift(void)
{
	Ast *left = parse_add();
	while (current.type == TOK_BIT_SHL || current.type == TOK_BIT_SHR)
	{
		BinOp op = (current.type == TOK_BIT_SHL) ? OP_BIT_SHL : OP_BIT_SHR;

		advance();
		Ast *right = parse_add();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = op;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_add(void)
{
	Ast *left = parse_mul();
	while (current.type == TOK_PLUS || current.type == TOK_MINUS)
	{
		BinOp op = (current.type == TOK_PLUS) ? OP_ADD : OP_SUB;

		advance();
		Ast *right = parse_mul();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = op;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_mul(void)
{
	Ast *left = parse_unary();
	while (current.type == TOK_STAR || current.type == TOK_SLASH || current.type == TOK_PERCENT)
	{
		BinOp op;
		switch (current.type)
		{
			case TOK_STAR: op = OP_MUL; break;
			case TOK_SLASH: op = OP_DIV; break;
			case TOK_PERCENT: op = OP_MOD; break;
			default:
				fprintf(stderr, "parser error at line %d: unexpected token in multiplication\n", current.line);
				exit(1);
		}

		advance();
		Ast *right = parse_unary();

		Ast *b = malloc(sizeof(Ast));
		b->type = AST_BINARY;
		b->binary.op = op;
		b->binary.left = left;
		b->binary.right = right;
		left = b;
	}
	return left;
}

static Ast *parse_unary(void)
{
	if (current.type == TOK_NOT || current.type == TOK_BIT_NOT || current.type == TOK_MINUS || current.type == TOK_STAR || current.type == TOK_BIT_AND)
	{
		UnOp op;
		switch (current.type)
		{
			case TOK_NOT: op = UN_NOT; break;
			case TOK_BIT_NOT: op = UN_BIT_NOT; break;
			case TOK_MINUS: op = UN_NEG; break;
			case TOK_STAR: op = UN_DEREF; break;
			case TOK_BIT_AND: op = UN_ADDR; break;
			default:
				fprintf(stderr, "parser error at line %d: unexpected token in unary expression\n", current.line);
				exit(1);
		}

		advance();
		Ast *expr = parse_unary();

		Ast *u = malloc(sizeof(Ast));
		u->type = AST_UNARY;
		u->unary.op = op;
		u->unary.expr = expr;
		return u;
	}
	else
	{
		return parse_primary();
	}
}

static Ast *parse_primary(void)
{
    Ast *node = NULL;

    //basis 
    if (current.type == TOK_NUMBER)
    {
        node = malloc(sizeof(Ast));
        node->type = AST_NUMBER;
        node->number = current.value;
		Type *t = NULL;
		if(current.suffix_length)
		{
			t = lookup_suffix(current.suffix, current.suffix_length);
			if(!t)
			{
				fprintf(stderr, "parser error at line %d: invalid number type suffix \n", current.line);
				exit(1);
			}
		}
		else
			t = &TYPE_INT64;

		node->type_ptr = t;
        advance();
    }

    else if (current.type == TOK_IDENT)
    {
        char *name = strndup(current.start, current.length);
        advance();

        if (current.type == TOK_LPAREN)	//fn call
        {
            advance();

            Ast **args = NULL;
            int arg_count = 0;

            while (current.type != TOK_RPAREN)
            {
                Ast *arg = parse_expression();
                args = realloc(args, sizeof(Ast*) * (arg_count + 1));
                args[arg_count++] = arg;

                if (current.type == TOK_COMMA)
                    advance();
                else
                    break;
            }

            expect(TOK_RPAREN);

            node = malloc(sizeof(Ast));
            node->type = AST_FUNCTION_CALL;
            node->function_call.name = name;
            node->function_call.arguments = args;
            node->function_call.arg_count = arg_count;
            node->function_call.symbol = NULL;
        }
        else
        {
            node = malloc(sizeof(Ast));
            node->type = AST_VAR;
            node->var.name = name;
        }
    }

    else if (current.type == TOK_LPAREN)
    {
        advance();
        node = parse_expression();
        expect(TOK_RPAREN);
    }

    else if (current.type == TOK_CAST || current.type == TOK_BITCAST)
    {
		AstType cast_type = (current.type == TOK_CAST) ? AST_CAST : AST_BITCAST;
        advance();
        expect(TOK_LT);
        Type *target = parse_type();
        expect(TOK_GT);
		expect(TOK_LPAREN);
        Ast *expr = parse_expression();
		expect(TOK_RPAREN);

        node = malloc(sizeof(Ast));
        node->type = cast_type;
        node->cast.expr = expr;
        node->cast.target_type = target;
    }

    else
    {
        fprintf(stderr, "parser error at line %d: invalid expression\n", current.line);
        exit(1);
    }

    while (1)	//postfix -> array,  bit access
    {
        if (current.type == TOK_LSQRBRACE)
        {
            advance();
            Ast *index = parse_expression();
            expect(TOK_RSQRBRACE);

            Ast *idx = malloc(sizeof(Ast));
            idx->type = AST_ARRAY_INDEX;
            idx->array_index.array = node;
            idx->array_index.index = index;

            node = idx;
        }

        else if (current.type == TOK_AT)
        {
            advance();

            Ast *start = parse_expression();
            expect(TOK_COLON);
            Ast *end = parse_expression();

            Ast *bit = malloc(sizeof(Ast));
            bit->type = AST_BIT_ACCESS;
            bit->bit_access.base = node;
            bit->bit_access.start = start;
            bit->bit_access.end = end;

            node = bit;
        }	

		else if (current.type == TOK_DOT)
		{
			advance();

			if (current.type != TOK_IDENT)
			{
				fprintf(stderr, "parser error at line %d: expected member name after '.'\n", current.line);
				exit(1);
			}

			char *member = strndup(current.start, current.length);
			advance();

			Ast *m = malloc(sizeof(Ast));
			m->type = AST_MEMBER_ACCESS;
			m->member_access.base = node;
			m->member_access.member_name = member;
			m->member_access.member_offset = 0; // filled by type checker

			node = m;
		}

		else if (current.type == TOK_ARROW)
		{
			advance();

			if (current.type != TOK_IDENT)
			{
				fprintf(stderr, "parser error at line %d: expected member name after '->'\n", current.line);
				exit(1);
			}

			char *member = strndup(current.start, current.length);
			advance();

			// a->b == (*a).b

			Ast *deref = malloc(sizeof(Ast));
			deref->type = AST_UNARY;
			deref->unary.op = UN_DEREF;
			deref->unary.expr = node;

			Ast *m = malloc(sizeof(Ast));
			m->type = AST_MEMBER_ACCESS;
			m->member_access.base = deref;
			m->member_access.member_name = member;
			m->member_access.member_offset = 0; // filled by type checker

			node = m;
		}

        else
            break;
    }

    return node;
}


static Ast *parse_statement(void)
{
	DEBUG_PRINT("DEBUG parse_statement: current token = %s", token_type_name(current.type));
	if (current.type == TOK_IDENT)
	{
        DEBUG_PRINT(" (identifier: %.*s)", current.length, current.start);
    }
    DEBUG_PRINT("\n");

	Type *typ = parse_type();
	if (typ)
	{
		if (current.type != TOK_IDENT) 
		{
    		fprintf(stderr, "parser error at line %d:expected variable name after type\n",current.line);
    		exit(1);
		}

		char *name = strndup(current.start, current.length);
		advance();

		Ast *assign = NULL;
		if (current.type == TOK_EQUAL)
		{
			advance();
			if(current.type == TOK_LSQRBRACE)
				assign = parse_array_literal();
			else
				assign = parse_expression();
		}

		expect(TOK_SEMI);

		Ast *d = malloc(sizeof(Ast));
		d->type = AST_VAR_DECL;
		d->var_decl.name = name;
		d->var_decl.var_type = typ;
		d->var_decl.symbol = NULL;
		d->var_decl.init_value = assign;

        return d;
	}

	if (current.type == TOK_IDENT)
	{
		Ast *left = parse_expression();

		if (current.type == TOK_EQUAL)
		{
			advance();

			Ast *value;
			if (current.type == TOK_LSQRBRACE)
				value = parse_array_literal();
			else
				value = parse_expression();

			expect(TOK_SEMI);

			Ast *assign = malloc(sizeof(Ast));
			assign->type = AST_VAR_ASSIGN;
			assign->var_assign.target = left;
			assign->var_assign.value = value;
 
			return assign;
		}

		expect(TOK_SEMI);
		return left; 
	}

	if (current.type == TOK_STAR || current.type == TOK_LPAREN)
	{
		Ast *left = parse_expression();

		if (current.type == TOK_EQUAL)
		{
			advance();
			Ast *value = parse_expression();
			expect(TOK_SEMI);

			Ast *assign = malloc(sizeof(Ast));
			assign->type = AST_VAR_ASSIGN;
			assign->var_assign.target = left;
			assign->var_assign.value = value;
			return assign;
		}

		expect(TOK_SEMI);
		return left;
	}

	if (current.type == TOK_RETURN)
	{
		expect(TOK_RETURN);

		Ast *expr = parse_expression();
		expect(TOK_SEMI);

		Ast *r = malloc(sizeof(Ast));
		r->type = AST_RETURN;
		r->ret.expr = expr;
		return r;
	}

	if (current.type == TOK_PRINT)
	{
		expect(TOK_PRINT);
		expect(TOK_LPAREN);
		Ast *expr = parse_expression();
		expect(TOK_RPAREN);
		expect(TOK_SEMI);

		Ast *p = malloc(sizeof(Ast));
		p->type = AST_PRINT;
		p->print.expr = expr;
		return p;
	}

	if (current.type == TOK_WHILE)
	{
		DEBUG_PRINT("DEBUG: Parsing WHILE loop\n");
		expect(TOK_WHILE);
		Ast *l = parse_loop(0);
		return l;
	}
	if (current.type == TOK_FOR)
	{
		DEBUG_PRINT("DEBUG: Parsing FOR loop\n");
		expect(TOK_FOR);
		Ast *l = parse_loop(1);
		return l;
	}

	if(current.type == TOK_IF)
	{
		DEBUG_PRINT("DEBUG: Parsing IF statement\n");
		expect(TOK_IF);

		expect(TOK_LPAREN);
		Ast *cond = parse_expression();
		expect(TOK_RPAREN);
		Ast **body;
		int body_length;
		if(current.type == TOK_LBRACE)
			body = parse_block(&body_length);

		else
		{
			body = malloc(sizeof(Ast *));
			body[0] = parse_statement();
			body_length = 1;
		}
			
		Ast *i = malloc(sizeof(Ast));
		i->type = AST_IF;
		i->if_stmt.cond = cond;
		i->if_stmt.body = body;
		i->if_stmt.body_length = body_length;
		i->if_stmt.else_stmt = NULL;
		if(current.type == TOK_ELSE)
		{
			expect(TOK_ELSE);
			int body_len = 1;
			Ast **bod;
			if(current.type == TOK_LBRACE)
				bod = parse_block(&body_len);

			else
			{
				bod = malloc(sizeof(Ast *));
				bod[0] = parse_statement();
				body_len = 1;
			}
				

			Ast *el = malloc(sizeof(Ast));
			el->type = AST_ELSE;
			el->else_stmt.body = bod;
			el->else_stmt.body_length = body_len;
			
			i->if_stmt.else_stmt = el;
		}

		return i;
	}
	if(current.type == TOK_ELSE)
	{
		fprintf(stderr, "parser error at line %d: missing if statement before else\n", current.line);
		exit(1);
	}

	fprintf(stderr, "parser error at line %d: unknown statement (token: %s)\n",current.line, token_type_name(current.type));
	exit(1);
}


static Ast *parse_statement_no_semi(void)
{
    Ast *expr = parse_expression();

    if (current.type == TOK_EQUAL)
    {
        advance();
        Ast *value = parse_expression();

        Ast *a = malloc(sizeof(Ast));
        a->type = AST_VAR_ASSIGN;
        a->var_assign.target = expr;
        a->var_assign.value = value;

        return a;
    }

    return expr;
}

static Type *parse_type(void)
{
	if (current.type == TOK_STAR)
	{
		Token after = peek_token();

		if (after.type != TOK_STAR && !(after.type == TOK_IDENT && lookup_type(after.start, after.length)))
			return NULL;

		advance(); 
        Type *pointee = parse_type();
        if (!pointee)
        {
            fprintf(stderr, "parser error at line %d: expected type after '*'\n", current.line);
            exit(1);
        }
        return make_pointer_type(pointee);
	}

    if (current.type != TOK_IDENT)
        return NULL;

    Type *base = lookup_type(current.start, current.length);
    if (!base)
        return NULL;

    advance();  



	while (current.type == TOK_LSQRBRACE)
	{
		advance();
		if (current.type != TOK_NUMBER)
		{
			fprintf(stderr, "parser error at line %d: expected array length\n", current.line);
			exit(1);
		}

		int length = (int)current.value;
		advance();

		expect(TOK_RSQRBRACE);

		Type *arr_type = malloc(sizeof(Type));
		arr_type->kind = TYPE_ARRAY;
		arr_type->size = base->size * length;
		arr_type->array.element_type = base;
		arr_type->array.length = length;
		base = arr_type;
	}

    return base;
}


static Ast **parse_block(int *count)
{
	expect(TOK_LBRACE);

	Ast **stmts = NULL;
	*count = 0;

	// save outer block context
	Ast ***saved_block_stmts = current_block_stmts;
	int *saved_block_count = current_block_count;

	// set up this block context
	current_block_stmts = &stmts;
	current_block_count = count;

	while(current.type != TOK_RBRACE)
	{
		DEBUG_PRINT("DEBUG parse_block: current token = %s\n", token_type_name(current.type));
        if (current.type == TOK_EOF)
		{
            fprintf(stderr, "parser error at line %d: Unexpected EOF in block\n", current.line);
            exit(1);
        }
        Ast *stmt = parse_statement();
        insert_ast(stmt);
	}

	// restore outer block context
	current_block_stmts = saved_block_stmts;
	current_block_count = saved_block_count;

	expect(TOK_RBRACE);
	return stmts;
}

static Ast *parse_struct_decl(void)
{
	expect(TOK_STRUCT);

	if (current.type != TOK_IDENT)
	{
		fprintf(stderr, "parser error at line %d: expected struct name\n", current.line);
		exit(1);
	}

	char *name = strndup(current.start, current.length);
	advance();
	expect(TOK_LBRACE);

	StructField *fields = NULL;
	int field_count = 0;

	while (current.type != TOK_RBRACE)
	{
		Type *field_type = parse_type();
		if (!field_type)
		{
			fprintf(stderr, "parser error at line %d: expected field type\n", current.line);
			exit(1);
		}

		if (current.type != TOK_IDENT)
		{
			fprintf(stderr, "parser error at line %d: expected field name\n", current.line);
			exit(1);
		}

		char *field_name = strndup(current.start, current.length);
		advance();
		expect(TOK_SEMI);

		fields = realloc(fields, sizeof(StructField) * (field_count + 1));
		fields[field_count].name = field_name;
		fields[field_count].type = field_type;
		fields[field_count].offset = 0; // will be filled in by ast checker
		field_count++;
	}

	expect(TOK_RBRACE);

	Ast *s = malloc(sizeof(Ast));
	s->type = AST_STRUCT_DECL;
	s->struct_decl.name = name;
	s->struct_decl.fields = fields;
	s->struct_decl.field_count = field_count;

	Type *t = malloc(sizeof(Type));
	t->kind = TYPE_STRUCT;
	t->struct_type.name = name;
	t->struct_type.fields = fields;
	t->struct_type.field_count = field_count;

	register_struct_type(t);

	return s;
}

Ast *parse_function(void)
{
	expect(TOK_FN);
	if(current.type != TOK_IDENT)
	{
		fprintf(stderr, "parser error at line %d:expected function name \n", current.line);
		exit(1);
	}

	char *name = strndup(current.start, current.length);
	advance();

	expect(TOK_LPAREN);

	int param_count = 0;
	Ast **params = NULL;

	while (current.type != TOK_RPAREN)
	{
		if (param_count > 0 && current.type == TOK_COMMA) advance();
		Type *param_type = parse_type();
		if (!param_type)
		{
			fprintf(stderr, "parser error at line %d: expected parameter type\n", current.line);
			exit(1);
		}

		if (current.type != TOK_IDENT)
		{
			fprintf(stderr, "parser error at line %d: expected parameter name\n", current.line);
			exit(1);
		}

		char *param_name = strndup(current.start, current.length);
		advance();
		Ast *param = malloc(sizeof(Ast));
		param->type = AST_VAR_DECL;
		param->var_decl.name = param_name;
		param->var_decl.var_type = param_type;
		param->var_decl.symbol = NULL;  
		param->var_decl.init_value = NULL;
		params = realloc(params, sizeof(Ast*) * (param_count + 1));
		params[param_count] = param;
		param_count++;
	}

	expect(TOK_RPAREN);

	Type *ret_type = NULL;

	if(current.type == TOK_COLON)
	{
		advance();
		ret_type = parse_type();
		if(!ret_type)
		{
			fprintf(stderr, "parser error at line %d: expected return type after colon\n", current.line);
			exit(1);
		}
	}

	int body_length;
	Ast **body = parse_block(&body_length);

	Ast *f = malloc(sizeof(Ast));
	f->type = AST_FUNCTION;
	f->function.name = name;
	f->function.arguments = params;
	f->function.arg_count = param_count;
	f->function.body = body;
	f->function.body_length = body_length;
	f->function.ret_type = ret_type;
	return f;
}

static Ast *parse_loop(int loopType)
{
	expect(TOK_LPAREN);
	Ast *init = NULL;
	Ast *condition = NULL;
	Ast *update = NULL;
	if(loopType == 0)
	{
		condition = parse_expression();
	}
	else if(loopType == 1)
	{
		init = parse_statement();
		condition = parse_expression();
		expect(TOK_SEMI);
		update = parse_statement_no_semi();
	}
	expect(TOK_RPAREN);
	int body_length;
	Ast **body = parse_block(&body_length);
	Ast *l = malloc(sizeof(Ast));
    l->type = AST_LOOP;
	l->loop.init = init;
    l->loop.cond = condition;
	l->loop.update = update;
    l->loop.body = body;
    l->loop.body_length = body_length;
	return l;
}

static Ast *parse_array_literal(void)
{
	expect(TOK_LSQRBRACE);

	Ast **elements = NULL;
	int count = 0;

	while (current.type != TOK_RSQRBRACE)
	{
		Ast *elem;
        if (current.type == TOK_LSQRBRACE)
            elem = parse_array_literal();
        else
            elem = parse_expression();
		
		elements = realloc(elements, sizeof(Ast*) * (count + 1));
        elements[count++] = elem;

        if (current.type == TOK_COMMA) advance();
        else break;
	}

	expect(TOK_RSQRBRACE);

	if (count == 0) 
	{ 
		fprintf(stderr, "parser error at line %d: cannot infer type of empty array literal\n", current.line); 
		exit(1); 
	}

	Ast *array_lit = malloc(sizeof(Ast));
	array_lit->type = AST_ARRAY_LITERAL;
	array_lit->array_literal.elements = elements;
	array_lit->array_literal.count = count;
	array_lit->type_ptr = NULL;  // Will be filled in by ast checker
	return array_lit;
}
