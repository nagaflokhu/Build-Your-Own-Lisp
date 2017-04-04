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

// Utilities
void lval_del(lval*);
lval* lval_add(lval*, lval*);
void lval_expr_print(lval*, char, char);
void lval_print(lval*);
void lval_println(lval*);
lval* lval_eval_sexpr(lenv*, lval*);
lval* eval_op(lval*, char*, lval*);
lval* lval_eval(lenv*, lval*);
lval* lval_pop(lval*, int);
lval* lval_take(lval*, int);
lval* lval_copy(lval*);
lenv* lenv_new(void);
char* ltype_name(int);
int valid_math_input(lval*);

// Environment functions
void lenv_del(lenv*);
lval* lenv_get(lenv*, lval*);
void lenv_add_builtin(lenv*, char*, lbuiltin);
void lenv_add_builtins(lenv*);
void lenv_put(lenv*, lval*, lval*);
#endif
