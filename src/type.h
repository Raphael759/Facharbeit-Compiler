#pragma once
typedef enum
{
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_CHAR,
    TYPE_ARRAY,
    TYPE_BITSLICE,
    TYPE_STRUCT,
    TYPE_POINTER
} TypeKind;

typedef struct Type Type;

typedef struct StructField
{
    const char *name;
    Type *type;
    int offset;
} StructField;

struct Type
{
    TypeKind kind;
    int size;   //0 for bitslice
    union
    {
        struct
        {
            Type* element_type;
            int length;
        } array;
        struct 
        {
            const char *name;
            StructField *fields;
            int field_count;
        } struct_type;
        struct
        {
            int width; // in bits
        } bitslice;
        struct
        {
            Type *pointer_type;
        } pointer;
    };
};

typedef struct TypeEntry
{
    char* name;
    Type *type;
} TypeEntry;

void init_type_table(void);
void register_struct_type(Type *t);
Type *lookup_type(const char* name, int length);
Type *lookup_suffix(const char* start, int length);
Type *make_array_type(Type *elem, int length);
Type *make_bitslice_type(int width);
Type *make_pointer_type(Type *pointee);

extern Type TYPE_INT8;
extern Type TYPE_INT16;
extern Type TYPE_INT32;
extern Type TYPE_INT64;
extern Type TYPE_CHARACTER;