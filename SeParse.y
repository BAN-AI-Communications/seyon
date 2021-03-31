%{
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "SeParse.h"

int yylex(void);

void (*callbackProc)();
%}

%token <sval> WORD

%union {
  char *sval;
}

%start Script

%%

Script		: Script FunCall
		    | Empty
;
FunCall		: FunName '(' ArgList ')' ';'
			{ (*callbackProc)(3, NULL); }
;
FunName		: WORD { (*callbackProc)(1, $1); }
;
ArgList		: Args
  		    | Empty
;
Args	    : Args ',' Arg
		    | Arg
;
Arg		    : WORD { (*callbackProc)(2, $1); }
;
Empty		:
;
%%

void
ParseThis(line, callback)
	 char     *line;
	 void     (*callback)();
{
  callbackProc = callback;
  scSetInputBuffer(line);
  yyparse();
}

void yyerror(char *msg)
{
  (*callbackProc)(4, msg);
}

#ifdef TEST
void SignalBeginFunction(char *name)
{
  printf("** Function call: %s(", name);
}

void SignalArg(char *arg)
{
  char *p = arg;
  printf("\n++Arg: (");
  while (*p) {
    if (isprint(*p))
      putchar(*p);
    else 
      printf("\\0%o", (int)*p);
    p++;
  }
  putchar(')');
}

void SignalEndFunction()
{
  printf("\n)\n");
}

void
main(int argc, char *argv[])
{
  char long_line[1000];

  char input_str[] = "This(is, a, real, funky); script();\n\
            Scripts(); Can(be); Multi(Line, \"Can't they?\");\n\
            Commas(are, no, longer, optional, inside, arglists);\n\
           Scripts(); Can(); contain(\"tabs \\t and backspaces \\b\");\n\
           As(\"Well\\ as Quoted Strings\", and, '\"Quoted Strings inside\n\
           quoted strings\"');\n\
       esc(can, appear, outside, strings, ^z, \\012\\015\\n)\n\
           But(parenthesis, should, match);\n\
  We(\"have a funny way of specifying \\012 chars and even)\"); \n\
       backslashes( \" \\\\ \");\n\
  new(\"in this version are ^m and ^A ctr-escapes, as in ^S^Q\");\n\
 The(next, line, will, give, a, syntax, error, because, it, has, two, adj, functions,\n\
       without, a, separating, semicolon);\n\
 End() script()";

  printf("------ String to parse: \n%s\n\n---- Parsing begins:\n", input_str);
  strcpy(long_line, input_str);
  ParseThis(long_line);
  strcpy(long_line, input_str);
  ParseThis(long_line);
}
#endif TEST
