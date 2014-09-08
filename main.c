#include <stdio.h>
#include <editline/readline.h>

#include "mpc.h"
#include "value.h"

int main(int argc, char** argv) {

  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol   = mpc_new("symbol");
  mpc_parser_t* Sexpr    = mpc_new("sexpr");
  mpc_parser_t* Qexpr    = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Tinylisp = mpc_new("tinylisp");

  mpca_lang(MPCA_LANG_DEFAULT,
    " \
      number   : /-?[0-9]+/ ; \
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
      sexpr    : '(' <expr>* ')' ; \
      qexpr    : '{' <expr>* '}' ; \
      expr     : <number> | <symbol> | <sexpr> | <qexpr>; \
      tinylisp : /^/ <expr>* /$/ ; \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Tinylisp);

  mpc_result_t r;

  puts("Tinylisp Version 0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while(1) {
    char* input = readline("tinylisp> ");
    add_history(input);

    if (strcmp(input, "exit") == 0) return 0;

    if (mpc_parse("<stdin>", input, Tinylisp, &r)) {
      Value* x = tl_val_eval(tl_val_read(r.output));
      tl_val_print(x);
      puts("");
      tl_val_delete(x);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Tinylisp);

  return 0;
}

