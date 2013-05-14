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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 * 
 * Author:   Laird Breyer <laird@lbreyer.com>
 */

/* 
 * Note to regenerate the Makefile and configure script:
 * aclocal
 * autoconf
 * touch NEWS README AUTHORS ChangeLog
 * automake -a
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#endif

#include <locale.h>

#if defined HAVE_MBRTOWC

#include <wctype.h>
#include <wchar.h>

#endif


#include <sys/types.h>
#include <regex.h>

#include "util.h"
#include "bayesol.h" 

/* global variables */

extern options_t u_options;
extern options_t m_options;

extern char *progname;
extern char *inputfile;
extern long inputline;

Spec spec; /* used in risk-parser */

extern char *textbuf;
extern charbuf_len_t textbuf_len;

extern char *aux_textbuf;
extern charbuf_len_t aux_textbuf_len;

#if defined HAVE_MBRTOWC
extern wchar_t *wc_textbuf;
extern charbuf_len_t wc_textbuf_len;
#endif

/* for option processing */
extern char *optarg;
extern int optind, opterr, optopt;

options_t options = 0;
char *title = "";

/* parser interface */
extern int parse_loss_vec(category_count_t i, char *buf);
extern int parse_risk_spec(FILE *input);

bool_t found_scores = 0;
extern int cmd;
int exit_code = 0; /* default */

/***********************************************************
 * MISCELLANEOUS FUNCTIONS                                 *
 ***********************************************************/

static void usage(char **argv) {
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "bayesol [-vinN] -c RISKSPEC [FILE]...\n");
  fprintf(stderr, 
	  "      calculates the optimal Bayes solution using RISKSPEC, with\n");
  fprintf(stderr, 
	  "      input from FILE or STDIN.\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "bayesol -V\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      prints program version.\n");
}

void print_line() {
  fprintf(stdout, "%s", textbuf);
}

void init_spec() {
  category_count_t i;

  spec.num_cats = 0;
  spec.num_priors = 0;

  for( i = 0; i < MAX_CAT; spec.loss_list[i++] = NULL);
  for( i = 0; i < MAX_CAT; spec.cross_entropy[i++] = 0.0);
  for( i = 0; i < MAX_CAT; spec.complexity[i++] = 0.0);

}

/***********************************************************
 * PARSING RELATED FUNCTIONS                               *
 ***********************************************************/

/* this function is called before processing each file.
   It constructs a linked list of regular expressions if
   there are any. Each regex is associated with the string 
   representation of the corresponding loss vector */
void setup_regexes() {
  category_count_t i;
  bool_t has_empty_string;
  LossVector *p;
  RegMatch *q, *r;

  for( i = 0; i < spec.num_cats; i++ ) {
    /* if spec.loss_list[i] does not contain a 
       LossVector whose regex is the empty string, 
       then this specification is not complete and we exit */
    if( spec.loss_list[i] == NULL ) {
      errormsg(E_FATAL,
	       "missing loss_matrix entry for category %s\n", 
	       spec.catname[i]);
    } else {
      has_empty_string = 0;
      /* step through the list of vectors looking for regexes */
      for(p = spec.loss_list[i]; p != NULL; p = p->next) {
	if( !(p->re[0]) ) {
	  has_empty_string = 1;
	  p->found = 1;
	} else {
	  /* it's a genuine regex, try compiling it */
	  q = malloc(sizeof(RegMatch));
	  if(q) {
	    /* GNU regexes accept ordinary strings */
	    if( regcomp(&(q->reg), p->re, REG_EXTENDED) == 0 ) {
	      p->found = 0;
	      q->lv = p;
	      q->next = NULL;
	    } else {
	      errormsg(E_WARNING,
		      "couldn't compile regex '%s', ignoring\n",
		      p->re);
	      free(q);
	      q = NULL;
	    }
	  } else {
	    errormsg(E_WARNING,
		    "no memory for regex '%s', ignoring\n",
		    p->re);
	  }
	  /* add this regex to the list we have to check online */	  
	  if( spec.regs == NULL ) {
	    spec.regs = q;
	  } else {
	    for(r = spec.regs; r->next != NULL; r = r->next);
	    r->next = q;
	  }
	}
      }
      /* consistency check */
      if( !has_empty_string ) {
	errormsg(E_FATAL,
		"missing loss_matrix entry for category %s (need \"\" case)\n", 
		spec.catname[i]);
      }
    }
  }
}

/* parses the dbacl scores. Format used is 
   scores [cat s * c]... 
   Note: destroys the buf */
int parse_dbacl_scores(char *buf) {
  char *tok;
  category_count_t index, counter;
  

  /* first token is scores */
  tok = strtok(buf, "# \n");

  counter = 0;
  /* next gobble up tokens four at a time */
  while(tok) {
    /* first we get the category name/index */
    if( !(tok = strtok(NULL, " \n")) ) {
      break;
    } else {
      for(index = 0; index < spec.num_cats; index++) {
	if( strcmp(spec.catname[index], tok) == 0 ) {
	  break;
	}
      }
      if( index == spec.num_cats ) {
	return 0;
      }
    }
    /* now that we have the index, associate the cross entropy */
    if( !(tok = strtok(NULL, " \n")) ) {
      errormsg(E_ERROR, "scores in wrong format\n");
      return 0;
    } else {
      spec.cross_entropy[index] = strtod(tok, NULL);
    }
    /* now skip the '*' */
    if( !(tok = strtok(NULL, " \n")) ) {
      errormsg(E_ERROR, "scores in wrong format\n");
      return 0;
    }
    /* now get the complexity */
    if( !(tok = strtok(NULL, " \n")) ) {
      errormsg(E_ERROR, "scores in wrong format\n");
      return 0;
    } else {
      spec.complexity[index] = strtod(tok, NULL);
    }
    counter++;
  }

  /* last consistency check */
  if( counter > spec.num_cats ) {
    return 0;
  } else {
    found_scores = 1;
    return 1;
  }
}


void finish_parsing() {
  category_count_t i;
  LossVector *p;
  real_value_t min_complexity, max_complexity;

  /* consistency checks */
  if( !found_scores ) {
    errormsg(E_FATAL,
	    "no scores found. Did you use dbacl with the -a switch?\n");
  } else {
    min_complexity = spec.complexity[0];
    max_complexity = spec.complexity[0];
    for(i = 0; i < spec.num_cats; i++) {
      if( !spec.catname[i] ) {
	errormsg(E_FATAL,
		"too few categories scored. Is your risk specification correct?\n");
	exit(0);
      }
      min_complexity = (min_complexity < spec.complexity[i]) ? min_complexity : 
	spec.complexity[i];
      max_complexity = (max_complexity > spec.complexity[i]) ? max_complexity : 
	spec.complexity[i];
    }

    if( min_complexity < MEANINGLESS_THRESHOLD * max_complexity ) {
      errormsg(E_WARNING,
	      "\n"
	      "         There is a significant disparity between the complexities\n"
	      "         reported under each category. This most likely indicates that\n"
	      "         you've chosen categories whose features aren't comparable.\n"
	      "\n"
	      "         The Bayes solution calculations will be meaningless.\n\n");
    } 
  }

  /* first, we must finish building the loss matrix
     We pick for each category the first lossvector
     which reported a match */
  for(i = 0; i < spec.num_cats; i++) {
    for(p = spec.loss_list[i]; p != NULL; p = p->next) {
      if( p->found ) {
	break;
      }
    }
    if( p == NULL ) {
      errormsg(E_ERROR,
	       "something's wrong with loss_list.\n");
    } else {
      /* now build the ith row of the loss matrix */
      if( options & (1<<OPTION_DEBUG) ) {
	fprintf(stdout,
		"category %s\t risk spec: %s\n", 
		spec.catname[i], p->ve);
      }
      if( parse_loss_vec(i, p->ve) != 0 ) {
	errormsg(E_FATAL,
		"couldn't parse spec for (%s, \"%s\")\n", 
		 spec.catname[i], p->re);
      }
    }
  }

  if( options & (1<<OPTION_SCORES_EX) ) {
    /* modify final loss matrix for cost' function. See explanation in
       costs.ps, but note we don't change loss_matrix[i][j] for i != j
       as we should.  The correction is loss_matrix'[i][j] =
       log(exp(loss_matrix[i][j] + 1), which has no effect for large values
       of loss_matrix[i][j]. For small values, the loss is irrelevant anyway. */
    for(i = 0; i < spec.num_cats; i++) {
      if( isinf(spec.loss_matrix[i][i]) ) {
	spec.loss_matrix[i][i] = 0.0;
      }
    }
  }
  
}

int compare_reals(const void *a, const void *b) {
  return (*((real_value_t *)a) > *((real_value_t *)b)) ? -1 : 1;
} 

/* now we are ready to score the losses 
   This is a simple matrix multiplication, but
   because the entries are all exponentials with
   enormously varying exponents, the simple calculation
   fails due to massive precision problems. 
   We approximate the true minimization by comparing
   the magnitude of the individual terms. The scores output
   via OPTION_SCORES are only approximate.
*/
category_count_t score_losses() {
  category_count_t i, j;
  real_value_t tmp[MAX_CAT], min_tmp[MAX_CAT];
  real_value_t norm, score, min_score;
  category_count_t min_cat;

  if( options & (1<<OPTION_DEBUG) ) {
    fprintf(stdout, "\nprior (log scale)\n");
    for(i = 0; i < spec.num_cats; i++) {
      fprintf(stdout, "%8.2f ", spec.prior[i]);
    }
    fprintf(stdout, "\nfinal loss_matrix (log scale)\n");
    for(i = 0; i < spec.num_cats; i++) {
      fprintf(stdout, "%s\t", spec.catname[i]);
      for(j = 0; j < spec.num_cats; j++) {
	fprintf(stdout, "%8.2f ", spec.loss_matrix[i][j]);
      }
      fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
  }

  min_score = 1.0/0.0;
  min_cat = 0;
  for(j = 0; j < spec.num_cats; j++) {
    min_tmp[j] = min_score;
  }

  for(i = 0; i < spec.num_cats; i++) {
    norm = -1.0/0.0;

    for(j = 0; j < spec.num_cats; j++) {
      tmp[j] = 	- spec.complexity[j] * spec.cross_entropy[j] +
	spec.prior[j];
      if( !isinf(tmp[j]) ) {
	tmp[j] += spec.loss_matrix[j][i]; 
	if( !isinf(tmp[j]) && (norm < tmp[j]) ) {
	  norm = tmp[j];
	}
      }
    }    
    if( isinf(norm) ) { norm = 0.0; } 

    score = 0.0;
    for(j = 0; j < spec.num_cats; j++) {
      score += exp( tmp[j] - norm );
    }
    score = log(score) + norm;

    qsort(tmp, spec.num_cats, sizeof(real_value_t), compare_reals);
    /* norm no longer needed */
    for(j = 0; j < spec.num_cats; j++) {
      norm = (tmp[j] != min_tmp[j]) ? (tmp[j] - min_tmp[j]) : 0;
      if( norm > 0 ) {
	break;
      } else if( norm < 0 ) {
	memcpy(min_tmp, tmp, sizeof(real_value_t) * spec.num_cats);
	min_cat = i;
	min_score = min_tmp[0]; /* norm; */
      } else {
	/* they're equal, continue loop */
      }
    }

    if( options & (1<<OPTION_DEBUG) ) {
      fprintf(stdout,
	      "decision score %s\t SumExp(", spec.catname[i]);
      for(j = 0; j < spec.num_cats; j++) {
	fprintf(stdout,
		"%g%s", tmp[j], (((j+1) < spec.num_cats) ? ", " : ")\n"));
      }
    } else if( options & (1<<OPTION_SCORES) ) {
      fprintf(stdout,
	      "%s %10.2f ", spec.catname[i], score);
    }

  }    

  if( options & (1<<OPTION_SCORES) ) {
    fprintf(stdout,"\n");
  }

  if( options & (1<<OPTION_SCORES_EX) ) {
      fprintf(stdout,
	      "%s %10.2f ", spec.catname[min_cat], min_score);
  }

  return min_cat;
}


void finish_parsing_and_score() {
  category_count_t best, i, j;
  real_value_t finfinity;

  finish_parsing();
  best = score_losses();

  if( spec.num_cats == 1 ) {
    errormsg(E_WARNING, "only one category specified, this is trivial!\n");
  }

  if( (options & (1<<OPTION_VERBOSE)) &&
      !(options & (1<<OPTION_SCORES)) &&
      !(options & (1<<OPTION_SCORES_EX)) ) {
    fprintf(stdout, "%s\n", spec.catname[best]);
  }

  exit_code = (best + 1);

  if( options & (1<<OPTION_SCORES_EX) ) {

    finfinity = -log(0.0);

    for(i = 1; i < spec.num_cats; i++) {

      for(j = 0; j < spec.num_cats; j++) {
	spec.loss_matrix[best][j] = -finfinity;
	spec.loss_matrix[j][best] = -finfinity;
      }
      spec.loss_matrix[best][best] = +finfinity;

      best = score_losses();
    }

    fprintf(stdout,"\n");

  }

}

/***********************************************************
 * FILE MANAGEMENT FUNCTIONS                               *
 ***********************************************************/

/* this function parses the risk specification */
int read_riskspec(char *filename) {
  FILE *input;

  /* parse the spec */
  if( (input = fopen(filename, "rb")) ) {

    if( parse_risk_spec(input) != 0 ) {
      fclose(input);
      exit(0);
    }

    fclose(input);
  } else {
    return 0;
  }

  /* do some consistency checks */
  if( spec.num_cats == 0 ) {
    errormsg(E_FATAL,"you need at least one category\n");
  } else if( spec.num_cats != spec.num_priors ) {
    errormsg(E_FATAL, "prior doesn't match number of categories\n");
  }

  return 1;
}



/***********************************************************
 * MULTIBYTE FILE HANDLING FUNCTIONS                       *
 * this is suitable for any locale whose character set     *
 * encoding doesn't include NUL bytes inside characters    *
 ***********************************************************/

#define MAGIC "# scores "
#define MULTIBYTE_EPSILON 10 /* enough for a multibyte char and a null char */


/* reads a text file as input, and applies several
   filters. */
void b_process_file(FILE *input, 
		  void (*line_fun)(void)) {
  category_count_t i;
  RegMatch *r;
  regmatch_t pmatch[MAX_SUBMATCH];
  submatch_order_t z;
  int extra_lines = 2;

  /* now start processing */
  while( fill_textbuf(input, &extra_lines) ) {

    /* now summarize this line if required */
    if( line_fun ) { (*line_fun)(); }


    if( (textbuf[0] == '#') && 
	(strncmp(MAGIC, textbuf, 7) == 0) ) {
      if( !parse_dbacl_scores(textbuf) ) {
	errormsg(E_FATAL,"scores don't match risk specification\n");
      } else if( options & (1<<OPTION_DEBUG) ) {
	for(i = 0; i < spec.num_cats; i++) {
	  fprintf(stdout, 
		  "category %s\t cross_entropy %7.2f complexity %7.0f\n",
		  spec.catname[i], spec.cross_entropy[i], spec.complexity[i]);
	}
	fprintf(stdout, "\n");
      }
    } else {
      /* for each regex in our list, try for a match */
      for( r = spec.regs; r != 0; r = r->next) {
	if( regexec(&(r->reg), textbuf, MAX_SUBMATCH, pmatch, 0) == 0 ) {
	  r->lv->found = 1;
	  /* convert each submatch to a number - pad remaining
	     elements to zero */
	  for(z = 1; z < MAX_SUBMATCH; z++) {
	    if(pmatch[z].rm_so > -1) {
	      r->lv->sm[z-1] = strtod(textbuf + pmatch[z].rm_so, NULL);
	    } else {
	      r->lv->sm[z-1] = 0.0;
	    }
	  }

	  if( options & (1<<OPTION_DEBUG) ) {
	    fprintf(stdout, 
		    "match \"%s\"", r->lv->re);
	    for(z = 1; (z < MAX_SUBMATCH) && (pmatch[z].rm_so > -1); z++) {
	      fprintf(stdout,
		      " %f", r->lv->sm[z-1]);
	    }
	    fprintf(stdout, "\n");
	  }
	}
      }
    }

  }
}

/***********************************************************
 * WIDE CHARACTER FILE HANDLING FUNCTIONS                  *
 * this is needed for any locale whose character set       *
 * encoding can include NUL bytes inside characters        *
 *                                                         *
 * Actually, at present this is quite useless. But it might*
 * prove handy in the future.                              *
 ***********************************************************/

#if defined HAVE_MBRTOWC


/* reads a text file as input, converting each line
   into a wide character representation and applies several
   filters. */
void w_b_process_file(FILE *input, 
		    void (*line_fun)(void)) {

  mbstate_t input_shiftstate;

  category_count_t i;
  RegMatch *r;
  regmatch_t pmatch[MAX_SUBMATCH];
  submatch_order_t z;
  int extra_lines = 2;

  memset(&input_shiftstate, 0, sizeof(mbstate_t));

  while( fill_textbuf(input, &extra_lines) ) {

    if( !fill_wc_textbuf(textbuf, &input_shiftstate) ) {
      continue;
    }

    /* now summarize this line if required */
    if( line_fun ) { (*line_fun)(); }

    /* the scores are written by dbacl, so there's no need for the conversion */
    if( (textbuf[0] == 's') && 
	(strncmp(MAGIC, textbuf, 7) == 0) ) {
      if( !parse_dbacl_scores(textbuf) ) {
	errormsg(E_FATAL, "scores don't match risk specification\n");
      } else if( options & (1<<OPTION_DEBUG) ) {
	for(i = 0; i < spec.num_cats; i++) {
	  fprintf(stdout, 
		  "category %s\t cross_entropy %7.2f complexity %7.0f\n",
		  spec.catname[i], spec.cross_entropy[i], spec.complexity[i]);
	}
	fprintf(stdout, "\n");
      }
    } else {
      /* for each regex in our list, try for a match */
      for( r = spec.regs; r != 0; r = r->next) {
	if( regexec(&(r->reg), textbuf, MAX_SUBMATCH, pmatch, 0) == 0 ) {
	  r->lv->found = 1;
	  /* convert each submatch to a number - pad remaining
	     elements to zero */
	  for(z = 1; z < MAX_SUBMATCH; z++) {
	    if(pmatch[z].rm_so > -1) {
	      r->lv->sm[z-1] = strtod(textbuf + pmatch[z].rm_so, NULL);
	    } else {
	      r->lv->sm[z-1] = 0.0;
	    }
	  }

	  if( options & (1<<OPTION_DEBUG) ) {
	    fprintf(stdout, 
		    "match \"%s\"", r->lv->re);
	    for(z = 1; (z < MAX_SUBMATCH) && (pmatch[z].rm_so > -1); z++) {
	      fprintf(stdout,
		      " %f", r->lv->sm[z-1]);
	    }
	    fprintf(stdout, "\n");
	  }
	}
      }
    }

  }
}

#endif /* HAVE_WCHAR_H */

/***********************************************************
 * MAIN FUNCTIONS                                          *
 ***********************************************************/

int main(int argc, char **argv) {

  FILE *input;
  signed char op;

  void (*preprocess_fun)(void) = NULL;
  void (*line_fun)(void) = NULL;
  void (*postprocess_fun)(void) = NULL;

  progname = "bayesol";
  inputfile = "stdin";
  inputline = 0;

  /* set up internationalization */
  if( !setlocale(LC_ALL, "") ) {
    errormsg(E_WARNING,
	    "could not set locale, internationalization disabled\n");
  } else {
    if( options & (1<<OPTION_DEBUG) ) {
      errormsg(E_WARNING,
	      "international locales not supported\n");
    }
  }

  /* parse the options */
  while( (op = getopt(argc, argv, "DVvinNc:")) > -1 ) {

    switch(op) {
    case 'V':
      fprintf(stdout, "bayesol version %s\n", VERSION);
      fprintf(stdout, COPYBLURB, "bayesol");
      exit(1);
      break;
    case 'n':
      options |= (1<<OPTION_SCORES);
      break;
    case 'N':
      options |= (1<<OPTION_SCORES_EX);
      break;
    case 'i':
      options |= (1<<OPTION_I18N);
      break;
    case 'c':
      if( *optarg && read_riskspec(optarg) ) {
	options |= (1<<OPTION_RISKSPEC);
      } else {
	errormsg(E_FATAL,"could not read %s, program aborted\n", optarg); 
      }
      break;
    case 'v':
      options |= (1<<OPTION_VERBOSE);
      break;
    case 'D':
      options |= (1<<OPTION_DEBUG);
      break;
    default:
      break;
    }
  }

  /* end option processing */
    
  /* consistency checks */
  if( !(options & (1<<OPTION_RISKSPEC)) ){
    errormsg(E_ERROR,"please use -c option\n");
    usage(argv);
    exit(0);
  }

  if( (options & (1<<OPTION_SCORES)) &&
      (options & (1<<OPTION_SCORES_EX)) ) {
    errormsg(E_WARNING,
	    "option -n is incompatible with -N, ignoring\n");
    options &= ~(1<<OPTION_SCORES);
  }


  /* set up callbacks */


  if( options & (1<<OPTION_RISKSPEC) ) {

    preprocess_fun = setup_regexes;
    line_fun = NULL; /* print_line; */
    postprocess_fun = finish_parsing_and_score;

  } else { /* something wrong ? */
    usage(argv);
    exit(0);
  }

  if( preprocess_fun ) { (*preprocess_fun)(); }

  /* preallocate primary text holding buffer */
  textbuf_len = BUFLEN;
  textbuf = malloc(textbuf_len);

  /* now process each file on the command line,
     or if none provided read stdin */
  while( (optind > -1) && *(argv + optind) ) {
    /* if it's a filename, process it */
    if( (input = fopen(argv[optind], "rb")) ) {
      inputfile = argv[optind];
      options |= (1<<INPUT_FROM_CMDLINE);

      if( options & (1<<OPTION_DEBUG) ) {
	fprintf(stdout, "processing file %s\n", argv[optind]);
      }

      if( !(options & (1<<OPTION_I18N)) ) {
	b_process_file(input, line_fun);
      } else {
#if defined HAVE_MBRTOWC
	w_b_process_file(input, line_fun);
#endif
      }

      fclose(input);

    } else { /* unrecognized file name */

      errormsg(E_ERROR, "couldn't open %s\n", argv[optind]);
      usage(argv);
      exit(0);

    }
    optind++;
  }
  /* in case no files were specified, get input from stdin */
  if( !(options & (1<<INPUT_FROM_CMDLINE)) &&
      (input = fdopen(fileno(stdin), "rb")) ) {

    if( options & (1<<OPTION_DEBUG) ) {
      fprintf(stdout, "taking input from stdin\n");
    }

    if( !(options & (1<<OPTION_I18N)) ) {
      b_process_file(input, line_fun);
    } else {
#if defined HAVE_MBRTOWC
      w_b_process_file(input, line_fun);
#endif
    }

    fclose(input);
  }
  
  if( postprocess_fun ) { (*postprocess_fun)(); }

  /* free some global resources */
  free(textbuf);

  exit(exit_code);
}
