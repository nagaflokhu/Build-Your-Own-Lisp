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
enum {LVAL_INT, LVAL_DBL, LVAL_ERR};
enum {LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM};
// enum to figure out whether we are dealing with operations
// with two integers or two doubles
enum {FIRST_INT, FIRST_DBL, TWO_INTS, TWO_DBLS};
typedef struct {
	int type;
	double dbl;
    long num;
	int err;
} lval;

lval lval_int(long x) {
	lval v;
	v.type = LVAL_INT;
	v.num = x;
	return v;
}

lval lval_dbl(double x) {
    lval v;
    v.type = LVAL_DBL;
    v.dbl = x;
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
		case LVAL_INT:
			printf("%ld", v.num);
			break;
        case LVAL_DBL:
            printf("%f", v.dbl);
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
    mpc_parser_t* Integer = mpc_new("integer");
    mpc_parser_t* Double = mpc_new("double");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          operator: '+' | '/' | '*' | '-' | '%' | '^' |\
		  	\"min\" | \"max\" ; \
          double: /-?[0-9]+[.][0-9]*/; \
          integer: /-?[0-9]+/; \
          expr: <integer> | <double> | '(' <operator> <expr>+ ')'; \
          lispr: /^/ <operator> <expr>+ /$/; \
        ", Operator, Integer, Double, Expr, Lispr);
    
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
    mpc_cleanup(5, Operator, Integer, Double, Expr, Lispr);
    return 0;
}

lval eval(mpc_ast_t* t) {
    // if it's a number, return it directly
    if (strstr(t->tag, "integer")) {
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_int(x) : lval_err(LERR_BAD_NUM);
    }
    
    if (strstr(t->tag, "double")) {
        errno = 0;
        double x = strtod(t->contents, NULL);
        return errno != ERANGE ? lval_dbl(x) : lval_err(LERR_BAD_NUM);
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
    
    int type;
    if (x.type == LVAL_INT && y.type == LVAL_DBL)
        type = FIRST_INT;
    else if (x.type == LVAL_DBL && y.type == LVAL_INT)
        type = FIRST_DBL;
    else if (x.type == LVAL_INT && y.type == LVAL_INT)
        type = TWO_INTS;
    else if (x.type == LVAL_DBL && y.type == LVAL_DBL)
        type = TWO_DBLS;
	
    if (strcmp(op, "+") == 0) {
        switch (type) {
            case FIRST_INT:
                return lval_dbl(x.num + y.dbl);
            case FIRST_DBL:
                return lval_dbl(x.dbl + y.num);
            case TWO_INTS:
                return lval_int(x.num + y.num);
            case TWO_DBLS:
                return lval_dbl(x.dbl + y.dbl);
        }
    }
    
    if (strcmp(op, "-") == 0) {
        switch (type) {
            case FIRST_INT:
                return lval_dbl(x.num - y.dbl);
            case FIRST_DBL:
                return lval_dbl(x.dbl - y.num);
            case TWO_INTS:
                return lval_int(x.num - y.num);
            case TWO_DBLS:
                return lval_dbl(x.dbl - y.dbl);
        }
    }
    
    if (strcmp(op, "*") == 0) {
        switch (type) {
            case FIRST_INT:
                return lval_dbl(x.num * y.dbl);
            case FIRST_DBL:
                return lval_dbl(x.dbl * y.num);
            case TWO_INTS:
                return lval_int(x.num * y.num);
            case TWO_DBLS:
                return lval_dbl(x.dbl * y.dbl);
        }
    }
    
    if (strcmp(op, "/") == 0) {
        switch (type) {
            case FIRST_INT:
                if (y.dbl == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(x.num / y.dbl);
            case FIRST_DBL:
                if (y.num == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(x.dbl / y.num);
            case TWO_INTS:
                if (y.num == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_int(x.num / y.num);
            case TWO_DBLS:
                if (y.dbl == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(x.dbl / y.dbl);
        }
	}
    
    if (strcmp(op, "%") == 0) {
        switch (type) {
            case FIRST_INT:
                if (y.dbl == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(fmod(x.num, y.dbl));
            case FIRST_DBL:
                if (y.num == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(fmod(x.dbl, y.num));
            case TWO_INTS:
                if (y.num == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_int(x.num % y.num);
            case TWO_DBLS:
                if (y.dbl == 0)
                    return lval_err(LERR_DIV_ZERO);
                return lval_dbl(fmod(x.dbl, y.dbl));
        }
	}
    
	if (strcmp(op, "^") == 0) {
        double dres = 1;
        long lres = 1;
        switch (type) {
            case FIRST_INT:
                printf("Exponentiation with non-integer exponent not yet supported.\n");
                return lval_err(LERR_BAD_OP);
            case FIRST_DBL:
                for (long i = 0; i < y.num; i++) {
                    dres *= x.dbl;
                }
                return lval_dbl(dres);
            case TWO_INTS:
                for (long i = 0; i < y.num; i++) {
                    lres *= x.num;
                }
                return lval_int(lres);
            case TWO_DBLS:
                printf("Exponentiation with non-integer exponent not yet supported.\n");
                return lval_err(LERR_BAD_OP);
        }
	}
    
	if (strcmp(op, "min") == 0) {
        switch (type) {
            case FIRST_INT:
                return x.num < y.dbl ? x : y;
            case FIRST_DBL:
                return x.dbl < y.num ? x : y;
            case TWO_INTS:
                return x.num < y.num ? x : y;
            case TWO_DBLS:
                return x.dbl < y.dbl ? x : y;
        }
    }
    
	if (strcmp(op, "max") == 0) {
        switch (type) {
            case FIRST_INT:
                return x.num > y.dbl ? x : y;
            case FIRST_DBL:
                return x.dbl > y.num ? x : y;
            case TWO_INTS:
                return x.num > y.num ? x : y;
            case TWO_DBLS:
                return x.dbl > y.dbl ? x : y;
        }
    }
	
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