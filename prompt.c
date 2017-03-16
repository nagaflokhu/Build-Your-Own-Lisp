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

long eval(mpc_ast_t*);
long eval_op(long, char*, long);
int count_leaves(mpc_ast_t*); 

int main(int argc, char** argv) {
    
    // Create parser
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispr = mpc_new("lispr");
    
    mpca_lang(MPCA_LANG_DEFAULT,
        "\
          operator: '+' | '/' | '*' | '-' | '%'; \
          number: /-?[0-9]+[.]?[0-9]*/; \
          expr: <number> | '(' <operator> <expr>+ ')'; \
          lispr: /^/ <operator> <expr>+ /$/; \
        ", Operator, Number, Expr, Lispr);
    
    puts("Lispr Version 0.0.0.0.1");
    puts("Press ctrl+c to Exit\n");
    
    // Infinite loop
    while (1) {
        char* input = readline("lispr> ");
        add_history(input);
        
        // Attempt to parse input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispr, &r)) {
            // On success, evaluate the input
            printf("%li\n", eval(r.output));
            mpc_ast_print(r.output);
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
    mpc_cleanup(4, Operator, Number, Expr, Lispr);
    return 0;
}

long eval(mpc_ast_t* t) {
    // if it's a number, return it directly
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }
    
    // the operator is always second child
    char* op = t->children[1]->contents;
    long x = eval(t->children[2]);
    
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    
    return x;
}

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) return x + y;
    if (strcmp(op, "-") == 0) return x - y;
    if (strcmp(op, "*") == 0) return x * y;
    if (strcmp(op, "/") == 0) return x / y;
    if (strcmp(op, "%") == 0) return x % y;
    return 0;
}

int count_leaves(mpc_ast_t* t) {
    mpc_ast_print(t);
    int child_num = t->children_num;
    if (child_num == 0) return 1;
    int total = 0;
    for (int i = 0; i < child_num; i++) {
        total = total + count_leaves(t->children[i]);
    }
    return total;
}