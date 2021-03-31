#include <strings.h>
/*---------------------------------------------------------------------------+
| External parser interface
+---------------------------------------------------------------------------*/
extern void SignalBeginFunction();
extern void SignalArg();
extern void SignalEndFunction();
extern void ParseThis();

extern void yyerror();

/*---------------------------------------------------------------------------+
| Control and escape characters
+---------------------------------------------------------------------------*/
#define CTRL_CHAR '^'
#define BACK_CHAR '\\'

extern void scSetInputBuffer();
extern int yyparse();
