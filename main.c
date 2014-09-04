#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>

#include "mpc.h"
#include "value.h"

#define TL_ASSERT(args, cond, err) if(!(cond)) { tl_val_delete(args); return tl_val_error(err); }

VAL tl_val_error(char*);
VAL tl_val_read(mpc_ast_t*);
VAL tl_val_pop(VAL, int);
VAL tl_val_take(VAL, int);
VAL tl_val_eval(VAL);
VAL tl_val_join(VAL, VAL);

void tl_val_print(VAL);
void tl_val_print_expr(VAL, char, char);
void tl_val_delete(VAL);

VAL builtin(VAL, char*);
VAL builtin_op(VAL, char*);
VAL builtin_list(VAL);
VAL builtin_head(VAL);
VAL builtin_tail(VAL);
VAL builtin_eval(VAL);
VAL builtin_join(VAL);

int main(int argc, char** argv) {

  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol   = mpc_new("symbol");
  mpc_parser_t* Sexpr    = mpc_new("sexpr");
  mpc_parser_t* Qexpr    = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Tinylisp = mpc_new("tinylisp");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                             \
      number   : /-?[0-9]+/ ;                     \
      symbol   : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
      sexpr    : '(' <expr>* ')' ;                \
      qexpr    : '{' <expr>* '}' ;                \
      expr     : <number> | <symbol> | <sexpr> | <qexpr>;  \
      tinylisp : /^/ <expr>* /$/ ;                \
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
      VAL x = tl_val_eval(tl_val_read(r.output));
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

VAL tl_val_error(char* m) {
  VAL v = malloc(sizeof(TLVAL));
  v->type = TL_ERROR;
  v->err = malloc(strlen(m)+1);
  strcpy(v->err, m);
  return v;
}

VAL tl_val_symbol(char* s) {
  VAL v = malloc(sizeof(TLVAL));
  v->type = TL_SYMBOL;
  v->sym = malloc(strlen(s)+1);
  strcpy(v->sym, s);
  return v;
}

VAL tl_val_sexpr(void) {
  VAL v = malloc(sizeof(TLVAL));
  v->type = TL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

VAL tl_val_qexpr(void) {
  VAL v = malloc(sizeof(TLVAL));
  v->type = TL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void tl_val_print(VAL v) {
  switch (v->type) {
    case TL_INTEGER:
      printf("%ld", v->num);
      break;

    case TL_ERROR:
      printf("Error: %s.", v->err);
      break;

    case TL_SYMBOL:
      printf("%s", v->sym);
      break;

    case TL_SEXPR:
      tl_val_print_expr(v, '(', ')');
      break;

    case TL_QEXPR:
      tl_val_print_expr(v, '{', '}');
      break;
  }
}

void tl_val_delete(VAL v) {
  switch(v->type) {
    case TL_INTEGER: break;
    case TL_ERROR:   free(v->err); break;
    case TL_SYMBOL:  free(v->sym); break;

    case TL_QEXPR:
    case TL_SEXPR:
      for(int i=0; i < v->count; i++) tl_val_delete(v->cell[i]);
      free(v->cell);
      break;
  }
  free(v);
}

VAL tl_val_add(VAL v, VAL x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(VAL) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

VAL tl_val_read_integer(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? tl_val_num(x) : tl_val_error("Invalid number");
}

VAL tl_val_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number")) { return tl_val_read_integer(t); }
  if (strstr(t->tag, "symbol")) { return tl_val_symbol(t->contents); }

  VAL x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = tl_val_sexpr();  }
  if (strstr(t->tag, "sexpr")) { x = tl_val_sexpr(); }
  if (strstr(t->tag, "qexpr")) { x = tl_val_qexpr(); }

  for(int i = 0; i<t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) continue;
    if (strcmp(t->children[i]->contents, ")") == 0) continue;
    if (strcmp(t->children[i]->contents, "{") == 0) continue;
    if (strcmp(t->children[i]->contents, "}") == 0) continue;
    if (strcmp(t->children[i]->tag,  "regex") == 0) continue;

    x = tl_val_add(x, tl_val_read(t->children[i]));
  }
  return x;
}

void tl_val_print_expr(VAL v, char open, char close) {
  putchar(open);
  putchar(' ');
  for(int i=0; i<v->count; i++) {
    tl_val_print(v->cell[i]);
    if (i != (v->count - 1)) putchar(' ');
  }
  putchar(' ');
  putchar(close);
}

VAL tl_val_eval_sexpr(VAL v) {
  for(int i=0; i < v->count; i++)
    v->cell[i] = tl_val_eval(v->cell[i]);

  for(int i=0; i < v->count; i++)
    if (v->cell[i]->type == TL_ERROR)
      return tl_val_take(v, i);

  if (v->count == 0) return v;

  if (v->count == 1) return tl_val_take(v, 0);

  VAL f = tl_val_pop(v, 0);
  if (f->type != TL_SYMBOL) {
    tl_val_delete(f);
    tl_val_delete(v);
    return tl_val_error("S-expression does not start with a symbol");
  }

  VAL result = builtin(v, f->sym);
  tl_val_delete(f);
  return result;
}

VAL tl_val_eval(VAL v) {
  if (v->type == TL_SEXPR) return tl_val_eval_sexpr(v);
  return v;
}

VAL tl_val_pop(VAL v, int i) {
  VAL x = v->cell[i];
  memmove(&v->cell[i], &v->cell[i+1], sizeof(VAL)*(v->count-i-1));
  v->count--;
  v->cell = realloc(v->cell, sizeof(VAL)*v->count);
  return x;
}

VAL tl_val_take(VAL v, int i) {
  VAL x = tl_val_pop(v, i);
  tl_val_delete(v);
  return x;
}

VAL builtin(VAL v, char* f) {
  if (strcmp("list", f) == 0) return builtin_list(v);
  if (strcmp("head", f) == 0) return builtin_head(v);
  if (strcmp("tail", f) == 0) return builtin_tail(v);
  if (strcmp("join", f) == 0) return builtin_join(v);
  if (strcmp("eval", f) == 0) return builtin_eval(v);
  if (strstr("+-*/", f))      return builtin_op(v, f);

  tl_val_delete(v);
  return tl_val_error("Unknown function called!");
}

VAL builtin_op(VAL a, char* op) {
  for (int i=0; i < a->count; i++) {
    if(a->cell[i]->type != TL_INTEGER) {
      tl_val_delete(a);
      return tl_val_error("Cannot operate on non-number");
    }
  }

  VAL x = tl_val_pop(a, 0);

  if(strcmp(op, "-") && a->count == 0)
    x->num = -x->num;

  while (a->count > 0) {
    VAL y = tl_val_pop(a, 0);

    if(strcmp(op, "+") == 0) x->num += y->num;
    if(strcmp(op, "-") == 0) x->num -= y->num;
    if(strcmp(op, "*") == 0) x->num *= y->num;

    if(strcmp(op, "/") == 0) {
      if (y->num == 0) {
        tl_val_delete(x);
        tl_val_delete(y);
        x = tl_val_error("Divide by zero");
        break;
      }
      x->num /= y->num;
    }
  }

  tl_val_delete(a);
  return x;
}

VAL builtin_list(VAL v) {
  v->type = TL_QEXPR;
  return v;
}

VAL builtin_head(VAL v) {
  TL_ASSERT(v, (v->count == 1), "Function 'head' passed too many arguments.");
  TL_ASSERT(v, (v->cell[0]->type == TL_QEXPR), "Function 'head' passed invalid types.");
  TL_ASSERT(v, (v->cell[0]->count != 0), "Function 'head' passed empty list");

  VAL x = tl_val_take(v, 0);
  while(x->count > 1) tl_val_delete(tl_val_pop(x, 1));
  return x;
}

VAL builtin_tail(VAL v) {
  TL_ASSERT(v, (v->count == 1), "Function 'tail' passed too many arguments");
  TL_ASSERT(v, (v->cell[0]->type == TL_QEXPR), "Function 'tail' passed invalid type");
  TL_ASSERT(v, (v->cell[0]->count != 0), "Function 'tail' passed empty list");

  VAL x = tl_val_take(v, 0);
  tl_val_delete(tl_val_pop(x, 0));
  return x;
}

VAL builtin_eval(VAL v) {
  TL_ASSERT(v, (v->count == 1), "Function 'eval' passed too many arguments");
  TL_ASSERT(v, (v->cell[0]->type == TL_QEXPR), "Function 'eval' passed invalid types");

  VAL x = tl_val_take(v, 0);
  x->type = TL_SEXPR;
  return tl_val_eval(x);
}

VAL builtin_join(VAL v) {
  for(int i=0; i < v->count; i++) {
    TL_ASSERT(v, (v->cell[0]->type == TL_QEXPR), "Function 'join' passed invalid type");
  }

  VAL x = tl_val_pop(v, 0);
  while(v->count > 0) x = tl_val_join(x, tl_val_pop(v, 0));
  tl_val_delete(v);
  return x;
}

VAL tl_val_join(VAL x, VAL y) {
  while (y->count)
    x = tl_val_add(x, tl_val_pop(y, 0));
  tl_val_delete(y);
  return x;
}

