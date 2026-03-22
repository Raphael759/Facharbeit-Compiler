#include "type.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_TYPES 128
static TypeEntry type_table[MAX_TYPES];
static int type_count = 0;

Type TYPE_INT8 = 
{
    .kind  = TYPE_I8,
    .size  = 1
};
Type TYPE_INT16 = 
{
    .kind  = TYPE_I16,
    .size  = 2
};
Type TYPE_INT32 = 
{
    .kind  = TYPE_I32,
    .size  = 4
};
Type TYPE_INT64 = 
{
    .kind  = TYPE_I64,
    .size  = 8
};
Type TYPE_CHARACTER = 
{
    .kind  = TYPE_CHAR,
    .size  = 1
};

static void add_type(const char* name, Type *type)
{
    if(type_count >= MAX_TYPES)
    {
        fprintf(stderr, "type table full\n");
        exit(1);
    }
    type_table[type_count].name = strdup(name);
    type_table[type_count].type = type;
    type_count++;
}

void init_type_table(void)
{
    add_type("int8", &TYPE_INT8);
    add_type("int16", &TYPE_INT16);
    add_type("int32", &TYPE_INT32);
    add_type("int64", &TYPE_INT64);
    add_type("char", &TYPE_CHARACTER);

    add_type("byte", &TYPE_INT8);
    add_type("int", &TYPE_INT64);
}

void register_struct_type(Type *t)
{
    if (!t || t->kind != TYPE_STRUCT)
    {
        fprintf(stderr, "type error: register_struct_type called with non-struct\n");
        exit(1);
    }

    for (int i = 0; i < type_count; i++)
    {
        if (strcmp(type_table[i].name, t->struct_type.name) == 0)
        {
            fprintf(stderr, "redefinition of struct '%s'\n", t->struct_type.name);
            exit(1);
        }
    }

    add_type(t->struct_type.name, t);
}

Type *lookup_type(const char *name, int length)
{
    for (int i = 0; i < type_count; i++)
    {
        if ((int)strlen(type_table[i].name) == length && strncmp(type_table[i].name, name, length) == 0)
        {
            return type_table[i].type;
        }
    }
    return NULL;
}

Type *lookup_suffix(const char* start, int length)
{
    if (!memcmp(start, "i8", length))  return &TYPE_INT8;
    if (!memcmp(start, "i16", length))  return &TYPE_INT16;
    if (!memcmp(start, "i32", length))  return &TYPE_INT32;
    if (!memcmp(start, "i64", length))  return &TYPE_INT64;
    if (!memcmp(start, "ch", length))   return &TYPE_CHARACTER;

    return NULL;
}

Type *make_array_type(Type *elem, int length)
{
    Type *t = malloc(sizeof(Type));
    t->kind = TYPE_ARRAY;
    t->size = elem->size * length;
    t->array.element_type = elem;
    t->array.length = length;
    return t;
}

Type *make_bitslice_type(int width)
{
    Type *t = malloc(sizeof(Type));
    t->kind = TYPE_BITSLICE;
    t->size = 0;                 //not memory storable
    t->bitslice.width = width;
    return t;
}

Type *make_pointer_type(Type *base)
{
    Type *t = malloc(sizeof(Type));
    t->kind = TYPE_POINTER;
    t->size = 8;
    t->pointer.pointer_type = base;
    return t;
}