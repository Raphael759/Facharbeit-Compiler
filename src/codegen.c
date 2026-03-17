#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "ast.h"
#include "symbol.h"

static void gen_function(Ast *f);
static void gen_block(Ast **stmts, int count);
static void gen_stmt(Ast *s);
static void gen_expr(Ast *e);
static void gen_print_runtime(void);
static void gen_array_literal(Ast *lit, int base_offset);
static void gen_lvalue(Ast *node);
static void gen_function_call(Ast *node);
static void gen_memcpy(int size);
static void gen_load_from_rbp(int size, int offset);
static void gen_load_from_addr(int size);
static void gen_store_to_rbp(int size, int offset);
static void gen_store_to_mem(int size);
static void gen_store_to_rsp(int size, int offset);
static int loop_count = 0;
static int if_count = 0;
static int function_counter = 0;
static int current_function = -1;
static int current_return_size = 0;

void codegen_program(Ast **program, int program_length)
{
	gen_print_runtime();
	for (int i = 0; i < program_length; i++)
	{
		if(program[i]->type == AST_FUNCTION)
		{
			gen_function(program[i]);
		}
	}
}

static void gen_function(Ast *f)
{
		current_function = function_counter++;
		current_return_size = f->function.ret_type ? f->function.ret_type->size : 0;

        printf(".globl %s\n", f->function.name);
        printf("%s:\n", f->function.name);

        printf("  pushq %%rbp\n");          
        printf("  movq %%rsp, %%rbp\n");

		if(f->function.stack_size > 0)
			printf("  sub $%d, %%rsp\n", f->function.stack_size);

        gen_block(f->function.body, f->function.body_length);

		printf(".Lreturn_%d:\n", current_function);
        printf("  movq %%rbp, %%rsp\n");    
        printf("  popq %%rbp\n");           
        printf("  ret\n");
		current_function = -1;
}

static void gen_loop(Ast *l)
{
	if (l->loop.init)
        gen_stmt(l->loop.init);

	printf(".L%d_check:\n", loop_count);
	if (l->loop.cond)
    {
        gen_expr(l->loop.cond);
        printf("  cmpq $0, %%rax\n");
        printf("  je .L%d_end\n", loop_count);
    }
	gen_block(l->loop.body, l->loop.body_length);
	if (l->loop.update)
        gen_stmt(l->loop.update);
	printf("  jmp .L%d_check\n",loop_count);


	printf(".L%d_end:\n", loop_count);

	loop_count++;
}

static void gen_if(Ast *i)
{
	int if_count_local = if_count;
	if_count++;
	printf(".If%d_check:\n", if_count_local);
	gen_expr(i->if_stmt.cond);
	printf("  cmpq $0, %%rax\n");
	if(i->if_stmt.else_stmt)
		printf("  je .else%d\n", if_count_local);
	else
		printf("  je .If%d_end\n", if_count_local);
	gen_block(i->if_stmt.body, i->if_stmt.body_length);
	if(i->if_stmt.else_stmt)
		printf("  jmp .If%d_end\n", if_count_local);
	if(i->if_stmt.else_stmt)
	{
		printf(".else%d:\n", if_count_local);
		gen_block(i->if_stmt.else_stmt->else_stmt.body, i->if_stmt.else_stmt->else_stmt.body_length);
	}
	printf(".If%d_end:\n", if_count_local);
}

static void gen_block(Ast **stmts, int count)
{
	for (int i = 0; i < count; i++)
	{
		gen_stmt(stmts[i]);
	}
}

static void gen_stmt(Ast *s)
{
    switch (s->type)
    {
        case AST_RETURN:
            gen_expr(s->ret.expr);
			if (current_return_size > 0)
			{
				printf("  lea 16(%%rbp), %%rdx\n");  // return space
				gen_store_to_mem(current_return_size);
			}

			printf("  jmp .Lreturn_%d\n", current_function);
            break;

		case AST_PRINT:
			gen_expr(s->print.expr);
			if(s->print.expr->type_ptr->kind == TYPE_CHAR)
				printf("  call print_char\n");
			else
				printf("  call print_int\n");

			break;
		case AST_VAR_DECL:
			if (s->var_decl.init_value)
			{
				if (s->var_decl.init_value->type_ptr->kind == TYPE_ARRAY)
				{
					gen_array_literal(s->var_decl.init_value, s->var_decl.symbol->offset);
				}
				else
				{
					gen_expr(s->var_decl.init_value);
					gen_store_to_rbp(s->var_decl.symbol->type->size, s->var_decl.symbol->offset);
				}
			}
			break;
    	case AST_VAR_ASSIGN:
		{
			Ast *target = s->var_assign.target;
			Ast *value = s->var_assign.value;

			if (target->type == AST_BIT_ACCESS)
			{
				Ast *base  = target->bit_access.base;

				int start  = target->bit_access.start->number;
				int width  = target->type_ptr->bitslice.width;

				unsigned long long mask = ((1ULL << width) - 1ULL) << start;

				gen_lvalue(base);          
				printf("  movq %%rax, %%rdx\n");	//address to base in rdx

				int size = base->type_ptr->size;
				
				if (size == 1)
					printf("  movzbq (%%rdx), %%rbx\n");
				else if (size == 2)
					printf("  movzwq (%%rdx), %%rbx\n");
				else if (size == 4)
					printf("  movl (%%rdx), %%ebx\n");
				else if (size == 8)
					printf("  movq (%%rdx), %%rbx\n");		//base value in rbx

				
				gen_expr(value);           // to be assigned in rax

				if (width < 64)
				{
					unsigned long long small =	(1ULL << width) - 1ULL;
					printf("  andq $%llu, %%rax\n", small);	//idfk
				}

				if (start != 0)
					printf("  shlq $%d, %%rax\n", start);	//shift bits of the value, so that lowest is start

				printf("  movq %%rbx, %%rcx\n");
				printf("  andq $%llu, %%rcx\n", ~mask);	//clear slice in target

				printf("  orq %%rcx, %%rax\n");

				if (size == 1)
					printf("  movb %%al, (%%rdx)\n");
				else if (size == 2)
					printf("  movw %%ax, (%%rdx)\n");
				else if (size == 4)
					printf("  movl %%eax, (%%rdx)\n");
				else if (size == 8)
					printf("  movq %%rax, (%%rdx)\n");

				break;
			}
			gen_expr(value);
			printf("  movq %%rax, %%r8\n");// value (or address for large types) in r8
			gen_lvalue(target);     
			printf("  movq %%rax, %%rdx\n");	// address to target in rdx    
			printf("  movq %%r8, %%rax\n");		//restore value
			int size = target->type_ptr->size;
			gen_store_to_mem(size);

    		break;
		}
		case AST_LOOP:
			gen_loop(s);
			break;
		case AST_IF:
			gen_if(s);
			break;
		case AST_FUNCTION_CALL:
			gen_function_call(s);
			break;
        default:
                break;
    }
}

static void gen_expr(Ast *e)
{
     switch (e->type)
     {
        case AST_NUMBER:
                printf("  movq $%lld, %%rax\n", e->number);  
                break;
		case AST_VAR:
     	{
			if (!e->var.symbol)
			{
				fprintf(stderr, "codegen error: variable has no symbol\n");
				exit(1);
			}
			if(e->type_ptr->kind == TYPE_ARRAY)
			{
				printf("  leaq %d(%%rbp), %%rax\n", e->var.symbol->offset);
				break;
			}
			if(e->type_ptr->size <= 8 && e->type_ptr->size > 0)
			{
				gen_load_from_rbp(e->type_ptr->size, e->var.symbol->offset);
			}
			else 
			{
				printf("  leaq %d(%%rbp), %%rax\n", e->var.symbol->offset);
			}
			break;
     	}
		case AST_UNARY:
			if (e->unary.op == UN_ADDR)
			{
				gen_lvalue(e->unary.expr);   // result: pointer in %rax
				break;
			}
			gen_expr(e->unary.expr);
			switch (e->unary.op)
			{
				case UN_NEG:
					if(e->unary.expr->type_ptr->size == 1) printf("  negb %%al\n");
                    else if(e->unary.expr->type_ptr->size == 2) printf("  negw %%ax\n");
                    else if(e->unary.expr->type_ptr->size == 4) printf("  negl %%eax\n");
                    else if(e->unary.expr->type_ptr->size == 8) printf("  negq %%rax\n");
                    break;
				case UN_NOT:	//0 -> 1, else -> 0
					printf("  cmp $0, %%rax\n");
                    printf("  sete %%al\n");
                    printf("  movzbq %%al, %%rax\n"); 
                    break;
				case UN_BIT_NOT:
					if(e->unary.expr->type_ptr->size == 1) printf("  notb %%al\n");
                    else if(e->unary.expr->type_ptr->size == 2) printf("  notw %%ax\n");
                    else if(e->unary.expr->type_ptr->size == 4) printf("  notl %%eax\n");
                    else if(e->unary.expr->type_ptr->size == 8) printf("  notq %%rax\n");
                    break;
				case UN_DEREF:
					gen_load_from_addr(e->type_ptr->size);
					break;
					
				default:
					fprintf(stderr, "unsupported unary operator: %d\n", e->unary.op);
					exit(1);
					break;
			}
			break;
        case AST_BINARY:
                gen_expr(e->binary.left);
                printf("  pushq %%rax\n");          
                gen_expr(e->binary.right);
                printf("  popq %%rcx\n");  
				if(e->type_ptr->size >8)
				{
					fprintf(stderr, "binary operations on types larger than 8 bytes are not supported\n");
					exit(1);
				}       
                switch (e->binary.op)
                {
                    case OP_ADD:
                        printf("  addq %%rcx, %%rax\n");    
                        break;
                    case OP_SUB:
                        printf("  subq %%rax, %%rcx\n");    
                        printf("  movq %%rcx, %%rax\n");    
                        break;
                    case OP_MUL:
                        printf("  imulq %%rcx, %%rax\n");   
                        break;
				    case OP_DIV:
				    case OP_MOD:
        				printf("  movq %%rax, %%rbx\n");
        				printf("  movq %%rcx, %%rax\n");
        				printf("  cqo\n");
        				printf("  idivq %%rbx\n");
        				if (e->binary.op == OP_MOD)
						{
                			    printf("  movq %%rdx, %%rax\n");
        				}
        				break;
					case OP_LT:
    					printf("  cmpq %%rax, %%rcx\n");
    					printf("  setl %%al\n");
    					printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_LE:
    					printf("  cmpq %%rax, %%rcx\n");
    					printf("  setle %%al\n");
    					printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_GT:
    					printf("  cmpq %%rax, %%rcx\n");
    					printf("  setg %%al\n");
    					printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_GE:
    					printf("  cmpq %%rax, %%rcx\n");
    					printf("  setge %%al\n");
   						printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_IE:   
   		 				printf("  cmpq %%rax, %%rcx\n");
    					printf("  sete %%al\n");
    					printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_NE:   
    					printf("  cmpq %%rax, %%rcx\n");
    					printf("  setne %%al\n");
    					printf("  movzbq %%al, %%rax\n");
    					break;

					case OP_AND:
						printf("  andq %%rcx, %%rax\n");
						break;
					case OP_OR:
						printf("  orq %%rcx, %%rax\n");
						break;
					case OP_XOR:
						printf("  xorq %%rcx, %%rax\n");
						break;

					case OP_BIT_AND:
						printf("  andq %%rcx, %%rax\n");
						break;
					case OP_BIT_OR:
						printf("  orq %%rcx, %%rax\n");
						break;
					case OP_BIT_XOR:
						printf("  xorq %%rcx, %%rax\n");
						break;
					case OP_BIT_SHL:
						printf("  shlq %%cl, %%rax\n");		
						break;
					case OP_BIT_SHR:
						printf("  shrq %%cl, %%rax\n");
						break;

                	default:
                		fprintf(stderr, "unsupported binary operator: %d\n", e->binary.op);
                		exit(1);
    			}
			break;
		case AST_BIT_ACCESS:
			gen_expr(e->bit_access.base);	//base in rax

			int start = e->bit_access.start->number;
			int width = e->type_ptr->bitslice.width;

			if(start != 0)
			{
				printf("  shrq $%d, %%rax\n", start); //shift down so that desired bits are at the right end
			}

			if(width < 64)
			{
				unsigned long long mask = (1ULL << width) - 1;
				printf("  andq $%llu, %%rax\n", mask);	//delete all unwanted upper bits
			}

			break;

		case AST_ARRAY_INDEX:
		{
			gen_expr(e->array_index.array);
			printf("  pushq %%rax\n");          

			gen_expr(e->array_index.index);
			printf("  popq %%rcx\n");

			int elem_size = e->array_index.array->type_ptr->array.element_type->size;
			printf("  imulq $%d, %%rax\n", elem_size);

			printf("  addq %%rcx, %%rax\n");

			gen_load_from_addr(elem_size);
			
			break;
		}
		case AST_ARRAY_LITERAL:
			gen_array_literal(e, e->var.symbol->offset);
			break;

		case AST_MEMBER_ACCESS:
			gen_lvalue(e);
			gen_load_from_addr(e->type_ptr->size);
			break;
		case AST_FUNCTION_CALL:
			gen_function_call(e);
			break;
		case AST_CAST:
			gen_expr(e->cast.expr);
			int src_size = e->cast.expr->type_ptr->size;
			int dst_size = e->cast.target_type->size;

			if (src_size < dst_size)  
			{
				if (src_size == 1) printf("  movsbq %%al, %%rax\n");
				else if (src_size == 2) printf("  movswq %%ax, %%rax\n");
				else if (src_size == 4) printf("  movslq %%eax, %%rax\n");
			}
			else if (src_size > dst_size) //just ignore upper bytes when narrowing 
			{
				if (dst_size == 4) printf("  movl %%eax, %%eax\n");
				else if (dst_size == 2) printf("  movw %%ax, %%ax\n");
				else if (dst_size == 1) printf("  movb %%al, %%al\n");
			}
			break;
		case AST_BITCAST:
			gen_expr(e->cast.expr);
			break;
		default:
			 fprintf(stderr, "codegen error: invalid expression type\n");
     			 exit(1);
    }
}

static void gen_array_literal(Ast *lit, int base_offset)
{
    for (int i = 0; i < lit->array_literal.count; i++)
    {
        gen_expr(lit->array_literal.elements[i]);
        int elem_size = lit->array_literal.elements[i]->type_ptr->size;
        int offset = base_offset + i * elem_size;
        gen_store_to_rbp(elem_size, offset);
    }
}

static void gen_print_runtime(void)
{
    printf(
        ".section .bss\n"
        "print_buf:\n"
        "    .skip 32\n"
        "\n"
        ".section .text\n"
        "print_int:\n"
        "    lea print_buf+30(%%rip), %%rsi\n"
        "    mov $10, %%rcx\n"
        ".convert:\n"
        "    xor %%rdx, %%rdx\n"
        "    div %%rcx\n"
        "    add $'0', %%dl\n"
        "    dec %%rsi\n"
        "    mov %%dl, (%%rsi)\n"
        "    test %%rax, %%rax\n"
        "    jnz .convert\n"
        "    mov $1, %%rax\n"
        "    mov $1, %%rdi\n"
        "    lea print_buf+32(%%rip), %%rdx\n"
        "    sub %%rsi, %%rdx\n"
        "    syscall\n"
        "    ret\n"
		"print_char:\n"
		"    push %%rax\n"
		"    mov $1, %%rax\n"
		"    mov $1, %%rdi\n"
		"    mov %%rsp, %%rsi\n"
		"    mov $1, %%rdx\n"
		"    syscall\n"
		"    add $8, %%rsp\n"
		"    ret\n"
    );
}

static void gen_lvalue(Ast *node)
{
    switch (node->type)
    {
        case AST_VAR:
            printf("  lea %d(%%rbp), %%rax\n", node->var.symbol->offset);
            break;
            
        case AST_ARRAY_INDEX:
            gen_expr(node->array_index.array);
            printf("  pushq %%rax\n");
            gen_expr(node->array_index.index);
            printf("  popq %%rcx\n");
            int elem_size = node->array_index.array->type_ptr->array.element_type->size;
            printf("  imulq $%d, %%rax\n", elem_size);
            printf("  add %%rcx, %%rax\n");
            break;
            
		case AST_MEMBER_ACCESS:
			gen_lvalue(node->member_access.base);
			printf("  add $%d, %%rax\n", node->member_access.member_offset);
			break;

		case AST_UNARY:
			if (node->unary.op == UN_DEREF)
			{
				gen_expr(node->unary.expr);
				break;
			}
			fprintf(stderr, "cannot take address of unary expression\n");
    		exit(1);
        default:
            fprintf(stderr, "cannot take address of expression\n");
            exit(1);
    }
}

static void gen_function_call(Ast *call)
{
    Symbol *fn = call->function_call.symbol;

    int ret_size = fn->type ? fn->type->size : 0;

    int arg_size = 0;
    for (int i = 0; i < fn->func.param_count; i++)
        arg_size += fn->func.param_types[i]->size;

    int total = ret_size + arg_size;

    if (total > 0)
        printf("  sub $%d, %%rsp\n", total);

    int offset = ret_size;

    for (int i = 0; i < call->function_call.arg_count; i++)
    {
        Ast *arg = call->function_call.arguments[i];
        int size = arg->type_ptr->size;

        gen_expr(arg);
        gen_store_to_rsp(size, offset);
        offset += size;
    }

    printf("  call %s\n", fn->name);

    if (ret_size > 0)
    {
        if (ret_size <= 8)
        {
			printf("  movq %%rsp, %%rax\n");
            gen_load_from_addr(ret_size);   // if small enough store in rax
        }
        else
        {
            printf("  movq %%rsp, %%rax\n");	//if large return keep address temp
        }
    }

    if (total > 0)
        printf("  add $%d, %%rsp\n", total);
}

static void gen_memcpy(int size)
{
    // rax = source address, rdx = destination address
    if (size <= 32)
    {
        int offset = 0;
        while (offset + 8 <= size)
        {
            printf("  movq %d(%%rax), %%rbx\n", offset);
            printf("  movq %%rbx, %d(%%rdx)\n", offset);
            offset += 8;
        }
        if (offset + 4 <= size)
        {
            printf("  movl %d(%%rax), %%ebx\n", offset);
            printf("  movl %%ebx, %d(%%rdx)\n", offset);
            offset += 4;
        }
        if (offset + 2 <= size)
        {
            printf("  movw %d(%%rax), %%bx\n", offset);
            printf("  movw %%bx, %d(%%rdx)\n", offset);
            offset += 2;
        }
        if (offset < size)
        {
            printf("  movb %d(%%rax), %%bl\n", offset);
            printf("  movb %%bl, %d(%%rdx)\n", offset);
        }
    }
    else
    {
        // rep movsb for large copies: rsi = src, rdi = dst, rcx = count
        printf("  movq %%rax, %%rsi\n");
        printf("  movq %%rdx, %%rdi\n");
        printf("  movq $%d, %%rcx\n", size);
        printf("  rep movsb\n");
    }
}

// Load 'size' bytes from address in %rax into %rax (value).
// For size > 8, the address in %rax is left unchanged.
// Uses %rcx and %rsi as scratch (all caller-saved).
static void gen_load_from_addr(int size)
{
    switch (size)
    {
        case 1: printf("  movzbl (%%rax), %%eax\n"); break;
        case 2: printf("  movzwl (%%rax), %%eax\n"); break;
        case 3:
            printf("  movq %%rax, %%rsi\n");
            printf("  movzwl (%%rsi), %%eax\n");
            printf("  movzbl 2(%%rsi), %%ecx\n");
            printf("  shlq $16, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 4: printf("  movl (%%rax), %%eax\n"); break;
        case 5:
            printf("  movq %%rax, %%rsi\n");
            printf("  movl (%%rsi), %%eax\n");
            printf("  movzbl 4(%%rsi), %%ecx\n");
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 6:
            printf("  movq %%rax, %%rsi\n");
            printf("  movl (%%rsi), %%eax\n");
            printf("  movzwl 4(%%rsi), %%ecx\n");
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 7:
            printf("  movq %%rax, %%rsi\n");
            printf("  movl (%%rsi), %%eax\n");
            printf("  movzwl 4(%%rsi), %%ecx\n");
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            printf("  movzbl 6(%%rsi), %%ecx\n");
            printf("  shlq $48, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 8: printf("  movq (%%rax), %%rax\n"); break;
        default: break; // size > 8: address already in rax, leave it
    }
}

// Load 'size' bytes from offset(%rbp) into %rax.
// For size > 8, puts the address (leaq) in %rax instead.
// Uses %rcx as scratch.
static void gen_load_from_rbp(int size, int offset)
{
    switch (size)
    {
        case 1: printf("  movzbl %d(%%rbp), %%eax\n", offset); break;
        case 2: printf("  movzwl %d(%%rbp), %%eax\n", offset); break;
        case 3:
            printf("  movzwl %d(%%rbp), %%eax\n", offset);
            printf("  movzbl %d(%%rbp), %%ecx\n", offset + 2);
            printf("  shlq $16, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 4: printf("  movl %d(%%rbp), %%eax\n", offset); break;
        case 5:
            printf("  movl %d(%%rbp), %%eax\n", offset);
            printf("  movzbl %d(%%rbp), %%ecx\n", offset + 4);
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 6:
            printf("  movl %d(%%rbp), %%eax\n", offset);
            printf("  movzwl %d(%%rbp), %%ecx\n", offset + 4);
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 7:
            printf("  movl %d(%%rbp), %%eax\n", offset);
            printf("  movzwl %d(%%rbp), %%ecx\n", offset + 4);
            printf("  shlq $32, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            printf("  movzbl %d(%%rbp), %%ecx\n", offset + 6);
            printf("  shlq $48, %%rcx\n");
            printf("  orq %%rcx, %%rax\n");
            break;
        case 8: printf("  movq %d(%%rbp), %%rax\n", offset); break;
        default: printf("  leaq %d(%%rbp), %%rax\n", offset); break; // large: return address
    }
}

// Store %rax (value for size <= 8, source address for size > 8) to (%rdx).
// For size > 8 calls gen_memcpy. Clobbers %rax for odd sizes (value is consumed anyway).
static void gen_store_to_mem(int size)
{
    switch (size)
    {
        case 1: printf("  movb %%al, (%%rdx)\n"); break;
        case 2: printf("  movw %%ax, (%%rdx)\n"); break;
        case 3:
            printf("  movw %%ax, (%%rdx)\n");
            printf("  shrq $16, %%rax\n");
            printf("  movb %%al, 2(%%rdx)\n");
            break;
        case 4: printf("  movl %%eax, (%%rdx)\n"); break;
        case 5:
            printf("  movl %%eax, (%%rdx)\n");
            printf("  shrq $32, %%rax\n");
            printf("  movb %%al, 4(%%rdx)\n");
            break;
        case 6:
            printf("  movl %%eax, (%%rdx)\n");
            printf("  shrq $32, %%rax\n");
            printf("  movw %%ax, 4(%%rdx)\n");
            break;
        case 7:
            printf("  movl %%eax, (%%rdx)\n");
            printf("  shrq $32, %%rax\n");
            printf("  movw %%ax, 4(%%rdx)\n");
            printf("  shrq $16, %%rax\n");
            printf("  movb %%al, 6(%%rdx)\n");
            break;
        case 8: printf("  movq %%rax, (%%rdx)\n"); break;
        default: gen_memcpy(size); break; // rax=src addr, rdx=dst addr
    }
}

// Store %rax to offset(%rbp). Large types: rax is source address, uses gen_memcpy.
static void gen_store_to_rbp(int size, int offset)
{
    printf("  leaq %d(%%rbp), %%rdx\n", offset);
    gen_store_to_mem(size);
}

// Store %rax to offset(%rsp). Large types: rax is source address, uses gen_memcpy.
static void gen_store_to_rsp(int size, int offset)
{
    printf("  leaq %d(%%rsp), %%rdx\n", offset);
    gen_store_to_mem(size);
}
