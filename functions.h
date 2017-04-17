#ifndef FUNCTIONS
#define FUNCTIONS
#include "types.h"
#include "mpc.h"
#include "macros.h"
// Internal representation generation
lval* eval(mpc_ast_t*);
lval* lval_read_num(mpc_ast_t*);
lval* lval_read(mpc_ast_t*);

// lval creation
lval* lval_num(Num);
lval* lval_err(char*, ...);
lval* lval_sym(char*);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
lval* lval_fun(lbuiltin);
lval* lval_lambda(lval* formals, lval* body);
lval* lval_bool(int b);

// builtins
lval* builtin_head(lenv*, lval*);
lval* builtin_tail(lenv*, lval*);
lval* builtin_list(lenv*, lval*);
lval* builtin_eval(lenv*, lval*);
lval* builtin_join(lenv*, lval*);
lval* lval_join(lval*,lval*);
lval* builtin_cons(lenv*, lval*);
lval* builtin_len(lenv*, lval*);
lval* builtin_init(lenv*, lval*);
lval* builtin_add(lenv*, lval*);
lval* builtin_sub(lenv*, lval*);
lval* builtin_mul(lenv*, lval*);
lval* builtin_div(lenv*, lval*);
lval* builtin_mod(lenv*, lval*);
lval* builtin_def(lenv*, lval*);
lval* builtin_lambda(lenv*, lval*);
lval* builtin_put(lenv* e, lval* a);
lval* builtin_var(lenv* e, lval* a, char* func);
lval* builtin_eq(lenv* e, lval* a);
lval* builtin_ne(lenv* e, lval* a);
lval* builtin_greater_than(lenv* e, lval* a);
lval* builtin_smaller_than(lenv* e, lval* a);

// Utilities
void lval_del(lval*);
lval* lval_add(lval*, lval*);
void lval_expr_print(lenv* e, lval* v, char open, char close);
void lval_print(lenv* e, lval* v);
void lval_println(lenv* e, lval* v);
lval* lval_eval_sexpr(lenv*, lval*);
lval* eval_op(lval*, char*, lval*);
lval* lval_eval(lenv*, lval*);
lval* lval_pop(lval*, int);
lval* lval_take(lval*, int);
lval* lval_copy(lval*);
lenv* lenv_new(void);
lenv* lenv_copy(lenv* e);
char* ltype_name(int);
int valid_math_input(lval*);
void print_env(lenv* e);
lval* lval_call(lenv* e, lval* f, lval* a);
lval* lval_equals(lval* x, lval* y);
lval* numerical_equals(Num x, Num y);
lval* builtin_cmp(lenv* e, lval* a, char* op);

// Environment functions
void lenv_del(lenv*);
lval* lenv_get(lenv*, lval*);
void lenv_add_builtin(lenv*, char*, lbuiltin);
void lenv_add_builtins(lenv*);
void lenv_put(lenv*, lval*, lval*);
void lenv_def(lenv* e, lval* k, lval* v);
#endif
