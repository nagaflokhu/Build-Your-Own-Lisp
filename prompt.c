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
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Long = mpc_new("long");
    mpc_parser_t* Double = mpc_new("double");
    mpc_parser_t* Symbol = mpc_new("symbol");
		mpc_parser_t* String = mpc_new("string");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          number: <double> | <long>; \
          long: /-?[0-9]+/; \
          double: /-?[0-9]+[.][0-9]*/; \
          symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%^|]+/; \
					string: /\"(\\\\.|[^\"])*\"/; \
          sexpr: '(' <expr>* ')'; \
          qexpr: '{' <expr>* '}'; \
          expr: <number> | <symbol> | <string> | <sexpr> | <qexpr>; \
          lispr: /^/ <expr>* /$/; \
        ", Number, Long, Double, Symbol, String, Sexpr, Qexpr, Expr, Lispr);
    
    puts("Lispr Version 0.0.0.0.1");
    puts("Press ctrl+c to Exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    
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
    mpc_cleanup(8, Number, Long, Double, Symbol, Sexpr, Qexpr, Expr, Lispr);
    return 0;
}


