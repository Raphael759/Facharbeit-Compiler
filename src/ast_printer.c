#include <stdio.h>
#include "ast.h"

void print_ast(Ast *node, int indent) 
{
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("    "); 

    switch(node->type) {
        case AST_NUMBER:
            printf("Number: %lld\n", node->number);
            break;

        case AST_BINARY:
            printf("Binary: ");
            switch(node->binary.op) {
                case OP_ADD: printf("+\n"); break;
                case OP_SUB: printf("-\n"); break;
                case OP_MUL: printf("*\n"); break;
                case OP_DIV: printf("/\n"); break;
                case OP_MOD: printf("%%\n"); break;
                case OP_LT:  printf("<\n"); break;
                case OP_LE:  printf("<=\n"); break;
                case OP_GT:  printf(">\n"); break;
                case OP_GE:  printf(">=\n"); break;
                case OP_IE:  printf("==\n"); break;
                case OP_NE:  printf("!=\n"); break;
                default:     printf("?\n"); break;
            }
            print_ast(node->binary.left, indent + 1);
            print_ast(node->binary.right, indent + 1);
            break;

        case AST_RETURN:
            printf("Return\n");
            print_ast(node->ret.expr, indent + 1);
            break;

        case AST_FUNCTION:
            printf("Function: %s\n", node->function.name);
            for (int i = 0; i < node->function.body_length; i++) {
                print_ast(node->function.body[i], indent + 1);
            }
            break;

        case AST_FUNCTION_CALL:
            printf("Function Call: %s\n", node->function_call.name);
            break;

        case AST_LOOP:
            printf("Loop\n");
            if (node->loop.init) {
                for (int i = 0; i < indent + 1; i++) printf("    ");
                printf("Init:\n");
                print_ast(node->loop.init, indent + 2);
            }
            if (node->loop.cond) {
                for (int i = 0; i < indent + 1; i++) printf("    ");
                printf("Condition:\n");
                print_ast(node->loop.cond, indent + 2);
            }
            if (node->loop.update) {
                for (int i = 0; i < indent + 1; i++) printf("    ");
                printf("Update:\n");
                print_ast(node->loop.update, indent + 2);
            }
            for (int i = 0; i < indent + 1; i++) printf("    ");
            printf("Body:\n");
            for (int i = 0; i < node->loop.body_length; i++) {
                print_ast(node->loop.body[i], indent + 2);
            }
            break;

        case AST_VAR_DECL:
            /*printf("VarDecl: %s", node->var_decl.name);
            if (node->var_decl.var_type) {
                switch (node->var_decl.var_type->kind) {
                    case TYPE_I32: printf(" (int32)"); break;

                    default: printf(" (unknown type)"); break;
                }
            }
            if (node->var_decl.symbol) {
                printf(" [offset: %d]", node->var_decl.symbol->offset);
            }
            printf("\n");*/
            break;

        case AST_VAR:
            

        case AST_VAR_ASSIGN:
            /*
            printf("Assign: %s", node->var_assign.name);
            if (node->var_assign.symbol) {
                printf(" [offset: %d]", node->var_assign.symbol->offset);
            }
            printf("\n");
            print_ast(node->var_assign.value, indent + 1);*/
            break;

        case AST_PRINT:
            printf("Print\n");
            print_ast(node->print.expr, indent + 1);
            break;

        case AST_IF:
            printf("If\n");
            for (int i = 0; i < indent + 1; i++) printf("    ");
            printf("Condition:\n");
            print_ast(node->if_stmt.cond, indent + 2);

            for (int i = 0; i < indent + 1; i++) printf("    ");
            printf("Body:\n");
            for (int i = 0; i < node->if_stmt.body_length; i++) {
                print_ast(node->if_stmt.body[i], indent + 2);
            }
            break;

        default:
            printf("Unknown AST node\n");
            break;
    }
}