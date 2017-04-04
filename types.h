#ifndef TYPES
#define TYPES
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
#endif
