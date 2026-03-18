#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "symbol.h"

SymbolTable *symtab_new(SymbolTable *parent)
{
    SymbolTable *tab = malloc(sizeof(SymbolTable));
    tab->symbols = NULL;
    tab->count = 0;
    tab->parent = parent;
    return tab;
}

void symtab_free(SymbolTable *tab)
{
    if (!tab) return;
    for (int i = 0; i < tab->count; i++)
        free(tab->symbols[i]->name);
    free(tab->symbols);
    free(tab);
}

Symbol *symtab_lookup(SymbolTable *tab, const char *name)
{
    for (int i = 0; i < tab->count; i++)
    {
        if (strcmp(tab->symbols[i]->name, name) == 0)
            return tab->symbols[i];
    }
    if (tab->parent)
        return symtab_lookup(tab->parent, name);
    return NULL;
}

Symbol *symtab_insert(SymbolTable *tab, const char *name, SymbolKind kind)
{
    if (symtab_lookup(tab, name))
        return NULL; // already exists

    Symbol *s = malloc(sizeof(Symbol));
    s->kind = kind;
    s->name = strdup(name);
    s->type = NULL;
    s->offset = 0;

    tab->symbols = realloc(tab->symbols, sizeof(Symbol*) * (tab->count + 1));
    tab->symbols[tab->count++] = s;
    return s;
}
