#pragma once
void register_struct_decl(Ast *node);
void check_function(SymbolTable *global_symtab, Ast *fn);
void register_function(SymbolTable *global_symtab, Ast *fn);