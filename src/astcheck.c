#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "ast.h"
#include "type.h"

static void check_stmt(Ast *node, SymbolTable *tab, int *current_offset, Type *expected_return);
static int type_equals(Type *a, Type *b);

static void check_expr(Ast *node, SymbolTable *tab)
{
    switch (node->type)
    {
        case AST_NUMBER:
            if(!node->type_ptr)
                node->type_ptr = &TYPE_INT64;
            return;

        case AST_VAR:
            Symbol *s = symtab_lookup(tab, node->var.name);
            if (!s)
            {
                fprintf(stderr, "use of undeclared variable '%s'\n", node->var.name);
                exit(1);
            }
            node->var.symbol = s;
            node->type_ptr = s->type;
            return;

        case AST_UNARY:
            check_expr(node->unary.expr, tab);
            if (node->unary.op == UN_NEG)
            {
                node->type_ptr = node->unary.expr->type_ptr;
            }
            else if (node->unary.op == UN_NOT)
            {
                node->type_ptr = &TYPE_INT64;   // logical NOT produces int64
            }
            else if (node->unary.op == UN_BIT_NOT)
            {
                node->type_ptr = node->unary.expr->type_ptr;
            }
            else if (node->unary.op == UN_DEREF)
            {
                Type *t = node->unary.expr->type_ptr;
                if (!t || t->kind != TYPE_POINTER)
                {
                    fprintf(stderr, "cannot dereference non-pointer expression\n");
                    exit(1);
                }
                node->type_ptr = t->pointer.pointer_type;
            }
            else if (node->unary.op == UN_ADDR)
            {
                Ast *base = node->unary.expr;
                if (base->type != AST_VAR && base->type != AST_MEMBER_ACCESS && base->type != AST_ARRAY_INDEX)
                {
                    fprintf(stderr, "can only take address of variable, struct member, or array element\n");
                    exit(1);
                }
                node->type_ptr = make_pointer_type(base->type_ptr);
            }
            else
            {
                fprintf(stderr, "unknown unary operator\n");
                exit(1);
            }
            return;

        case AST_BINARY:
            check_expr(node->binary.left, tab);
            check_expr(node->binary.right, tab);
            
            
            Type *left  = node->binary.left->type_ptr;
            Type *right = node->binary.right->type_ptr;

            if (!left || !right)
            {
                fprintf(stderr, "invalid operands\n");
                exit(1);
            }

            switch (node->binary.op)
            {
                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV:
                case OP_MOD:
                    if (!type_equals(left, right))
                    {
                        fprintf(stderr, "invalid operands to binary arithmetic operator\n");
                        exit(1);
                    }
                    node->type_ptr = left;
                    return;

                case OP_IE:
                case OP_NE:
                case OP_LT:
                case OP_LE:
                case OP_GT:
                case OP_GE:
                    if (!type_equals(left, right))
                    {
                        fprintf(stderr, "type mismatch in comparison\n");
                        exit(1);
                    }
                    node->type_ptr = &TYPE_INT64;   // comparisons always produce int64 
                    return;

                case OP_AND:
                case OP_OR:
                case OP_XOR:
                    if (!type_equals(left, &TYPE_INT64) || !type_equals(right, &TYPE_INT64))
                    {
                        fprintf(stderr, "logical operators require 64-bit integers\n");
                        exit(1);
                    }
                    node->type_ptr = &TYPE_INT64;   // logical ops produce int64
                    return;

                default:
                    fprintf(stderr, "unknown binary operator\n");
                    exit(1);
            }

            break;
            
        case AST_ARRAY_INDEX:
            check_expr(node->array_index.array, tab);
            check_expr(node->array_index.index, tab);

            if(node->array_index.array->type_ptr->kind != TYPE_ARRAY)
            {
                fprintf(stderr, "attempting to index non-array expression\n");
                exit(1);
            }
            if (node->array_index.index->type_ptr != &TYPE_INT64) 
            {
                fprintf(stderr, "array index must be a 64-bit integer\n");
                exit(1);
            }
            node->type_ptr = node->array_index.array->type_ptr->array.element_type;
            return;

        case AST_ARRAY_LITERAL:
            if(node->array_literal.count == 0)
            {
                fprintf(stderr, "cannot infer type of empty array literal\n");
                exit(1);
            }
            for(int i = 0; i < node->array_literal.count; i++)
                check_expr(node->array_literal.elements[i], tab);

            Type *elem_type = node->array_literal.elements[0]->type_ptr;
            if(!elem_type)
            {
                fprintf(stderr, "cannot infer type of array elements\n");
                exit(1);
            }
            for(int i = 1; i < node->array_literal.count; i++)
            {
                if(node->array_literal.elements[i]->type_ptr != elem_type)
                {
                    fprintf(stderr, "array literal elements have mismatched types\n");
                    exit(1);
                }
            }
            Type *arr_type = malloc(sizeof(Type));
            arr_type->kind = TYPE_ARRAY;
            arr_type->size = elem_type->size * node->array_literal.count;
            arr_type->array.element_type = elem_type;
            arr_type->array.length = node->array_literal.count;

            node->type_ptr = arr_type;
            return;

        case AST_MEMBER_ACCESS:
            check_expr(node->member_access.base, tab);

            Type *struct_type = node->member_access.base->type_ptr;

            if (!struct_type || struct_type->kind != TYPE_STRUCT)
            {
                fprintf(stderr, "attempting to access member of non-struct expression\n");
                exit(1);
            }
            
            StructField *fields = struct_type->struct_type.fields;
            int count = struct_type->struct_type.field_count;
            for (int i = 0; i < count; i++)
            {
                if (strcmp(fields[i].name, node->member_access.member_name) == 0)
                {
                    node->member_access.member_offset = fields[i].offset;
                    node->type_ptr = fields[i].type;
                    return;
                }
            }

            fprintf(stderr, "struct '%s' has no member named '%s'\n", struct_type->struct_type.name, node->member_access.member_name);
            exit(1);

        case AST_BIT_ACCESS:
            check_expr(node->bit_access.base, tab);
            check_expr(node->bit_access.start, tab);

            if (node->bit_access.end)
                check_expr(node->bit_access.end, tab);

            Type *base = node->bit_access.base->type_ptr;
            if (!base || base->size == 0)
            {
                fprintf(stderr, "invalid bit access base\n");
                exit(1);
            }
            if (!type_equals(node->bit_access.start->type_ptr, &TYPE_INT64))
            {
                fprintf(stderr, "bit access start index must be a 64-bit integer\n");
                exit(1);
            }
            if (node->bit_access.end && !type_equals(node->bit_access.end->type_ptr, &TYPE_INT64))
            {
                fprintf(stderr, "bit slice end index must be 64-bit integer\n");
                exit(1);
            }
            if (node->bit_access.start->type != AST_NUMBER)
            {
                fprintf(stderr, "bit access index must be number literal\n");
                exit(1);
            }
            if (node->bit_access.end && node->bit_access.end->type != AST_NUMBER)
            {
                fprintf(stderr, "bit slice end index must be number literal\n");
                exit(1);
            }
            if (base->kind == TYPE_BITSLICE)
            {
                fprintf(stderr, "cannot apply bit access to bitslice\n");
                exit(1);
            }
            int start = node->bit_access.start->number;
            int end = node->bit_access.end ? node->bit_access.end->number : start;
            if (start < 0 || start >= base->size * 8 || end < 0 || end >= base->size * 8 || start > end)
            {
                fprintf(stderr, "bit access index out of bounds\n");
                exit(1);
            }
            int width = end - start + 1;
            node->type_ptr = make_bitslice_type(width);
            return;

        case AST_CAST:
        case AST_BITCAST:
            check_expr(node->cast.expr, tab);

            Type *src_type = node->cast.expr->type_ptr;
            Type *target_type = node->cast.target_type;
            if (!src_type || !target_type)
            {
                fprintf(stderr, "invalid cast expression\n");
                exit(1);
            }
            if (node->type == AST_BITCAST)
            {
                if (src_type->size != target_type->size)
                {
                    fprintf(stderr, "bitcast requires source and target types to have the same size\n");
                    exit(1);
                }
            }

            node->type_ptr = target_type;
            return;

        case AST_FUNCTION_CALL:
            Symbol *sy = symtab_lookup(tab, node->function_call.name);
            if (!sy || sy->kind != SYMBOL_FUNC) 
            {
                fprintf(stderr, "call to undeclared function '%s'\n", node->function_call.name);
                exit(1);
            }
            node->function_call.symbol = sy;
            node->type_ptr = sy->type;
            if(node->function_call.arg_count != sy->func.param_count)
            {
                fprintf(stderr, "argument count mismatch in call to function '%s'\n", node->function_call.name);
                exit(1);
            }
            for(int i = 0; i < node->function_call.arg_count; i++)
            {
                check_expr(node->function_call.arguments[i], tab);
                if(!type_equals(node->function_call.arguments[i]->type_ptr, sy->func.param_types[i]))
                {
                    fprintf(stderr, "argument type mismatch in call to function '%s'\n", node->function_call.name);
                    exit(1);
                }
            }
            return;

        default:
            return;
    }
}

static void check_stmt(Ast *node, SymbolTable *tab, int *current_offset, Type *expected_return)
{
    Symbol *s = NULL;

    switch (node->type)
    {
        case AST_VAR_DECL:
            s = symtab_insert(tab, node->var_decl.name, SYMBOL_VAR);
            if (!s)
            {
                fprintf(stderr, "redefinition of '%s'\n", node->var_decl.name);
                exit(1);
            }

            s->type = node->var_decl.var_type;
            *current_offset -= s->type->size;
            s->offset = *current_offset;

            node->var_decl.symbol = s;

            if (node->var_decl.init_value)
            {
                check_expr(node->var_decl.init_value, tab);
                if (!type_equals(s->type, node->var_decl.init_value->type_ptr))
                {
                    fprintf(stderr, "type mismatch in initialization of variable '%s'\n", node->var_decl.name);
                    exit(1);
                }
            }
            return;

        case AST_VAR_ASSIGN:
        {
            check_expr(node->var_assign.value, tab);

            Ast *target = node->var_assign.target;

            if (target->type == AST_VAR)
            {
                Symbol *s = symtab_lookup(tab, target->var.name);
                if (!s)
                {
                    fprintf(stderr, "assignment to undeclared variable '%s'\n",
                            target->var.name);
                    exit(1);
                }

                target->var.symbol = s;
                target->type_ptr = s->type;

                if (!type_equals(s->type, node->var_assign.value->type_ptr))
                {
                    fprintf(stderr, "type mismatch assigning to variable '%s'\n",
                            target->var.name);
                    exit(1);
                }
            }
            else if (target->type == AST_ARRAY_INDEX || target->type == AST_BIT_ACCESS || target->type == AST_MEMBER_ACCESS)
            {
                check_expr(target, tab);

                Type *expected = target->type_ptr;
                Type *actual   = node->var_assign.value->type_ptr;

                if (!type_equals(expected, actual))
                {
                    fprintf(stderr, "type mismatch in indexed/bit assignment\n");
                    exit(1);
                }
            }
            else if (target->type == AST_UNARY && target->unary.op == UN_DEREF)
            {
                check_expr(target, tab);

                if (!type_equals(target->type_ptr, node->var_assign.value->type_ptr))
                {
                    fprintf(stderr, "type mismatch in dereference assignment\n");
                    exit(1);
                }
            }
            else
            {
                fprintf(stderr, "invalid assignment target\n");
                exit(1);
            }

            return;
        }


        


        case AST_RETURN:
            check_expr(node->ret.expr, tab);
            if (!type_equals(node->ret.expr->type_ptr, expected_return))
            {
                fprintf(stderr, "return type mismatch\n");
                exit(1);
            }
            return;

		case AST_PRINT:
			check_expr(node->print.expr, tab);
			return;

		case AST_LOOP:
			SymbolTable *loop_tab = symtab_new(tab);
			int loop_offset = *current_offset;

			if (node->loop.init)
        		check_stmt(node->loop.init, loop_tab, &loop_offset, expected_return);

			if (node->loop.cond)
        		check_expr(node->loop.cond, loop_tab);

			for (int i = 0; i < node->loop.body_length; i++)
        		check_stmt(node->loop.body[i], loop_tab, &loop_offset, expected_return);

			if (node->loop.update)
        		check_stmt(node->loop.update, loop_tab, &loop_offset, expected_return);

            *current_offset = loop_offset;
			symtab_free(loop_tab);
			return;
        
        case AST_IF:
            check_expr(node->if_stmt.cond, tab);

            SymbolTable *if_tab = symtab_new(tab);
            int if_offset = *current_offset;

            for (int i = 0; i < node->if_stmt.body_length; i++)
                check_stmt(node->if_stmt.body[i], if_tab, &if_offset, expected_return);
            *current_offset = if_offset;
            symtab_free(if_tab);


            if(node->if_stmt.else_stmt)
            {
                Ast *temp = node->if_stmt.else_stmt;
                SymbolTable *else_tab = symtab_new(tab);
                int else_offset = *current_offset;
                for(int i =  0; i < temp->else_stmt.body_length; i++)
                    check_stmt(temp->else_stmt.body[i], else_tab, &else_offset, expected_return);
                *current_offset = else_offset;
                symtab_free(else_tab);
            }
            return;
        case AST_FUNCTION_CALL:
            Symbol *sy = symtab_lookup(tab, node->function_call.name);
            if (!sy || sy->kind != SYMBOL_FUNC) 
            {
                fprintf(stderr, "call to undeclared function '%s'\n", node->function_call.name);
                exit(1);
            }
            node->function_call.symbol = sy;
            node->type_ptr = sy->type;
            if(node->function_call.arg_count != sy->func.param_count)
            {
                fprintf(stderr, "argument count mismatch in call to function '%s'\n", node->function_call.name);
                exit(1);
            }
            for(int i = 0; i < node->function_call.arg_count; i++)
            {
                check_expr(node->function_call.arguments[i], tab);
                if(!type_equals(node->function_call.arguments[i]->type_ptr, sy->func.param_types[i]))
                {
                    fprintf(stderr, "argument type mismatch in call to function '%s'\n", node->function_call.name);
                    exit(1);
                }
            }
            return;

        default:
            return;
    }
}

void register_struct_decl(Ast *node)
{
    Type *t = lookup_type(node->struct_decl.name, strlen(node->struct_decl.name));
    if (!t)
    {
        fprintf(stderr, "internal error: struct '%s' not found in type table\n", node->struct_decl.name);
        exit(1);
    }

    if (t->kind != TYPE_STRUCT)
    {
        fprintf(stderr, "internal error: type '%s' is not a struct\n", node->struct_decl.name);
        exit(1);
    }
    int offset = 0;
    for(int i = 0; i< node->struct_decl.field_count; i++)
    {
        StructField *f = &t->struct_type.fields[i];
        if (strcmp(f->name, node->struct_decl.fields[i].name) != 0 || f->type != node->struct_decl.fields[i].type)
        {
            fprintf(stderr, "internal error: struct field mismatch for struct '%s'\n", node->struct_decl.name);
            exit(1);
        }
        f->offset = offset;
        offset += f->type->size;
    }
    t->size = offset;
}

void check_function(SymbolTable *glob_tab, Ast *fn)
{
    SymbolTable *local_tab = symtab_new(glob_tab);

    //parameters
    int param_offset = 16;  //rbp+0 is old rbp, rbp+8 is return address -> parameters start at rbp+16+returnsize
    if(fn->function.ret_type)
        param_offset += fn->function.ret_type->size;
    for(int i = 0; i < fn->function.arg_count; i++)
    {
        Ast *param = fn->function.arguments[i];
        Symbol *s = symtab_insert(local_tab, param->var_decl.name, SYMBOL_VAR);
        if (!s)
        {
            fprintf(stderr, "redefinition of parameter '%s'\n", param->var_decl.name);
            exit(1);
        }
        s->type = param->var_decl.var_type;
        s->offset = param_offset;
        param_offset += s->type->size;
        param->var_decl.symbol = s;
    }

    int current_offset = 0;

    for (int i = 0; i < fn->function.body_length; i++)
        check_stmt(fn->function.body[i], local_tab, &current_offset, fn->function.ret_type);
    symtab_free(local_tab);

    fn->function.stack_size = -current_offset;
}

static int type_equals(Type *a, Type *b)
{
    if (a == b) return 1; 
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->kind == TYPE_BITSLICE)   return a->bitslice.width == b->bitslice.width;
    if (a->kind == TYPE_STRUCT) return a == b;
    if (a->kind == TYPE_POINTER)  return type_equals(a->pointer.pointer_type, b->pointer.pointer_type);
    if (a->kind != TYPE_ARRAY) return 1; 
    
    return a->array.length == b->array.length && type_equals(a->array.element_type, b->array.element_type);
}

void register_function(SymbolTable *global_symtab, Ast *fn)
{
    Symbol *s = symtab_insert(global_symtab, fn->function.name, SYMBOL_FUNC);
    if (!s)
    {
        fprintf(stderr, "redefinition of function '%s'\n", fn->function.name);
        exit(1);
    }
    s->type = fn->function.ret_type;
    s->func.param_count = fn->function.arg_count;
    s->func.param_types = malloc(sizeof(Type*) * fn->function.arg_count);
    for (int i = 0; i < fn->function.arg_count; i++)
    {
        if (fn->function.arguments[i]->type != AST_VAR_DECL)
        {
            fprintf(stderr, "invalid function parameter\n");
            exit(1);
        }
        s->func.param_types[i] = fn->function.arguments[i]->var_decl.var_type;
    }
}
