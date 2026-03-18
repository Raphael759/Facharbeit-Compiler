#pragma once
#include "ast.h"
#include "type.h"

typedef enum
{
    SYMBOL_VAR,
    SYMBOL_FUNC
} SymbolKind;

typedef struct Symbol
{
    char *name;
    SymbolKind kind;
    Type *type; //return type or var type
    int offset;
    union
    {
        struct
        {
            
        } var;

        struct
        {
            Type **param_types;
            int param_count;
        } func;
    };
} Symbol;

typedef struct SymbolTable
{
    Symbol **symbols;
    int count;
    struct SymbolTable *parent;
} SymbolTable;

SymbolTable *symtab_new(SymbolTable *parent);
void symtab_free(SymbolTable *tab);

Symbol *symtab_lookup(SymbolTable *tab, const char *name);
Symbol *symtab_insert(SymbolTable *tab, const char *name, SymbolKind kind);



