#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"
#include "types.h"
#include "functions.h"

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

int main(int argc, char** argv) {
		
    // Create parser
    Number = mpc_new("number");
    Long = mpc_new("long");
    Double = mpc_new("double");
    Symbol = mpc_new("symbol");
		String = mpc_new("string");
		Comment = mpc_new("comment");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          number: <double> | <long>; \
          long: /-?[0-9]+/; \
          double: /-?[0-9]+[.][0-9]*/; \
          symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%^|]+/; \
					string: /\"(\\\\.|[^\"])*\"/; \
					comment: /;[^\\n\\r]*/; \
          sexpr: '(' <expr>* ')'; \
          qexpr: '{' <expr>* '}'; \
          expr: <number> | <symbol> | <string> | <comment> | <sexpr> | \
						<qexpr>; \
          lispr: /^/ <expr>* /$/; \
        ", Number, Long, Double, Symbol, String, Comment, Sexpr, Qexpr, Expr, 
				Lispr);
    
    puts("Lispr Version 0.0.0.0.1");
    puts("Press ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);		
		
		if (argc >= 2) {
			// this means we have been supplied with files to load
			for (int i = 1; i < argc; i++) {
				lval* args = lval_add(lval_sexpr(), lval_str(argv[i]));
				lval* x = builtin_load(e, args);
				if (x->type == LVAL_ERR) lval_println(e,x);
				lval_del(x);
			}
		}
    
    while (1) {
        char* input = readline("lispr> ");
        add_history(input);
        
        // Attempt to parse input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispr, &r)) {
            // On success, evaluate the input
						lval* x = lval_eval(e, lval_read(r.output));
            lval_println(e,x);
            lval_del(x);
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
    lenv_del(e);
    mpc_cleanup(10, Number, Long, Double, Symbol, String, Comment, Sexpr, 
				Qexpr, Expr, Lispr);
    return 0;
}


