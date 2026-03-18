#pragma once
#include "type.h"
#include "symbol.h"

typedef struct Symbol Symbol;

typedef enum
{
	AST_NUMBER,
	AST_BINARY,
	AST_UNARY,
	AST_RETURN,
	AST_FUNCTION,
	AST_FUNCTION_CALL,
	AST_LOOP,
	AST_IF,
	AST_ELSE,
	AST_VAR_DECL,
	AST_VAR_ASSIGN,
	AST_VAR,
	AST_ARRAY_INDEX,
	AST_ARRAY_LITERAL,
	AST_BIT_ACCESS,
	AST_CAST,		
	AST_BITCAST, 
	AST_PRINT,
	AST_STRUCT_DECL,
	AST_MEMBER_ACCESS
} AstType;

typedef enum
{
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,

	OP_LT,
	OP_LE,
	OP_GT,
	OP_GE,
	OP_IE,
	OP_NE,

	OP_AND,
	OP_OR,
	OP_XOR,
	
	OP_BIT_AND,
	OP_BIT_OR,
	OP_BIT_XOR,
	OP_BIT_SHL,       
	OP_BIT_SHR

} BinOp;

typedef enum 
{
	UN_NEG,
	UN_NOT,   
	UN_BIT_NOT,
	UN_DEREF,	//dereference *
	UN_ADDR //address of var &
} UnOp;

typedef struct Ast Ast;

struct Ast
{
	AstType type;
	Type *type_ptr;
	union
	{
		long long number;
		struct
		{
			BinOp op; //+ - * / % < <= > >= == !=
			Ast *left;
			Ast *right;
		} binary;

		struct
		{
			UnOp op; // ! ~
			Ast *expr;
		} unary;

		struct 
		{
			Ast *expr;
		} ret;

		struct
		{
			const char *name;
			Ast **arguments;
			int arg_count;
			Type *ret_type;
			Ast **body;
			int body_length;
			int stack_size; // filled by astcheck
		} function;

		struct
		{
			const char *name;
			Symbol *symbol; // filled by astcheck
			Ast **arguments;
			int arg_count;
		} function_call;

		struct 
		{
			Ast *cond;
			Ast **body;
			int body_length;
			Ast *else_stmt;
		}   if_stmt;

		struct
		{
			Ast **body;
			int body_length;
		} else_stmt;

		struct
		{
			Ast *init;        //only for
			Ast *cond;
			Ast *update;   //only for
			Ast **body;
			int body_length;
		} loop;

		struct
		{
			const char *name;
			Type *var_type;
			Symbol *symbol;
			Ast *init_value; //null when no init
		} var_decl;

		struct 
		{
			const char *name;
			StructField *fields;
			int field_count;
		} struct_decl;

		struct
		{
			Ast *base;
			const char *member_name;
			int member_offset; // filled by astcheck
		} member_access;

		struct
		{
			const char *name;
			Symbol *symbol;// filled by astcheck
		} var;

		struct
		{
			Ast **elements;
			int count;
		} array_literal;

		struct
		{
			Ast *array;
			Ast *index;
		} array_index;

		struct
		{
			Ast *target;
			Ast *value;
		} var_assign;

		struct 
		{
			Ast *base;
			Ast *start;
			Ast *end;
		} bit_access;

		struct
		{
			Ast *expr;
			Type *target_type;
		} cast;

		struct
		{
			Ast *expr;
		} print;
	};
};

void print_ast(Ast *node, int indent);
