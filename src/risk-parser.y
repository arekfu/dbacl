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

#define YYDEBUG 0
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bayesol.h"
  extern char *yytext;
  extern FILE *yyin;
  extern int current_lineno;
  extern int yylex(void);
  extern void reset_lexer();
  extern void lexer_prepare_string(char *buf);
  extern void lexer_free_string();

  extern Spec spec;

  /* defined here */
  void set_loss_and_increment(real_value_t v);
  int parse_loss_vec(category_count_t i, char *buf);
  int parse_risk_spec(FILE *input);
  int yyerror(char *s);
  void add_prior_weight(real_value_t w);
  void add_cat_name(char *n);
  void attach_cat_vec(char *n, char *r, char *v);

  category_count_t x, y;

  %}

%union {
  real_value_t numval;
  char *strval;
}
%token tCATEGORIES
%token tPRIOR
%token tLOSS
%token tEXP
%token tLOG
%token <numval> tNUMBER
%token <numval> tMATCH
%token <numval> tCOMPLEXITY
%token <strval> tNAME
%token <strval> tREGEX
%token <strval> tVEC

%type <numval> catvec priorvec
%type <numval> formula
%type <strval> multivec

%start spec
%left '+'
%left '-'
%left '*'
%left '/'
%right '^'

%%

spec:       catlist priorlist lossmat
          | flist
;

catlist:    tCATEGORIES '{' catvec '}'
;

catvec:     tNAME                             { add_cat_name($1); }
          | catvec ',' tNAME                  { add_cat_name($3); }
;

priorlist:  tPRIOR '{' priorvec '}' 
;

priorvec:   tNUMBER                           { add_prior_weight($1); }
          | priorvec ',' tNUMBER              { add_prior_weight($3); }
;

lossmat:    tLOSS '{' multivec '}'
;

multivec:   tREGEX tNAME tVEC                 { attach_cat_vec($2,$1,$3); }
          | multivec tREGEX tNAME tVEC        { attach_cat_vec($3,$2,$4); }
;

flist:      formula                          { set_loss_and_increment($1); }
          | flist ',' formula                { set_loss_and_increment($3); }
;

formula:    tNUMBER                          { $$ = log($1); }
          | tMATCH                           { $$ = log(spec.loss_list[x]->sm[(int)$1]); }
          | tCOMPLEXITY                      { $$ = log(spec.complexity[x]); }
          | formula '+' formula              { $$ = log(exp($1) + exp($3)); }
          | formula '-' formula              { $$ = log(exp($1) - exp($3)); }
          | formula '*' formula              { $$ = $1 + $3; }
          | formula '/' formula              { $$ = $1 - $3; }
          | formula '^' formula              { $$ = $1 * exp($3); }
          | '(' formula ')'                  { $$ = $2; }
          | tEXP '(' formula ')'            { $$ = exp($3); }
          | tLOG '(' formula ')'            { $$ = log($3); }
;

%%

void set_loss_and_increment(real_value_t v) {
  if( isnan(v) ) {
    fprintf(stderr, "error: negative losses not supported (%s,%s)\n",
	    spec.catname[x], spec.catname[y]);
    exit(0);
  } else {
    spec.loss_matrix[x][y++] = v;
  }
}


int parse_loss_vec(category_count_t i, char *buf) {
  int result;

#if YYDEBUG
  yydebug = 1;
#endif

  reset_lexer();
  x = i;
  y = 0;
  lexer_prepare_string(buf);
  result = yyparse();
  lexer_free_string();
  return result;
}


int parse_risk_spec(FILE *input) {

#if YYDEBUG
  yydebug = 1;
#endif

  yyin = input;
  reset_lexer();
  return yyparse();
}

int yyerror(char *s)
{ 
  fprintf(stderr, "parse error at line %d before '%s'\n", 
	  current_lineno, yytext); 
  return 0;
}

void add_prior_weight(real_value_t w) {
  if( w < 0.0 ) {
    fprintf(stderr, "error: prior can't have negative values (%f)\n", w);
    exit(0);
  } else if(spec.num_priors < MAX_CAT) {
    spec.prior[spec.num_priors++] = log(w);
  } else {
    fprintf(stderr, "warning: maximum reached, prior weight ignored\n");
  }
}

void add_cat_name(char *n) {
  if(spec.num_cats < MAX_CAT) { 
    spec.catname[spec.num_cats++] = n;
  } else {
    fprintf(stderr, "warning: maximum reached, category ignored\n");
  }            
}

void attach_cat_vec(char *n, char *r, char *v) {
  category_count_t i;
  LossVector *p, *q;

  /* see if category name is known */
  for( i = 0; i < spec.num_cats; i++ ) {
    if( strcmp(spec.catname[i], n) == 0 ) {
      break;
    }
  }

  if( i < spec.num_cats ) {

    /* create a new LossVector */
    p = malloc(sizeof(LossVector));
    if( p ) {
      p->re = r;
      p->ve = v;
      p->next = NULL;
    } else {
      fprintf(stderr, 
	      "error: couldn't allocate memory needed for loss matrix\n");
      yyerror(NULL);
    }

    /* add this vector spec to the appropriate list */
    if( spec.loss_list[i] == NULL ) {
      spec.loss_list[i] = p;
    } else {
      for(q = spec.loss_list[i]; q->next != NULL; q = q->next);
      q->next = p;
    }

  } else {
    fprintf(stderr, "error: encountered unknown category\n");
    yyerror(NULL);
  }
}

/* we never parse multiple files */
int yywrap() { return 1; }
