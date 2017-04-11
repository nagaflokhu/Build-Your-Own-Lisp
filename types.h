#ifndef TYPES
#define TYPES
// lvals represent the result of evaluating a lisp
// expression
enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, 
      LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR};
enum {LONG, DOUBLE};
enum {TRUE, FALSE};

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

		// Basic
		Num num;
    char* err;
    char* sym;

		// Function
    lbuiltin builtin;
		lenv* env;
		lval* formals;
		lval* body;
    
		// Expression
    int count;
    struct lval** cell;
};

struct lenv {
		lenv* par;
    int count;
    lval** vals;
    char** syms;
};
#endif
