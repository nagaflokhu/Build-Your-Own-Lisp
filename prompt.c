#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

// input buffer
static char buffer[2048];

// fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

// fake add history function
void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif

// lvals represent the result of evaluating a lisp
// expression
enum {LVAL_NUM, LVAL_ERR};
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};
typedef struct {
	int type;
	long num;
	int err;
} lval;

lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

void lval_print(lval v) {
	switch (v.type) {
		case LVAL_NUM:
			printf("%li", v.num);
			break;
		case LVAL_ERR:
			switch (v.err) {
				case LERR_DIV_ZERO:
					printf("Error: Division by zero.");
					break;
				case LERR_BAD_OP:
					printf("Error: Invalid operator.");
					break;
				case LERR_BAD_NUM:
					printf("Error: Invalid number.");
					break;
			}
		break;
	}
}

void lval_println(lval v) {
	lval_print(v);
	putchar('\n');
}

lval eval(mpc_ast_t*);
lval eval_op(lval, char*, lval);
int count_leaves(mpc_ast_t*);
int count_branches(mpc_ast_t*);
int most_children(mpc_ast_t*);

int main(int argc, char** argv) {
    
    // Create parser
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          operator: '+' | '/' | '*' | '-' | '%' | '^' |\
		  	\"min\" | \"max\" ; \
          number: /-?[0-9]+[.]?[0-9]*/; \
          expr: <number> | '(' <operator> <expr>+ ')'; \
          lispr: /^/ <operator> <expr>+ /$/; \
        ", Operator, Number, Expr, Lispr);
    
    puts("Lispr Version 0.0.0.0.1");
    puts("Press ctrl+c to Exit\n");
    
    // Infinite loop
    while (1) {
        char* input = readline("lispr> ");
        add_history(input);
        
        // Attempt to parse input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispr, &r)) {
            // On success, evaluate the input
			lval result = eval(r.output);
			lval_println(result);
            mpc_ast_delete(r.output);
        }
        else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    
    // Deallocate memory
    mpc_cleanup(4, Operator, Number, Expr, Lispr);
    return 0;
}

lval eval(mpc_ast_t* t) {
    // if it's a number, return it directly
    if (strstr(t->tag, "number")) {
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }
    
    // the operator is always second child
    char* op = t->children[1]->contents;
    lval x = eval(t->children[2]);
    
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    
    return x;
}

lval eval_op(lval x, char* op, lval y) {
	if (x.type == LVAL_ERR) return x;
	if (y.type == LVAL_ERR) return y;
	
    if (strcmp(op, "+") == 0) return lval_num(x.num + y.num);
    if (strcmp(op, "-") == 0) return lval_num(x.num - y.num);
    if (strcmp(op, "*") == 0) return lval_num(x.num * y.num);
    if (strcmp(op, "/") == 0) {
		return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
	}
    if (strcmp(op, "%") == 0) {
		return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num % y.num);
	}
	if (strcmp(op, "^") == 0) {
		long result = 1;
		for (long i = 0; i < y.num; i++) {
			result *= x.num;
		}
		return lval_num(result);;
	}
	if (strcmp(op, "min") == 0) return x.num > y.num ? y : x;
	if (strcmp(op, "max") == 0) return x.num > y.num ? x : y;
	
	return lval_err(LERR_BAD_OP);
}

int count_leaves(mpc_ast_t* t) {
    int child_num = t->children_num;
    if (child_num == 0) return 1;
    int total = 0;
    for (int i = 0; i < child_num; i++) {
        total = total + count_leaves(t->children[i]);
    }
    return total;
}

int count_branches(mpc_ast_t* t) {
    int child_num = t->children_num;
    if (child_num == 0) return 0;
    int total = 1;
    for (int i = 0; i < child_num; i++) {
        total = total + count_branches(t->children[i]);
    }
    return total;
}

int most_children(mpc_ast_t* t) {
	int child_num = t->children_num;
    int most = child_num;
	for (int i = 0; i < child_num; i++) {
		int children = most_children(t->children[i]);
		if (children > most) {
			most = children;
		}
	}
	return most;
}