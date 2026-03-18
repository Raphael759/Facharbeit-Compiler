#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "astcheck.h"
#include "token.h"
#include "debug.h"
#include "type.h"

int main(int argc, char **argv)
{
    // Parse command line arguments
    const char *filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_enabled = true;
        } else if (argv[i][0] != '-') {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "usage: mylang [-d|--debug] <file>\n");
        return 1;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    size_t bytes_read = fread(buf, 1, size, f);
    buf[bytes_read] = 0;
    fclose(f);

    if (debug_enabled) {
        printf("=== Source code (%zu bytes) ===\n", bytes_read);
        for (size_t i = 0; i < bytes_read; i++) {
            if (buf[i] >= 32 && buf[i] <= 126) {
                putchar(buf[i]);
            } else if (buf[i] == '\n') {
                printf("\\n\n");
            } else if (buf[i] == '\t') {
                printf("\\t");
            } else {
                printf("[0x%02x]", (unsigned char)buf[i]);
            }
        }
        printf("\n=== End source ===\n\n");
    }

    lexer_init(buf);
    init_type_table();
    int function_count;
    Ast **program = parse_program(&function_count);

    if (debug_enabled) {
        printf("Parsed %d functions\n\n", function_count);

        for (int i = 0; i < function_count; i++)
        {
            printf("Function has %d statements in body\n", program[i]->function.body_length);
            for (int j = 0; j < program[i]->function.body_length; j++) {
                printf("Statement %d type: %d\n", j, program[i]->function.body[j]->type);
            }
            printf("\n");
        }
    }

    SymbolTable *global_symtab = symtab_new(NULL);

    for (int i = 0; i < function_count; i++)
    {
        if (program[i]->type == AST_STRUCT_DECL)
        {
            register_struct_decl(program[i]);
        }
    }
    for (int i = 0; i < function_count; i++)
    {
        if(program[i]->type == AST_FUNCTION)
            register_function(global_symtab, program[i]);
    }
    for (int i = 0; i < function_count; i++)
    {
		if (program[i]->type == AST_FUNCTION)
        {
            if (debug_enabled)
                print_ast(program[i], 0);
            else
                check_function(global_symtab, program[i]);
        }
    }
    codegen_program(program, function_count);

    return 0;
}