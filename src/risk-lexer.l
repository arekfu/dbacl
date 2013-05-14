%{
/* 
 * Copyright (C) 2002 Laird Breyer
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * Author:   Laird Breyer <laird@lbreyer.com>
 */

#include <stdlib.h>
#include <string.h>
#include "bayesol.h"
#include "risk-parser.h"
  int yyerror(char *);
  int current_lineno = 1;
  YY_BUFFER_STATE handle;
%}

EXP                "E"|"e"
DIGIT              [0-9]
DECIMAL            "."
SIGN               "+"|"-"
INF                "INF"|"inf"

NUMBER1            {DIGIT}+{DECIMAL}{DIGIT}*{EXP}{SIGN}?{DIGIT}+|{INF} 
NUMBER2            {DIGIT}+{DECIMAL}{DIGIT}*
NUMBER3            {DIGIT}+
NAME               [A-Za-z][A-Za-z0-9]*
REGEX              \"[^\"\\]*(\\.[^\"\\]*)*\"
VEC                \[[^\[\]]*\]

%%

"categories"       { return tCATEGORIES; }
"loss_matrix"      { return tLOSS; }
"prior"            { return tPRIOR; }
\$[0-9]            { yylval.numval = atoi(yytext + 1); if(yylval.numval > 0) yylval.numval--; return tMATCH; }
"complexity"       { return tCOMPLEXITY; }
"exp"              { return tEXP; }
"log"              { return tLOG; }
[ \t]              /* ignore white space */
[\n]               { current_lineno++; }
^\#.*$             /* ignore comments */
{NUMBER1}          { yylval.numval = strtod(yytext, NULL); return tNUMBER; }
{NUMBER2}          { yylval.numval = strtod(yytext, NULL); return tNUMBER; }
{NUMBER3}          { yylval.numval = strtod(yytext, NULL); return tNUMBER; }
{NAME}             { yylval.strval = strdup(yytext); return tNAME; }
{REGEX}            { yylval.strval = strdup(yytext+1); 
                     yylval.strval[yyleng - 2] = '\0'; return tREGEX; }
{VEC}              { yylval.strval = strdup(yytext+1); 
                     yylval.strval[yyleng -2] = '\0'; return tVEC; }
.                  { return *yytext; }

%%

void reset_lexer() {
  current_lineno = 1;
}

void lexer_prepare_string(char *buf) {
  handle = yy_scan_string(buf);
}

void lexer_free_string() {
  yy_delete_buffer(handle);
}
