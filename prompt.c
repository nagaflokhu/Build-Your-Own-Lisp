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

// Macro for error checking
#define LASSERT(args, cond, err)\
    if(!(cond)) { lval_del(args); return lval_err(err); }
#define CHECK_NUM(arg, num, err)\
    LASSERT(arg, arg->count == num, err)
#define CHECK_EMPTY(arg, err)\
    LASSERT(arg, arg->cell[0]->count != 0, err)

// lvals represent the result of evaluating a lisp
// expression
enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, 
      LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};
enum {LONG, DOUBLE};

// Since lvals can either hold longs or doubles, I'm creating
// a union type, Num, to holdthe values, which should simplify
// later code.
typedef struct {
    union {
        long l;
        double d;
    };
    int type;
} Num;

// Forward declaration for types with cyclical dependencies
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
typedef lval*(*lbuiltin)(lenv*, lval*);

struct lval {
    int type;
	Num num;
    
    // the error and symbols will be stored as strings
    char* err;
    char* sym;
    lbuiltin fun;
    
    // counter and pointer to a list of lval pointers
    int count;
    struct lval** cell;
};

struct lenv {
    int count;
    lval** vals;
    char** syms;
};

lval* eval(mpc_ast_t*);
lval* eval_op(lval*, char*, lval*);
lval* lval_num(Num);
lval* lval_err(char*);
lval* lval_sym(char*);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
void lval_del(lval*);
lval* lval_read_num(mpc_ast_t*);
lval* lval_read(mpc_ast_t*);
lval* lval_add(lval*, lval*);
void lval_expr_print(lval*, char, char);
void lval_print(lval*);
void lval_println(lval*);
lval* lval_eval_sexpr(lenv*, lval*);
lval* lval_eval(lenv*, lval*);
lval* lval_pop(lval*, int);
lval* lval_take(lval*, int);
lval* builtin_op(lval*, char*);
lval* builtin_head(lval*);
lval* builtin_list(lval*);
lval* builtin_eval(lenv*, lval*);
lval* builtin_join(lval*);
lval* lval_join(lval*,lval*);
lval* builtin_cons(lval*);
lval* len(lval*);
lval* init(lval*);
lval* lval_fun(lbuiltin);
lval* lval_copy(lval*);
lenv* lenv_new(void);
void lenv_del(lenv*);
lval* lenv_get(lenv*, lval*);
void lenv_put(lenv*, lval*, lval*);
int valid_math_input(lval*);
lval* builtin_add(lenv*, lval*);
void lenv_add_builtin(lenv*, char*, lbuiltin);
void lenv_add_builtins(lenv*);

int main(int argc, char** argv) {
    
    // Create parser
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Long = mpc_new("long");
    mpc_parser_t* Double = mpc_new("double");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          number: <double> | <long>; \
          long: /-?[0-9]+/; \
          double: /-?[0-9]+[.][0-9]*/; \
          symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/; \
          sexpr: '(' <expr>* ')'; \
          qexpr: '{' <expr>* '}'; \
          expr: <number> | <symbol> | <sexpr> | <qexpr>; \
          lispr: /^/ <expr>* /$/; \
        ", Number, Long, Double, Symbol, Sexpr, Qexpr, Expr, Lispr);
    
    puts("Lispr Version 0.0.0.0.1");
    puts("Press ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    
    // Infinite loop
    while (1) {
        char* input = readline("lispr> ");
        add_history(input);
        
        // Attempt to parse input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispr, &r)) {
            // On success, evaluate the input
			lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
        }
        else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        // need to free result?
        free(input);
    }
    
    // Deallocate memory
    mpc_cleanup(8, Number, Long, Double, Symbol, Sexpr, Qexpr, Expr, Lispr);
    return 0;
}

lval* lval_num(Num x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char* e) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
    v->err = malloc(strlen(e)+1);
	strcpy(v->err, e);
	return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s)+1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_FUN: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    
    free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    
    if (strstr(t->tag, "long")) {
        long x = strtol(t->contents, NULL, 10);
        if (errno != ERANGE) {
            Num n;
            n.type = LONG;
            n.l = x;
            return lval_num(n);
        }
        return lval_err("invalid number");
    }
    
    if (strstr(t->tag, "double")) {
        double x = strtod(t->contents, NULL);
        if (errno != ERANGE) {
            Num n;
            n.type = DOUBLE;
            n.d = x;
            return lval_num(n);
        }
        return lval_err("invalid number");
    }
    return lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    // If Symbol or Number, return converstion to that type.
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);
    
    // If root (>) or sexpr, create empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strstr(t->tag, "sexpr")) x = lval_sexpr();
    if (strstr(t->tag, "qexpr")) x = lval_qexpr();
    
    // Fill list with any valid expressions contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->contents, "{") == 0) continue;
        if (strcmp(t->children[i]->contents, "}") == 0) continue;
        if (strcmp(t->children[i]->tag, "regex") == 0) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(v->cell[i]);
        
        // print trailing space if not last element
        if (i != (v->count-1))
            putchar(' ');
    }
    putchar(close);
}

void lval_print(lval* v) {
	switch (v->type) {
		case LVAL_NUM:
            switch (v->num.type) {
                case LONG:
                    printf("%ld", v->num.l);
                    break;
                case DOUBLE:
                    printf("%f", v->num.d);
                    break;
            }
            break;
		case LVAL_ERR:
			printf("Error: %s", v->err);
		    break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUN:
            printf("<function>");
            break;
	}
}

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
    
    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
        // Error checking
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v,i);
    }
    
    // Empty expression
    if (v->count == 0) return v;
    
    // Single expression
    if (v->count == 1) return lval_take(v,0);
    
    // Ensure first element is a function
    lval* f = lval_pop(v,0);
    if (f->type != LVAL_FUN) {
        lval_del(v); lval_del(f);
        return lval_err("first element is not a function");
    }
    
    lval* result = f->fun(e,v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    // Evaluate S-Expressions
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(e, v);
    // All other lval types are returned as is
    return v;
}

lval* lval_pop(lval* v, int i) {
    // Find the item at i
    lval* x = v->cell[i];
    
    // Shift memory after item i
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count-i-1));
    
    // Decrease item count
    v->count--;
    
    // Reallocate used memory
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lval* a, char* op) {
    // Ensure all arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on a non-number!");
        }
    }
    
    // Pop the first element
    lval* x = lval_pop(a, 0);
    
    // If we are dealing with a subtraction with a single argument, perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        if (x->num.type == LONG)
            x->num.l = -x->num.l;
        else if (x->num.type == DOUBLE)
            x->num.d = -x->num.d;
        lval_del(a);
        return x;
    }
    
    // While there are still arguments
    while (a->count > 0) {
        // Pop the next element
        lval* y = lval_pop(a, 0);
        
        if (strcmp(op, "+") == 0) {
            if (x->num.type == LONG && y->num.type == LONG) {
                x->num.l += y->num.l;
            }
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                x->num.type = DOUBLE;
                x->num.d = x->num.l + y->num.d;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG) {
                x->num.d += y->num.l;
            }
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE) {
                x->num.d += y->num.d;
            }
        }

        if (strcmp(op, "-") == 0) {
            if (x->num.type == LONG && y->num.type == LONG) {
                x->num.l -= y->num.l;
            }
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                x->num.type = DOUBLE;
                x->num.d = x->num.l - y->num.d;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG) {
                x->num.d -= y->num.l;
            }
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE) {
                x->num.d -= y->num.d;
            }
        }

        if (strcmp(op, "*") == 0) {
            if (x->num.type == LONG && y->num.type == LONG) {
                x->num.l *= y->num.l;
            }
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                x->num.type = DOUBLE;
                x->num.d = x->num.l * y->num.d;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG) {
                x->num.d *= y->num.l;
            }
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE) {
                x->num.d *= y->num.d;
            }
        }

        if (strcmp(op, "/") == 0) {
            if (x->num.type == LONG && y->num.type == LONG) {
                if (y->num.l == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.l /= y->num.l;
            }
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                if (y->num.d == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.type = DOUBLE;
                x->num.d = x->num.l / y->num.d;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG) {
                if (y->num.l == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.d /= y->num.l;
            }
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE) {
                if (y->num.d == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.d /= y->num.d;
            }
        }

        if (strcmp(op, "%") == 0) {
            if (x->num.type == LONG && y->num.type == LONG) {
                if (y->num.l == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.l %= y->num.l;
            }
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                if (y->num.d == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.type = DOUBLE;
                x->num.d = fmod(x->num.l, y->num.d);
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG) {
                if (y->num.l == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.d = fmod(x->num.d, y->num.l);
            }
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE) {
                if (y->num.d == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Division by zero!");
                    break;
                }
                x->num.d = fmod(x->num.d, y->num.d);
            }
        }

        if (strcmp(op, "^") == 0) {
            if (y->num.type == DOUBLE) {
                lval_del(x); lval_del(y);
                x = lval_err("Exponentiation with non-integer exponent not yet supported.");
                break;
            }

            if (y->num.l < 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Exponentiation with negative exponent not yet supported.");
                break;
            }

            Num n;
            switch(x->num.type) {
                case LONG:
                    n.type = LONG;
                    n.l = 1;
                    for (long i = 0; i < y->num.l; i++) {
                        n.l *= x->num.l;
                    }
                    x->num = n;
                    break;
                case DOUBLE:
                    n.type = DOUBLE;
                    n.d = 1;
                    for (long i = 0; i < y->num.l; i++) {
                        n.d *= x->num.d;
                    }
                    x->num = n;
                    break;
            }
        }

        if (strcmp(op, "min") == 0) {
            if (x->num.type == LONG && y->num.type == LONG)
                x->num.l = x->num.l < y->num.l ? x->num.l : y->num.l;
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                x->num.type = DOUBLE;
                x->num.d = x->num.l < y->num.d ? x->num.l : y->num.d;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG)
                x->num.d = x->num.d < y->num.l ? x->num.d : y->num.l;
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE)
                x->num.d = x->num.d < y->num.d ? x->num.d : y->num.d;
        }

        if (strcmp(op, "max") == 0) {
            if (x->num.type == LONG && y->num.type == LONG)
                x->num.l = x->num.l < y->num.l ? y->num.l : x->num.l;
            else if (x->num.type == LONG && y->num.type == DOUBLE) {
                x->num.type = DOUBLE;
                x->num.l = x->num.l < y->num.d ? y->num.d : x->num.l;
            }
            else if (x->num.type == DOUBLE && y->num.type == LONG)
                x->num.d = x->num.d < y->num.l ? y->num.l : x->num.d;
            else if (x->num.type == DOUBLE && y->num.type == DOUBLE)
                x->num.d = x->num.d < y->num.d ? y->num.d : x->num.d;
        }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}

lval* builtin_head(lval* a) {
    // Check for error conditions
    CHECK_NUM(a, 1, "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect types!");
    CHECK_EMPTY(a, "Function 'head' passed {}!");
    
    lval* v = lval_take(a, 0);
    
    // Delete all other element and return v
    while (v->count > 1) lval_del(lval_pop(v, 1));
    return v;
}

lval* builtin_tail(lval* a) {
    // Check for error conditions
    CHECK_NUM(a, 1, "Function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' passed incorrect types!");
    CHECK_EMPTY(a, "Function 'tail' passed {}!");
    
    lval* v = lval_take(a, 0);
    
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    CHECK_NUM(a, 1, "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect types!");
    
    lval* x = lval_take(a,0);
    x->type = LVAL_SEXPR;
    return lval_eval(e,x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect types!");
    }
    
    lval* x = lval_pop(a,0);
    while (a->count) {
        x = lval_join(x, lval_pop(a,0));
    }
    
    lval_del(a);
    return x;
}

lval* lval_join(lval* x, lval* y) {
    // Add each cell in y to x
    while (y->count) {
        x = lval_add(x, lval_pop(y,0));
    }
    
    lval_del(y);
    return x;
}

lval* builtin_cons(lval* a) {
    // First argument should be a value and second should be a qexpr
    LASSERT(a, a->cell[0]->type == LVAL_NUM,
            "Function 'cons' requires a value!");
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
            "Function 'cons' requires a qexpr!");
    
    lval* x = lval_qexpr();
    x = lval_add(x, lval_pop(a,0));
    while (a->count) {
        x = lval_join(x, lval_pop(a,0));
    }
    
    lval_del(a);
    return x;
}

lval* builtin_len(lval* a) {
    CHECK_NUM(a, 1, "Function 'len' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'len' requires a qexpr!");
    
    Num n;
    n.type = LONG;
    n.l = a->cell[0]->count;
    return lval_num(n);
}

lval* builtin_init(lval* a) {
    CHECK_NUM(a, 1, "Function 'init' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'init' requires a qexpr!");
    CHECK_EMPTY(a, "Function 'init' passed {}!");
    
    lval* x = lval_take(a,0);
    lval_del(lval_pop(x, x->count-1));
    return x;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->fun = func;
    v->type = LVAL_FUN;
    return v;
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch (v->type) {
        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;
            
        case LVAL_ERR:
            x->err = malloc(strlen(v->err)+1);
            strcpy(x->err, v->err); break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym)+1);
            strcpy(x->sym, v->sym); break;
            
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* v) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], v->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v) {
    // Check if symbol already exists
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }
    
    // Create space and add symbol
    e->count++;
    e->vals = realloc(e->vals, e->count * sizeof(lval*));
    e->syms = realloc(e->syms, e->count * sizeof(char*));
    
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

int valid_math_input(lval* v) {
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type != LVAL_NUM)
            return 0;
    }
    return 1;
}

lval* builtin_add(lenv* e, lval* a) {
    if (!valid_math_input(a)) {
        lval_del(a);
        return lval_err("'+' needs all numeric inputs");
    }
    lval* x = lval_pop(a,0);
    
    while (a->count) {
        lval* y = lval_pop(a,0);
        
        if (x->num.type == LONG && y->num.type == LONG) {
            x->num.l += y->num.l;
        }
        else if (x->num.type == LONG && y->num.type == DOUBLE) {
            x->num.type = DOUBLE;
            x->num.d = x->num.l + y->num.d;
        }
        else if (x->num.type == DOUBLE && y->num.type == LONG) {
            x->num.d += y->num.l;
        }
        else {
            x->num.d += y->num.d;
        }
        
        lval_del(y);
    }
    
    lval_del(a);
    return x;
}

lval* builtin_sub(lenv* e, lval* a) {
    if (!valid_math_input(a)) {
        lval_del(a);
        return lval_err("'-' needs all numeric inputs");
    }
    lval* x = lval_pop(a,0);
    
    if (a->count == 0) {
        if (x->num.type == LONG) {
            x->num.l = -x->num.l;
        }
        else {
            x->num.d = -x->num.d;
        }
        lval_del(a);
        return x;
    }
    
    while (a->count) {
        lval* y = lval_pop(a,0);
        
        if (x->num.type == LONG && y->num.type == LONG) {
            x->num.l -= y->num.l;
        }
        else if (x->num.type == LONG && y->num.type == DOUBLE) {
            x->num.type = DOUBLE;
            x->num.d = x->num.l - y->num.d;
        }
        else if (x->num.type == DOUBLE && y->num.type == LONG) {
            x->num.d -= y->num.l;
        }
        else {
            x->num.d -= y->num.d;
        }
        
        lval_del(y);
    }
    
    lval_del(a);
    return x;
}

lval* builtin_mul(lenv* e, lval* a) {
    if (!valid_math_input(a)) {
        lval_del(a);
        return lval_err("'*' requires all numeric inputs");
    }
    
    lval* x = lval_pop(a,0);
    
    while(a->count) {
        lval* y = lval_pop(a,0);
        if (x->num.type == LONG) {
            if (y->num.type == LONG) {
                x->num.l *= y->num.l;
            }
            else {
                x->num.type = DOUBLE;
                x->num.d = x->num.l * y->num.d;
            }
        }
        else {
            if (y->num.type == LONG) {
                x->num.d *= y->num.l;
            }
            else {
                x->num.d *= y->num.d;
            }
        }

        lval_del(y);
    }
    
    lval_del(a); return x;
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e,k,v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
}