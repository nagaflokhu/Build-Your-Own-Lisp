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

lval* lval_str(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_STR;
	v->str = malloc(strlen(s)+1);
	strcpy(v->str, s);
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

lval* lval_bool(int b) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_BOOL;
	v->bool = b;
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
				case LVAL_STR: free(v->str); break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
				break;
				case LVAL_BOOL: break;
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
		if (strstr(t->tag, "string")) return lval_read_str(t);
    
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
				if (strstr(t->children[i]->tag, "comment")) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }
    
    return x;
}

lval* lval_read_str(mpc_ast_t* t) {
	// Cut off final quote character
	t->contents[strlen(t->contents)-1] = '\0';
	// Copy the string without the initial quote
	char* unescaped = malloc(strlen(t->contents+1)+1);
	strcpy(unescaped, t->contents+1);
	// Pass through the unescape function
	unescaped = mpcf_unescape(unescaped);
	// New lval using unescaped string
	lval* str = lval_str(unescaped);
	free(unescaped);
	return str;
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
	  case LVAL_STR:
			lval_print_str(v);
		break;
		case LVAL_SEXPR:
			lval_expr_print(e, v, '(', ')');
		break;
		case LVAL_QEXPR:
			lval_expr_print(e, v, '{', '}');
		break;
		case LVAL_FUN:
			if (v->builtin) {
				printf("<builtin>");
			}
			else {
				printf("(\\ "); lval_print(e,v->formals);
				putchar(' '); lval_print(e,v->body); putchar(')');
			}
		break;
		case LVAL_BOOL:
			if (v->bool == TRUE) {
				printf("t");
			}
			else {
				printf("nil");
			}
		break;
	}
}

void lval_print_str(lval* v) {
	char* escaped = malloc(strlen(v->str)+1);
	strcpy(escaped, v->str);
	// Pass it through the escape function in the mpc library
	escaped = mpcf_escape(escaped);
	// Print between '"' characters
	printf("\"%s\"", escaped);
	free(escaped);
}

void lval_println(lenv* e, lval* v) {
	lval_print(e,v);
	putchar('\n');
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
	// Before evaluating an s-expression, we need to evaluate each of its
	// components
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e,v->cell[i]);
		if (v->cell[i]->type == LVAL_ERR) return lval_take(v,i);
	}

	// If the sexpr is empty, we can just return it
	if (v->count == 0) return v;
	// If it has one component, we can evaulate it and return it
	if (v->count == 1) return lval_eval(e,lval_take(v,0));

	lval* f = lval_pop(v,0);
	if (f->type != LVAL_FUN) {
		lval* err = lval_err("S-expression starts with incorrect type. "
				"Got %s, expected %s.", ltype_name(f->type), ltype_name(LVAL_FUN));
		lval_del(f); lval_del(v);
		return err;
	}

	lval* result = lval_call(e,f,v);
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
		int first_type = a->cell[0]->type;
		if (first_type != LVAL_QEXPR && first_type != LVAL_STR) {
			lval* err = lval_err("Function 'head' passed wrong argument type. "
					"Got a %s, expected a %s or a %s.", ltype_name(first_type),
					ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));
			lval_del(a);
			return err;
		}

		lval* v;
		if (first_type == LVAL_QEXPR) {
			CHECK_EMPTY(a, "Function 'head' passed {}!");
			v = lval_take(a, 0);
			// Delete all other element and return v
			while (v->count > 1) lval_del(lval_pop(v, 1));
		}
		else {
			char* str = malloc(2);
			str[0] = a->cell[0]->str[0];
			str[1] = '\0';
			v = lval_str(str);
			free(str);
			lval_del(a);
		}
    
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    // Check for error conditions
		CHECK_COUNT("tail", a, 1);
		int first_type = a->cell[0]->type;
		if (first_type != LVAL_QEXPR && first_type != LVAL_STR) {
			lval* err = lval_err("Function 'tail' passed wrong argument type. Got a "
					"%s, expected a %s or a %s.", ltype_name(first_type),
					ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));
			lval_del(a);
			return err;
		}

	lval* v;
		if (first_type == LVAL_QEXPR) {
			CHECK_EMPTY(a, "Function 'tail' passed {}!");
			// a is a q-expression. We assign its contents to v, and then delete
			// the first element
			v = lval_take(a, 0);
			lval_del(lval_pop(v, 0));
		}
		else {
			char* str = malloc(strlen(a->cell[0]->str));
			// skip the first character
			strcpy(str, a->cell[0]->str+1);
    	v = lval_str(str);
			free(str);
			lval_del(a);
		}
    
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
		// We want join to work on q-expressions and strings. We need to check
		// that the arguments passed to it are either all q-expressions or all
		// strings.
		int first_type = a->cell[0]->type;
		if (first_type != LVAL_QEXPR && first_type != LVAL_STR) {
			lval* err = lval_err("Function 'join' pased wrong argument type. Got a "
					"%s, expected a %s or %s.", ltype_name(first_type), 
					ltype_name(LVAL_QEXPR), ltype_name(LVAL_STR));
			lval_del(a);
			return err;
		}

		for (int i = 1; i < a->count; i++) {
			if (a->cell[i]->type != first_type) {
				lval* err = lval_err("Function 'join' passed incompatible types. Got "
						"a %s as the first argument and a %s.", ltype_name(first_type),
						ltype_name(a->cell[i]->type));
				lval_del(a);
				return err;
			}
		}
     
		lval* x;
		if (first_type == LVAL_QEXPR) {
			x = lval_pop(a,0);
			while (a->count) {
					x = lval_join(x, lval_pop(a,0));
			}
		}
		else {
			char* str = malloc(strlen(a->cell[0]->str)+1);
			strcpy(str, a->cell[0]->str);
			for (int i = 1; i < a->count; i++) {
				strcat(str, a->cell[i]->str);
			}
			x = lval_str(str);
			free(str);
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
        case LVAL_FUN: 
					if (v->builtin) {
						x->builtin = v->builtin;
					}
					else {
						x->builtin = NULL;
						x->env = lenv_copy(v->env);
						x->formals = lval_copy(v->formals);
						x->body = lval_copy(v->body);
					}
				break;
        case LVAL_NUM: x->num = v->num; break;
            
        case LVAL_ERR:
            x->err = malloc(strlen(v->err)+1);
            strcpy(x->err, v->err); 
				break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym)+1);
            strcpy(x->sym, v->sym); 
				break;
				case LVAL_STR:
						x->str = malloc(strlen(v->str)+1);
						strcpy(x->str, v->str);
				break;
            
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
				break;
				case LVAL_BOOL:
					x->bool = v->bool;
    }
    return x;
}

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
		e->par = NULL;
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

		if (e->par) {
			return lenv_get(e->par, v);
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
	return builtin_var(e,a,"def");
}

lval* builtin_put(lenv* e, lval* a) {
	return builtin_var(e,a,"=");
}

lval* builtin_var(lenv* e, lval* a, char* func) {
		CHECK_INPUT_TYPE(func, a, 0, LVAL_QEXPR);
   
    // Symbols to define passed as first argument qexpr
    lval* syms = a->cell[0];
    
    // Make sure everything in qexpr is a symbol
    for (int i = 0; i < syms->count; i++) {
				LASSERT(a, (syms->cell[i]->type == LVAL_SYM), "Function '%s' cannot "
						"define non-symbol. Got %s, expected %s.", func,
						ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
   }
    
    // Make sure number of values matches number of symbols
    LASSERT(a, syms->count == a->count-1,
            "Function '%s' received %d symbols and %d values. Mismatch.",
           func, syms->count, a->count-1);
    
    for (int i = 0; i < syms->count; i++) {
				lval* val = lenv_get(e, syms->cell[i]);
				if (val->type == LVAL_FUN && val->builtin != NULL) {
					lval_del(a); lval_del(val);
					return lval_err("attempting to redefine builtin function.");
				}
				lval_del(val);

				if (strcmp(func, "def") == 0) {
					lenv_def(e, syms->cell[i], a->cell[i+1]);
				}

				if (strcmp(func, "=") == 0) {
					lenv_put(e, syms->cell[i], a->cell[i+1]);
				}
    }
    
    lval_del(a);
    return lval_sexpr();
}

lval* lval_call(lenv* e, lval* f, lval* a) {
	if (f->builtin) return f->builtin(e,a);

	// Record argument counts
	int given = a->count;
	int total = f->formals->count;

	while (a->count) {
		if (f->formals->count == 0) {
			lval_del(a); 
			return lval_err("Function passed too many arguments. Got %i, expected "
					"%i", given, total);
		}

		// Pop first symbol from formals
		lval* sym = lval_pop(f->formals,0);

		// Adding way to deal with variable number of arguments
		if (strcmp(sym->sym, "&") == 0) {
			// Ensure "&" is followed by another symbol
			if (f->formals->count != 1) {
				lval_del(a);
				return lval_err("Function format invalid. Symbol '&' not followed by "
						"single symbol");
			}

			// Bind next formal to remaining arguments
			lval* nsym = lval_pop(f->formals,0);
			lenv_put(f->env, nsym, builtin_list(e,a));
			lval_del(sym); lval_del(nsym);
			break;
		}
		// Pop next argument from the list
		lval* val = lval_pop(a,0);
		// Bind copy into function's environment
		lenv_put(f->env, sym, val);
		// Delete symbol and value
		lval_del(sym); lval_del(val);
	}

	lval_del(a);
	// If '&' remains in formal list, bind to empty list
	if (f->formals->count > 0 && strcmp(f->formals->cell[0]->sym, "&") == 0) {
		// Check that '&' is passed with one other symbol
		if (f->formals->count != 2) {
			return lval_err("Function format invalid. Symbol '&' not followed by "
					"single symbol.");
		}

		// Pop and delete '&' symbol
		lval_del(lval_pop(f->formals,0));

		// Pop next symbol and create empty list
		lval* sym = lval_pop(f->formals,0);
		lval* val = lval_qexpr();

		// Bind to environment and delete
		lenv_put(f->env, sym, val);
		lval_del(sym); lval_del(val);
	}
	// If all formals have been bound, evaluate function
	else if (f->formals->count == 0) {
		f->env->par = e;
		return builtin_eval(f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
	}
	return lval_copy(f);
}

char* ltype_name(int t) {
	switch (t) {
		case LVAL_FUN: return "function";
		case LVAL_NUM: return "number";
		case LVAL_ERR: return "error";
		case LVAL_SYM: return "symbol";
		case LVAL_STR: return "string";
		case LVAL_SEXPR: return "s-expression";
		case LVAL_QEXPR: return "q-expression";
		default: return "unknown";
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

lval* builtin_lambda(lenv* e, lval* a) {
	// Check that there are two arguments, both q-expressions
	CHECK_COUNT("\\", a, 2);
	CHECK_INPUT_TYPE("\\", a, 0, LVAL_QEXPR);
	CHECK_INPUT_TYPE("\\", a, 1, LVAL_QEXPR);

	// Check that first q-expr only contains symbols
	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM), "Cannot define "
				"non-symbol. Got %s, expected %s",
				ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
	}

	// Pop two last arguments and pass them to lval_lambda
	lval* formals = lval_pop(a,0);
	lval* body = lval_pop(a,0);
	lval_del(a);

	return lval_lambda(formals, body);
}

lval* lval_equals(lval* x, lval* y) {
	// If the inputs have different type, they can't be equal
	if (x->type != y->type) {
		return lval_bool(FALSE);
	}

	lval* res;
	switch (x->type) {
		case LVAL_NUM:
			// Simple numerical comparison
			res = numerical_equals(x->num, y->num);
		break;
		case LVAL_SYM:
			res = strcmp(x->sym,y->sym) == 0 ? lval_bool(TRUE) : lval_bool(FALSE);
		break;
		case LVAL_STR:
			res = strcmp(x->str, y->str) == 0 ? lval_bool(TRUE) : lval_bool(FALSE);
		break;
		case LVAL_FUN:
			if (x->builtin || y->builtin) {
				res = x->builtin == y->builtin ? lval_bool(TRUE) :
					lval_bool(FALSE);
			}
			else {
				if (lval_equals(x->formals,y->formals) &&
						lval_equals(x->body,y->body)) {
					res = lval_bool(TRUE);
				}
				else res = lval_bool(FALSE);
			}
		break;
		case LVAL_ERR:
			res = strcmp(x->err,y->err) == 0 ? lval_bool(TRUE) :
				lval_bool(FALSE);
		break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			if (x->count != y->count) return lval_bool(FALSE);
			for (int i = 0; i < x->count; i++) {
				if (lval_equals(x->cell[i], y->cell[i])->bool == FALSE) {
					lval_del(x); lval_del(y);
					return lval_bool(FALSE);
				}
			}
			res = lval_bool(TRUE);
		break;
	}

	return res;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
	CHECK_COUNT(op, a, 2);

	lval* res;
	if (strcmp(op, "==") == 0) {
		res = lval_equals(a->cell[0],a->cell[1]);
	}
	else {
		// Opposite result
		lval* opp = lval_equals(a->cell[0],a->cell[1]);
		res = opp->bool == TRUE ? lval_bool(FALSE) : lval_bool(TRUE);
		lval_del(opp);
	}
	lval_del(a);
	return res;
}

lval* builtin_eq(lenv* e, lval* a) {
	return builtin_cmp(e,a,"==");
}

lval* builtin_ne(lenv* e, lval* a) {
	return builtin_cmp(e,a,"!=");
}
			
lval* numerical_equals(Num x, Num y) {
	int result;
	switch (x.type) {
		case LONG:
			switch (y.type) {
				case LONG:
					if (x.l == y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.l == y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
		case DOUBLE:
			switch (y.type) {
				case LONG:
					if (x.d == y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.d == y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
	}

	return lval_bool(result);
}

lval* builtin_greater_than(lenv* e, lval* a) {
	CHECK_COUNT(">", a, 2);
	CHECK_INPUT_TYPE(">", a, 0, LVAL_NUM);
	CHECK_INPUT_TYPE(">", a, 1, LVAL_NUM);
	
	Num x = a->cell[0]->num;
	Num y = a->cell[1]->num;
	int result;

	switch (x.type) {
		case LONG:
			switch (y.type) {
				case LONG:
					if (x.l > y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.l > y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
		case DOUBLE:
			switch (y.type) {
				case LONG:
					if (x.d > y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.d > y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
	}

	lval_del(a);
	return result == TRUE ? lval_bool(TRUE) : lval_bool(FALSE);
}

lval* builtin_smaller_than(lenv* e, lval* a) {
	CHECK_COUNT("<", a, 2);
	CHECK_INPUT_TYPE("<", a, 0, LVAL_NUM);
	CHECK_INPUT_TYPE("<", a, 1, LVAL_NUM);

	// Result of opposite operation
	Num x = a->cell[0]->num;
	Num y = a->cell[1]->num;
	int result;
	switch (x.type) {
		case LONG:
			switch (y.type) {
				case LONG:
					if (x.l < y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.l < y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
		case DOUBLE:
			switch (y.type) {
				case LONG:
					if (x.d < y.l) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
				case DOUBLE:
					if (x.d < y.d) {
						result = TRUE;
						break;
					}
					result = FALSE;
				break;
			}
		break;
	}

	lval_del(a);
	return result == TRUE ? lval_bool(TRUE) : lval_bool(FALSE);
}

lval* builtin_smaller_than_or_equal_to(lenv* e, lval* a) {
	CHECK_COUNT("<=", a, 2);
	CHECK_INPUT_TYPE("<=", a, 0, LVAL_NUM);
	CHECK_INPUT_TYPE("<=", a, 0, LVAL_NUM);

	lval* opp = builtin_greater_than(e, a);
	int result = opp->bool == TRUE ? FALSE : TRUE;
	// a gets deallocated in builtin_greater_than
	lval_del(opp);
	return result == TRUE ? lval_bool(TRUE) : lval_bool(FALSE);
}

lval* builtin_greater_than_or_equal_to(lenv* e, lval* a) {
	CHECK_COUNT(">=", a, 2);
	CHECK_INPUT_TYPE(">=", a, 0, LVAL_NUM);
	CHECK_INPUT_TYPE(">=", a, 0, LVAL_NUM);

	lval* opp = builtin_smaller_than(e, a);
	int result = opp->bool == TRUE ? FALSE : TRUE;
	// a gets deallocated in builtin_smaller_than
	lval_del(opp);
	return result == TRUE ? lval_bool(TRUE) : lval_bool(FALSE);
}

lval* builtin_if(lenv* e, lval* a) {
	// an if should have three parts: a condition, code to be evaluated
	// if the condition is true, and code to be evaluated if the condition
	// is false
	CHECK_COUNT("if", a, 3);
	// Our booleans are implemented as symbols for the time being
	CHECK_INPUT_TYPE("if", a, 0, LVAL_BOOL);
	CHECK_INPUT_TYPE("if", a, 1, LVAL_QEXPR);
	CHECK_INPUT_TYPE("if", a, 2, LVAL_QEXPR);

	lval* result;
	// make both q-expressions be s-expressions so they can be evaluated
	a->cell[1]->type = LVAL_SEXPR;
	a->cell[2]->type = LVAL_SEXPR;
	if (a->cell[0]->bool == TRUE) {
		result = lval_eval(e, lval_pop(a,1));
	}
	else {
		result = lval_eval(e, lval_pop(a,2));
	}
	return result;
}

lval* builtin_and(lenv* e, lval* a) {
	// and should receive two LVAL_BOOLs or more
	if (a->count < 2) {
		lval* err = lval_err("Function '&&' received %d arguments, expects at "
				"least 2.", a->count);
		lval_del(a);
		return err;
	}

	// check that they are all LVAL_BOOL and evaluate
	lval* result;
	for (int i = 0; i < a->count; i++) {
		CHECK_INPUT_TYPE("&&", a, i, LVAL_BOOL);
		if (a->cell[i]->bool == FALSE) {
			result = lval_bool(FALSE);
			lval_del(a);
			return result;
		}
	}

	result = lval_bool(TRUE);
	lval_del(a);
	return result;
}

lval* builtin_or(lenv* e, lval* a) {
	// should receive at least two inputs
	if (a->count < 2) {
		lval* err = lval_err("Function '||' received %d arguments, expects at "
				"least 2.", a->count);
		lval_del(a);
		return err;
	}

	// Check that arguments are LVAL_BOOLs and evaluate
	lval* result;
	for (int i = 0; i < a->count; i++) {
		CHECK_INPUT_TYPE("||", a, i, LVAL_BOOL);
		if (a->cell[i]->bool == TRUE) {
			result = lval_bool(TRUE);
			lval_del(a);
			return result;
		}
	}

	result = lval_bool(FALSE);
	lval_del(a);
	return result;
}

lval* builtin_not(lenv* e, lval* a) {
	CHECK_COUNT("!", a, 1);
	CHECK_INPUT_TYPE("!", a, 0, LVAL_BOOL);

	lval* result;
	switch (a->cell[0]->bool) {
		case TRUE:
			result = lval_bool(FALSE);
			break;
		case FALSE:
			result = lval_bool(TRUE);
			break;
	}
	lval_del(a);
	return result;
}

lval* builtin_print(lenv* e, lval* a) {
	for (int i = 0; i < a->count; i++) {
		lval_print(e, a->cell[i]); putchar(' ');
	}
	putchar('\n');
	lval_del(a);
	return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
	CHECK_COUNT("error", a, 1);
	CHECK_INPUT_TYPE("error", a, 0, LVAL_STR);

	lval* err = lval_err(a->cell[0]->str);
	lval_del(a);
	return err;
}

lenv* lenv_copy(lenv* e) {
	lenv* n = malloc(sizeof(lenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*) * n->count);
	n->vals = malloc(sizeof(lval*) * n->count);
	for (int i = 0; i < e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

void lenv_def(lenv* e, lval* k, lval* v) {
	while (e->par) e = e->par;
	lenv_put(e,k,v);
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

		lenv_add_builtin(e, "==", builtin_eq);
		lenv_add_builtin(e, "!=", builtin_ne);
		lenv_add_builtin(e, ">", builtin_greater_than);
		lenv_add_builtin(e, "<", builtin_smaller_than);
		lenv_add_builtin(e, "<=", builtin_smaller_than_or_equal_to);
		lenv_add_builtin(e, ">=", builtin_greater_than_or_equal_to);
		lenv_add_builtin(e, "if", builtin_if);
		lenv_add_builtin(e, "&&", builtin_and);
		lenv_add_builtin(e, "||", builtin_or);
		lenv_add_builtin(e, "!", builtin_not);
    
    lenv_add_builtin(e, "def", builtin_def);
		lenv_add_builtin(e, "=", builtin_put);
		lenv_add_builtin(e, "\\", builtin_lambda);
		lenv_add_builtin(e, "load", builtin_load);
		lenv_add_builtin(e, "print", builtin_print);
		lenv_add_builtin(e, "error", builtin_error);
}

lval* builtin_load(lenv* e, lval* a) {
	CHECK_COUNT("load", a, 1);
	CHECK_INPUT_TYPE("load", a, 0, LVAL_STR);

	// parse file
	mpc_result_t r;
	if (mpc_parse_contents(a->cell[0]->str, Lispr, &r)) {
		// read contents
		lval* expr = lval_read(r.output);
		mpc_ast_delete(r.output);

		// evaluate each expression
		while (expr->count) {
			lval* x = lval_eval(e, lval_pop(expr, 0));
			if (x->type == LVAL_ERR) lval_println(e,x);
			lval_del(x);
		}

		// delete expression and arguments
		lval_del(expr); lval_del(a);

		// return empty list to signal correct execution
		return lval_sexpr();
	}

	else {
		// get parse error as string
		char* err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);
		lval* err = lval_err("Could not load library %s", err_msg);
		free(err_msg);
		lval_del(a);
		return err;
	}
}
