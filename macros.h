#ifndef MACROS
#define MACROS
#include "functions.h"
// Macro for error checking
#define LASSERT(args, cond, fmt, ...)\
    if (!(cond)) { \
			lval* err = lval_err(fmt, ##__VA_ARGS__); \
			lval_del(args); \
			return err; \
    }
#define CHECK_COUNT(fun, arg, num)\
	if (arg->count != num) {\
		lval* err = lval_err("Function '%s' received %d arguments"\
				", expects %d.", fun, arg->count, num);\
		lval_del(arg);\
		return err;	}
#define CHECK_EMPTY(arg, fmt, ...)\
    LASSERT(arg, arg->cell[0]->count != 0, fmt, ##__VA_ARGS__)
#define CHECK_INPUT_TYPE(fun, arg, idx, expected)\
	if (arg->cell[idx]->type != expected) {\
		lval* err = lval_err("Function '%s' passed wrong argument type. "\
				"Expected argument %d to be %s, received %s.",\
				fun, idx, ltype_name(expected), ltype_name(arg->cell[idx]->type));\
		lval_del(arg);\
		return err;	}
#endif
