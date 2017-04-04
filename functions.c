#include "functions.h"

lval* lval_num(Num x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char* fmt, ...) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
    
    // create and initialize a va list
    va_list va;
    va_start(va, fmt);
    
    v->err = malloc(512);
		vsnprintf(v->err, 511, fmt, va);
    v->err = realloc(v->err, strlen(v->err)+1);
    
    va_end(va);
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
        case LVAL_FUN:
						if (!v->builtin) {
							lenv_del(v->env);
							lval_del(v->formals);
							lval_del(v->body);
						}
						break;
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
    }
    
    else if (strstr(t->tag, "double")) {
        double x = strtod(t->contents, NULL);
        if (errno != ERANGE) {
            Num n;
            n.type = DOUBLE;
            n.d = x;
            return lval_num(n);
        }
    }
    
    return lval_err("invalid number %s", t->contents);
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

void lval_expr_print(lenv* e, lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        // print value contained within
        lval_print(e, v->cell[i]);
        
        // print trailing space if not last element
        if (i != (v->count-1))
            putchar(' ');
    }
    putchar(close);
}

void lval_print(lenv* e, lval* v) {
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
            lval_expr_print(e, v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(e, v, '{', '}');
            break;
        case LVAL_FUN:
						// Search environment for same pointer as v
						// Print corresponding function name
						for (int i = 0; i < e->count; i++) {
							if (e->vals[i]->builtin == v->builtin) {
								printf("%s", e->syms[i]);
							}
						}
            break;
	}
}

void lval_println(lenv* e, lval* v) {
	lval_print(e,v);
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
    
    lval* result = f->builtin(e,v);
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

lval* builtin_head(lenv* e, lval* a) {
    // Check for error conditions
		CHECK_COUNT("head", a, 1);
		CHECK_INPUT_TYPE("head", a, 0, LVAL_QEXPR);
    CHECK_EMPTY(a, "Function 'head' passed {}!");
    
    lval* v = lval_take(a, 0);
    
    // Delete all other element and return v
    while (v->count > 1) lval_del(lval_pop(v, 1));
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    // Check for error conditions
		CHECK_COUNT("tail", a, 1);
		CHECK_INPUT_TYPE("tail", a, 0, LVAL_QEXPR);
    CHECK_EMPTY(a, "Function 'tail' passed {}!");
    
    lval* v = lval_take(a, 0);
    
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
		CHECK_COUNT("eval", a, 1);
		CHECK_INPUT_TYPE("eval", a, 0, LVAL_QEXPR);
    lval* x = lval_take(a,0);
    x->type = LVAL_SEXPR;
    return lval_eval(e,x);
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
			CHECK_INPUT_TYPE("join", a, i, LVAL_QEXPR);
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

lval* builtin_cons(lenv* e, lval* a) {
    // First argument should be a value and second should be a qexpr
		CHECK_COUNT("cons", a, 2);
		CHECK_INPUT_TYPE("cons", a, 0, LVAL_NUM);
		CHECK_INPUT_TYPE("cons", a, 1, LVAL_QEXPR);
   
    lval* x = lval_qexpr();
    x = lval_add(x, lval_pop(a,0));
    while (a->count) {
        x = lval_join(x, lval_pop(a,0));
    }
    
    lval_del(a);
    return x;
}

lval* builtin_len(lenv* e, lval* a) {
		CHECK_COUNT("len", a, 1);
		CHECK_INPUT_TYPE("len", a, 0, LVAL_QEXPR);
   
    Num n;
    n.type = LONG;
    n.l = a->cell[0]->count;
    return lval_num(n);
}

lval* builtin_init(lenv* e, lval* a) {
		CHECK_COUNT("init", a, 1);
		CHECK_INPUT_TYPE("init", a, 0, LVAL_QEXPR);
		CHECK_EMPTY(a, "Function 'init' passed {}!");
    
    lval* x = lval_take(a,0);
    lval_del(lval_pop(x, x->count-1));
    return x;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->builtin = func;
    v->type = LVAL_FUN;
    return v;
}

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;
    
    switch (v->type) {
        case LVAL_FUN: x->builtin = v->builtin; break;
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
    return lval_err("Unbound symbol '%s'", v->sym);
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
    LASSERT(a, valid_math_input(a), "'+' requires all numerical inputs");
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
    LASSERT(a, valid_math_input(a), "'-' requires all numerical inputs");
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
    LASSERT(a, valid_math_input(a), "'*' requires all numerical inputs");
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

lval* builtin_div(lenv* e, lval* a) {
    LASSERT(a, valid_math_input(a), "'/' requires all numerical inputs");
    lval* x = lval_pop(a,0);
    
    while (a->count) {
        lval* y = lval_pop(a,0);
        
        switch (x->num.type) {
            case LONG:
                switch (y->num.type) {
                    case LONG:
                        if (y->num.l == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.l /= y->num.l;
                        break;
                    case DOUBLE:
                        if (y->num.d == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.type = DOUBLE;
                        x->num.d = x->num.l / y->num.d;
                        break;
                }
                break;
            case DOUBLE:
                switch (y->num.type) {
                    case LONG:
                        if (y->num.l == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.d /= y->num.l;
                        break;
                    case DOUBLE:
                        if (y->num.d == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.d /= y->num.d;
                        break;
                }
                break;
        }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}

lval* builtin_mod(lenv* e, lval* a) {
    LASSERT(a, valid_math_input(a), "'%' requires all numerical inputs");
    lval* x = lval_pop(a,0);
    
    while (a->count) {
        lval* y = lval_pop(a,0);
        
        switch (x->num.type) {
            case LONG:
                switch (y->num.type) {
                    case LONG:
                        if (y->num.l == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.l %= y->num.l;
                        break;
                    case DOUBLE:
                        if (y->num.d == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.type = DOUBLE;
                        x->num.d = fmod(x->num.l, y->num.d);
                        break;
                }
                break;
            case DOUBLE:
                switch (y->num.type) {
                    case LONG:
                        if (y->num.l == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.d = fmod(x->num.d, y->num.l);
                        break;
                    case DOUBLE:
                        if (y->num.d == 0) {
                            lval_del(x); lval_del(y);
                            return lval_err("division by zero");
                        }
                        x->num.d = fmod(x->num.d, y->num.d);
                        break;
                }
                break;
        }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}

lval* builtin_exp(lenv* e, lval* a) {
    LASSERT(a, valid_math_input(a), "'^' requires all numerical inputs");
    lval* x = lval_pop(a,0);
    
    while (a->count) {
        lval* y = lval_pop(a,0);
        
        if (y->num.type == DOUBLE) {
            lval_del(x); lval_del(y);
            return lval_err("exponentiation by non-integer not supported yet. "
										"Got %f, of type double, as an exponent.", y->num.type);
        }
        
        if (y->num.l < 0) {
            lval_del(x); lval_del(y);
            return lval_err("exponentiation by negative exponent not supported yet. "
										"Got %ld as an exponent.", y->num.l);
        }
        
        Num n;
        long l;
        double d;
        switch (x->num.type) {
            case LONG:
                l = 1;
                for (long i = 0; i < y->num.l; i++) {
                    l *= x->num.l;
                }
                n.type = LONG;
                n.l = l;
                lval_del(x);
                x = lval_num(n);
                break;
            case DOUBLE:
                d = 1;
                for (long i = 0; i < y->num.l; i++) {
                    d *= x->num.d;
                }
                n.type = DOUBLE;
                n.d = d;
                lval_del(x);
                x = lval_num(n);
                break;
        }
        
        lval_del(y);
    }
    
    lval_del(a); return x;
}

lval* builtin_def(lenv* e, lval* a) {
		CHECK_INPUT_TYPE("def", a, 0, LVAL_QEXPR);
   
    // Symbols to define passed as first argument qexpr
    lval* syms = a->cell[0];
    
    // Make sure everything in qexpr is a symbol
    for (int i = 0; i < syms->count; i++) {
				CHECK_INPUT_TYPE("def", syms, i, LVAL_SYM);
				// Part of the ugly implementation of exit: prohibit definition of exit
				if (strcmp(syms->cell[i]->sym, "exit") == 0) {
					lval_del(a);
					return lval_err("def of 'exit' is prohibited.");
				}
   }
    
    // Make sure number of values matches number of symbols
    LASSERT(a, syms->count == a->count-1,
            "'def' received %d symbols and %d values. Mismatch.",
           syms->count, a->count-1);
    
    for (int i = 0; i < syms->count; i++) {
				if (lenv_get(e, syms->cell[i])->type != LVAL_ERR) {
					lval_del(a);
					return lval_err("attempting to redefine builtin function.");
				}
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }
    
    lval_del(a);
    return lval_sexpr();
}

char* ltype_name(int t) {
	switch (t) {
		case LVAL_FUN: return "function";
		case LVAL_NUM: return "number";
		case LVAL_ERR: return "error";
		case LVAL_SYM: return "symbol";
		case LVAL_SEXPR: return "s-expression";
		case LVAL_QEXPR: return "q-expression";
		default: return "Unknown";
	}
}

lval* lval_lambda(lval* formals, lval* body) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;

	// Set builtin to NULL; this allows us to distinguish between builtin and user-defined
	// functions
	v->builtin = NULL;

	// Build new environment
	v->env = lenv_new();

	// Set formals and body
	v->formals = formals;
	v->body = body;
	return v;
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
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_exp);
    
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "len", builtin_len);
    lenv_add_builtin(e, "init", builtin_init);
    
    lenv_add_builtin(e, "def", builtin_def);
}
