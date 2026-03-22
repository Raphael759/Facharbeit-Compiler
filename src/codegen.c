#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "ast.h"
#include "symbol.h"

#define out(...) fprintf(output_file, __VA_ARGS__)
static FILE *output_file;

static void gen_function(Ast *f);
static void gen_block(Ast **stmts, int count);
static void gen_stmt(Ast *s);
static void gen_expr(Ast *e);
static void gen_print_runtime(void);
static void gen_array_literal(Ast *lit, int base_offset, int use_rsp);
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
static int function_count = 0;
static int current_function = -1;
static int current_return_size = 0;

void codegen_program(Ast **program, int program_length)
{
	output_file = fopen("output.s", "w");
	if (!output_file) 
	{
        fprintf(stderr, "Error opening output file\n");
        exit(1);
    }

	gen_print_runtime();
	for (int i = 0; i < program_length; i++)
	{
		if(program[i]->type == AST_FUNCTION)
		{
			gen_function(program[i]);
		}
	}

	fclose(output_file);
}

static void gen_function(Ast *f)
{
		current_function = function_count++;
		current_return_size = f->function.ret_type ? f->function.ret_type->size : 0;

        out(".globl %s\n", f->function.name);
        out("%s:\n", f->function.name);

        out("  pushq %%rbp\n");          
        out("  movq %%rsp, %%rbp\n");

		if(f->function.stack_size > 0)
			out("  sub $%d, %%rsp\n", f->function.stack_size);

        gen_block(f->function.body, f->function.body_length);

		out(".Lreturn_%d:\n", current_function);
        out("  movq %%rbp, %%rsp\n");    
        out("  popq %%rbp\n");           
        out("  ret\n");
		current_function = -1;
}

static void gen_loop(Ast *l)
{
	int loop_count_local = loop_count++;
	if (l->loop.init)
        gen_stmt(l->loop.init);

	out(".L%d_check:\n", loop_count_local);
	if (l->loop.cond)
    {
        gen_expr(l->loop.cond);
        out("  cmpq $0, %%rax\n");
        out("  je .L%d_end\n", loop_count_local);
    }

	gen_block(l->loop.body, l->loop.body_length);
	if (l->loop.update)
        gen_stmt(l->loop.update);
	out("  jmp .L%d_check\n",loop_count_local);


	out(".L%d_end:\n", loop_count_local);

}

static void gen_if(Ast *i)
{
	int if_count_local = if_count;
	if_count++;
	out(".If%d_check:\n", if_count_local);
	gen_expr(i->if_stmt.cond);
	out("  cmpq $0, %%rax\n");
	if(i->if_stmt.else_stmt)
		out("  je .else%d\n", if_count_local);
	else
		out("  je .If%d_end\n", if_count_local);
	gen_block(i->if_stmt.body, i->if_stmt.body_length);
	if(i->if_stmt.else_stmt)
		out("  jmp .If%d_end\n", if_count_local);
	if(i->if_stmt.else_stmt)
	{
		out(".else%d:\n", if_count_local);
		gen_block(i->if_stmt.else_stmt->else_stmt.body, i->if_stmt.else_stmt->else_stmt.body_length);
	}
	out(".If%d_end:\n", if_count_local);
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
				out("  leaq 16(%%rbp), %%rdx\n");  // return space
				gen_store_to_mem(current_return_size);
			}

			out("  jmp .Lreturn_%d\n", current_function);
            break;

		case AST_PRINT:
			gen_expr(s->print.expr);
			if(s->print.expr->type_ptr->kind == TYPE_CHAR)
				out("  call print_char\n");
			else
				out("  call print_int\n");

			break;
		case AST_VAR_DECL:
			if (s->var_decl.init_value)
			{
				if (s->var_decl.init_value->type_ptr->kind == TYPE_ARRAY)
				{
					gen_array_literal(s->var_decl.init_value, s->var_decl.symbol->offset, 0);
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
				out("  movq %%rax, %%rdx\n");	//address to base in rdx

				int size = base->type_ptr->size;
				
				if (size == 1)
					out("  movzbq (%%rdx), %%rbx\n");
				else if (size == 2)
					out("  movzwq (%%rdx), %%rbx\n");
				else if (size == 4)
					out("  movl (%%rdx), %%ebx\n");
				else if (size == 8)
					out("  movq (%%rdx), %%rbx\n");		//base value in rbx

				
				gen_expr(value);           // to be assigned in rax

				if (width < 64)
				{
					unsigned long long small =	(1ULL << width) - 1ULL;
					out("  andq $%llu, %%rax\n", small);	//idfk
				}

				if (start != 0)
					out("  shlq $%d, %%rax\n", start);	//shift bits of the value, so that lowest is start

				out("  movq %%rbx, %%rcx\n");
				out("  andq $%llu, %%rcx\n", ~mask);	//clear slice in target

				out("  orq %%rcx, %%rax\n");

				if (size == 1)
					out("  movb %%al, (%%rdx)\n");
				else if (size == 2)
					out("  movw %%ax, (%%rdx)\n");
				else if (size == 4)
					out("  movl %%eax, (%%rdx)\n");
				else if (size == 8)
					out("  movq %%rax, (%%rdx)\n");

				break;
			}
			gen_expr(value);
			out("  movq %%rax, %%r8\n");// value (or address for large types) in r8
			gen_lvalue(target);     
			out("  movq %%rax, %%rdx\n");	// address to target in rdx    
			out("  movq %%r8, %%rax\n");		//restore value
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
                out("  movq $%lld, %%rax\n", e->number);  
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
				out("  leaq %d(%%rbp), %%rax\n", e->var.symbol->offset);
				break;
			}
			if(e->type_ptr->size <= 8 && e->type_ptr->size > 0)
			{
				gen_load_from_rbp(e->type_ptr->size, e->var.symbol->offset);
			}
			else 
			{
				out("  leaq %d(%%rbp), %%rax\n", e->var.symbol->offset);
			}
			break;
     	}
		case AST_UNARY:
			if (e->unary.op == UN_ADDR)
			{
				gen_lvalue(e->unary.expr);   // result: address in %rax
				break;
			}
			gen_expr(e->unary.expr);
			switch (e->unary.op)
			{
				case UN_NEG:
					if(e->unary.expr->type_ptr->size == 1) out("  negb %%al\n");
                    else if(e->unary.expr->type_ptr->size == 2) out("  negw %%ax\n");
                    else if(e->unary.expr->type_ptr->size == 4) out("  negl %%eax\n");
                    else if(e->unary.expr->type_ptr->size == 8) out("  negq %%rax\n");
                    break;
				case UN_NOT:	//0 -> 1, else -> 0
					out("  cmp $0, %%rax\n");
                    out("  sete %%al\n");
                    out("  movzbq %%al, %%rax\n"); 
                    break;
				case UN_BIT_NOT:
					if(e->unary.expr->type_ptr->size == 1) out("  notb %%al\n");
                    else if(e->unary.expr->type_ptr->size == 2) out("  notw %%ax\n");
                    else if(e->unary.expr->type_ptr->size == 4) out("  notl %%eax\n");
                    else if(e->unary.expr->type_ptr->size == 8) out("  notq %%rax\n");
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
                out("  pushq %%rax\n");          
                gen_expr(e->binary.right);
                out("  popq %%rcx\n");  
				if(e->type_ptr->size >8)
				{
					fprintf(stderr, "binary operations on types larger than 8 bytes are not supported\n");
					exit(1);
				}       
                switch (e->binary.op)
                {
                    case OP_ADD:
                        out("  addq %%rcx, %%rax\n");    
                        break;
                    case OP_SUB:
                        out("  subq %%rax, %%rcx\n");    
                        out("  movq %%rcx, %%rax\n");    
                        break;
                    case OP_MUL:
                        out("  imulq %%rcx, %%rax\n");   
                        break;
				    case OP_DIV:
				    case OP_MOD:
        				out("  movq %%rax, %%rbx\n");
        				out("  movq %%rcx, %%rax\n");
        				out("  cqo\n");
        				out("  idivq %%rbx\n");
        				if (e->binary.op == OP_MOD)
						{
                			    out("  movq %%rdx, %%rax\n");
        				}
        				break;
					case OP_LT:
    					out("  cmpq %%rax, %%rcx\n");
    					out("  setl %%al\n");
    					out("  movzbq %%al, %%rax\n");
    					break;

					case OP_LE:
    					out("  cmpq %%rax, %%rcx\n");
    					out("  setle %%al\n");
    					out("  movzbq %%al, %%rax\n");
    					break;

					case OP_GT:
    					out("  cmpq %%rax, %%rcx\n");
    					out("  setg %%al\n");
    					out("  movzbq %%al, %%rax\n");
    					break;

					case OP_GE:
    					out("  cmpq %%rax, %%rcx\n");
    					out("  setge %%al\n");
   						out("  movzbq %%al, %%rax\n");
    					break;

					case OP_IE:   
   		 				out("  cmpq %%rax, %%rcx\n");
    					out("  sete %%al\n");
    					out("  movzbq %%al, %%rax\n");
    					break;

					case OP_NE:   
    					out("  cmpq %%rax, %%rcx\n");
    					out("  setne %%al\n");
    					out("  movzbq %%al, %%rax\n");
    					break;

					case OP_AND:
						out("  andq %%rcx, %%rax\n");
						break;
					case OP_OR:
						out("  orq %%rcx, %%rax\n");
						break;
					case OP_XOR:
						out("  xorq %%rcx, %%rax\n");
						break;

					case OP_BIT_AND:
						out("  andq %%rcx, %%rax\n");
						break;
					case OP_BIT_OR:
						out("  orq %%rcx, %%rax\n");
						break;
					case OP_BIT_XOR:
						out("  xorq %%rcx, %%rax\n");
						break;
					case OP_BIT_SHL:
					    out("  xchgq %%rax, %%rcx\n");
						out("  shlq %%cl, %%rax\n");		
						break;
					case OP_BIT_SHR:
						out("  xchgq %%rax, %%rcx\n");
						out("  shrq %%cl, %%rax\n");
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
				out("  shrq $%d, %%rax\n", start); //shift down so that desired bits are at the right end
			}

			if(width < 64)
			{
				unsigned long long mask = (1ULL << width) - 1;
				out("  andq $%llu, %%rax\n", mask);	//delete all unwanted upper bits
			}

			break;

		case AST_ARRAY_INDEX:
		{
			gen_expr(e->array_index.array);
			out("  pushq %%rax\n");          

			gen_expr(e->array_index.index);
			out("  popq %%rcx\n");

			int elem_size = e->array_index.array->type_ptr->array.element_type->size;
			out("  imulq $%d, %%rax\n", elem_size);

			out("  addq %%rcx, %%rax\n");

			gen_load_from_addr(elem_size);
			
			break;
		}
		case AST_ARRAY_LITERAL:
			int size = e->type_ptr->size;
			out("  sub $%d, %%rsp\n", size);	//allocate space for array literal
			gen_array_literal(e, 0, 1);
			out("  movq %%rsp, %%rax\n");	//address of array literal in rax
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
				if (src_size == 1) out("  movsbq %%al, %%rax\n");
				else if (src_size == 2) out("  movswq %%ax, %%rax\n");
				else if (src_size == 4) out("  movslq %%eax, %%rax\n");
			}
			else if (src_size > dst_size) //just ignore upper bytes when narrowing 
			{
				if (dst_size == 4) out("  movl %%eax, %%eax\n");
				else if (dst_size == 2) out("  movw %%ax, %%ax\n");
				else if (dst_size == 1) out("  movb %%al, %%al\n");
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

static void gen_array_literal(Ast *lit, int base_offset, int use_rsp)
{
	int elem_size = lit->array_literal.elements[0]->type_ptr->size;
    for (int i = 0; i < lit->array_literal.count; i++)
    {
        gen_expr(lit->array_literal.elements[i]);
        int offset = base_offset + i * elem_size;
        if(use_rsp)
			gen_store_to_rsp(elem_size, offset);
		else
			gen_store_to_rbp(elem_size, offset);
    }
}

static void gen_print_runtime(void)
{
    out(
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
            out("  leaq %d(%%rbp), %%rax\n", node->var.symbol->offset);
            break;
            
        case AST_ARRAY_INDEX:
            gen_expr(node->array_index.array);
            out("  pushq %%rax\n");
            gen_expr(node->array_index.index);
            out("  popq %%rcx\n");
            int elem_size = node->array_index.array->type_ptr->array.element_type->size;
            out("  imulq $%d, %%rax\n", elem_size);
            out("  add %%rcx, %%rax\n");
            break;
            
		case AST_MEMBER_ACCESS:
			gen_lvalue(node->member_access.base);
			out("  add $%d, %%rax\n", node->member_access.member_offset);
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
        out("  sub $%d, %%rsp\n", total);

    int offset = ret_size;

    for (int i = 0; i < call->function_call.arg_count; i++)
    {
        Ast *arg = call->function_call.arguments[i];
        int size = arg->type_ptr->size;

        gen_expr(arg);
        gen_store_to_rsp(size, offset);
        offset += size;
    }

    out("  call %s\n", fn->name);

    if (ret_size > 0)
    {
        if (ret_size <= 8)
        {
			out("  movq %%rsp, %%rax\n");
            gen_load_from_addr(ret_size);   // if small enough store in rax
        }
        else
        {
            out("  movq %%rsp, %%rax\n");	//if large return keep address temp
        }
    }

    if (total > 0)
        out("  add $%d, %%rsp\n", total);
}

static void gen_memcpy(int size)
{
    // rax = source address, rdx = destination address
    if (size <= 32)
    {
        int offset = 0;
        while (offset + 8 <= size)
        {
            out("  movq %d(%%rax), %%rbx\n", offset);
            out("  movq %%rbx, %d(%%rdx)\n", offset);
            offset += 8;
        }
        if (offset + 4 <= size)
        {
            out("  movl %d(%%rax), %%ebx\n", offset);
            out("  movl %%ebx, %d(%%rdx)\n", offset);
            offset += 4;
        }
        if (offset + 2 <= size)
        {
            out("  movw %d(%%rax), %%bx\n", offset);
            out("  movw %%bx, %d(%%rdx)\n", offset);
            offset += 2;
        }
        if (offset < size)
        {
            out("  movb %d(%%rax), %%bl\n", offset);
            out("  movb %%bl, %d(%%rdx)\n", offset);
        }
    }
    else
    {
        // rep movsb for large copies: rsi = src, rdi = dst, rcx = count
        out("  movq %%rax, %%rsi\n");
        out("  movq %%rdx, %%rdi\n");
        out("  movq $%d, %%rcx\n", size);
        out("  rep movsb\n");
    }
}

// for size <= 8 load from address in %rax to %rax
// For size > 8 address stays in rax
static void gen_load_from_addr(int size)
{
    switch (size)
    {
        case 1: out("  movzbl (%%rax), %%eax\n"); break;
        case 2: out("  movzwl (%%rax), %%eax\n"); break;
        case 3:
            out("  movq %%rax, %%rsi\n");
            out("  movzwl (%%rsi), %%eax\n");
            out("  movzbl 2(%%rsi), %%ecx\n");
            out("  shlq $16, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 4: out("  movl (%%rax), %%eax\n"); break;
        case 5:
            out("  movq %%rax, %%rsi\n");
            out("  movl (%%rsi), %%eax\n");
            out("  movzbl 4(%%rsi), %%ecx\n");
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 6:
            out("  movq %%rax, %%rsi\n");
            out("  movl (%%rsi), %%eax\n");
            out("  movzwl 4(%%rsi), %%ecx\n");
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 7:
            out("  movq %%rax, %%rsi\n");
            out("  movl (%%rsi), %%eax\n");
            out("  movzwl 4(%%rsi), %%ecx\n");
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            out("  movzbl 6(%%rsi), %%ecx\n");
            out("  shlq $48, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 8: out("  movq (%%rax), %%rax\n"); break;
        default: break; // size > 8: address already in rax, leave it
    }
}

// Load size bytes from offset into %rax
// for size > 8 put address in %rax 
static void gen_load_from_rbp(int size, int offset)
{
    switch (size)
    {
        case 1: out("  movzbl %d(%%rbp), %%eax\n", offset); break;
        case 2: out("  movzwl %d(%%rbp), %%eax\n", offset); break;
        case 3:
            out("  movzwl %d(%%rbp), %%eax\n", offset);
            out("  movzbl %d(%%rbp), %%ecx\n", offset + 2);
            out("  shlq $16, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 4: out("  movl %d(%%rbp), %%eax\n", offset); break;
        case 5:
            out("  movl %d(%%rbp), %%eax\n", offset);
            out("  movzbl %d(%%rbp), %%ecx\n", offset + 4);
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 6:
            out("  movl %d(%%rbp), %%eax\n", offset);
            out("  movzwl %d(%%rbp), %%ecx\n", offset + 4);
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 7:
            out("  movl %d(%%rbp), %%eax\n", offset);
            out("  movzwl %d(%%rbp), %%ecx\n", offset + 4);
            out("  shlq $32, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            out("  movzbl %d(%%rbp), %%ecx\n", offset + 6);
            out("  shlq $48, %%rcx\n");
            out("  orq %%rcx, %%rax\n");
            break;
        case 8: out("  movq %d(%%rbp), %%rax\n", offset); break;
        default: out("  leaq %d(%%rbp), %%rax\n", offset); break; // large: return address
    }
}

// store %rax (value for size <= 8, source address for size > 8) to (%rdx).
// for size > 8 calls gen_memcpy. Clobbers %rax for odd sizes (value is consumed anyway).
static void gen_store_to_mem(int size)
{
    switch (size)
    {
        case 1: out("  movb %%al, (%%rdx)\n"); break;
        case 2: out("  movw %%ax, (%%rdx)\n"); break;
        case 3:
            out("  movw %%ax, (%%rdx)\n");
            out("  shrq $16, %%rax\n");
            out("  movb %%al, 2(%%rdx)\n");
            break;
        case 4: out("  movl %%eax, (%%rdx)\n"); break;
        case 5:
            out("  movl %%eax, (%%rdx)\n");
            out("  shrq $32, %%rax\n");
            out("  movb %%al, 4(%%rdx)\n");
            break;
        case 6:
            out("  movl %%eax, (%%rdx)\n");
            out("  shrq $32, %%rax\n");
            out("  movw %%ax, 4(%%rdx)\n");
            break;
        case 7:
            out("  movl %%eax, (%%rdx)\n");
            out("  shrq $32, %%rax\n");
            out("  movw %%ax, 4(%%rdx)\n");
            out("  shrq $16, %%rax\n");
            out("  movb %%al, 6(%%rdx)\n");
            break;
        case 8: out("  movq %%rax, (%%rdx)\n"); break;
        default: gen_memcpy(size); break; // rax=src addr, rdx=dst addr
    }
}

// Store %rax to offset(%rbp). Large types: rax is source address, uses gen_memcpy.
static void gen_store_to_rbp(int size, int offset)
{
    out("  leaq %d(%%rbp), %%rdx\n", offset);
    gen_store_to_mem(size);
}

// Store %rax to offset(%rsp). Large types: rax is source address, uses gen_memcpy.
static void gen_store_to_rsp(int size, int offset)
{
    out("  leaq %d(%%rsp), %%rdx\n", offset);
    gen_store_to_mem(size);
}