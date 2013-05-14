/* 
 * Copyright (C) 2005 Laird Breyer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#endif

#include <locale.h>

#if defined HAVE_LANGINFO_H
#include <langinfo.h>
#if !defined CODESET
/* on OpenBSD, CODESET doesn't seem to be defined - 
   we use 3, which should be US-ASCII, but it's not ideal... */
#define CODESET 3
#endif
#endif

#include "util.h" 
#include "dbacl.h" 
#include "hypex.h"


/* global variables */

extern hash_bit_count_t default_max_hash_bits;
extern hash_count_t default_max_tokens;

extern hash_bit_count_t default_max_grow_hash_bits;
extern hash_count_t default_max_grow_tokens;

hash_bit_count_t decimation;

extern options_t u_options;
extern options_t m_options;
extern char *extn;

extern token_order_t ngram_order; /* defaults to 1 */

/* for option processing */
extern char *optarg;
extern int optind, opterr, optopt;

extern char *textbuf;
extern charbuf_len_t textbuf_len;

extern char *progname;
extern char *inputfile;
extern long inputline;

int exit_code = 0; /* default */
int overflow_warning = 0;

score_t default_stepsize = 0.1;

extern long system_pagesize;

extern void *in_iobuf;
extern void *out_iobuf;


/***********************************************************
 * MISCELLANEOUS FUNCTIONS                                 *
 ***********************************************************/

static void usage(char **argv) {
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "hypex CATDUMP1 CATDUMP2...\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      computes the Chernoff information and related quantities for\n");
  fprintf(stderr, 
	  "      a binary hypothesis test based on the category dump files CATDUMP1, CATDUMP2.\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "hypex -V\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      prints program version.\n");
}

/***********************************************************
 * CATEGORY PAIRED HASH TABLE FUNCTIONS                    *
 ***********************************************************/
cp_item_t *find_in_category_pair(category_pair_t *cp, hash_value_t id) {
    register cp_item_t *i, *loop;
    /* start at id */
    i = loop = &cp->hash[id & (cp->max_tokens - 1)];

    while( FILLEDP(i) ) {
	if( EQUALP(i->id,id) ) {
	    return i; /* found id */
	} else {
	    i++; /* not found */
	    /* wrap around */
	    i = (i >= &cp->hash[cp->max_tokens]) ? cp->hash : i; 
	    if( i == loop ) {
		return NULL; /* when hash table is full */
	    }
	}
    }

    /* empty slot, so not found */

    return i; 
}

/* returns true if the hash could be grown, false otherwise.
   When the hash is grown, the old values must be redistributed. */
bool_t grow_category_pair(category_pair_t *cp) {
  hash_count_t c, new_size;
  cp_item_t *i, temp_item;

  if( !(u_options & (1<<U_OPTION_GROWHASH)) ) {
    return 0;
  } else {
    if( cp->max_hash_bits < default_max_grow_hash_bits ) {

      /* grow the memory around the hash */
      if( (i = (cp_item_t *)realloc(cp->hash, 
		       sizeof(cp_item_t) * 
		       (1<<(cp->max_hash_bits+1)))) == NULL ) {
	errormsg(E_WARNING,
		"failed to grow hash table.\n");
	return 0;
      }
      /* we need the old value of learner->max_tokens for a while yet */
      cp->max_hash_bits++;

      /* realloc doesn't initialize the memory */
      memset(&((cp_item_t *)i)[cp->max_tokens], 0, 
	     ((1<<cp->max_hash_bits) - cp->max_tokens) * 
	     sizeof(cp_item_t));
      cp->hash = i; 
      
      /* now mark every used slot */
      for(c = 0; c < cp->max_tokens; c++) {
	if( FILLEDP(&cp->hash[c]) ) {
	  SETMARK(&cp->hash[c]);
	}
      }

      /* now relocate each marked slot and clear it */
      new_size = (1<<cp->max_hash_bits) - 1;
      for(c = 0; c < cp->max_tokens; c++) {
	while( MARKEDP(&cp->hash[c]) ) {
	  /* find where it belongs */
	  i = &cp->hash[cp->hash[c].id & new_size];
	  while( FILLEDP(i) && !MARKEDP(i) ) {
	    i++;
	    i = (i > &cp->hash[new_size]) ? cp->hash : i;
	  } /* guaranteed to exit since hash is larger than original */

	  /* clear the mark - this must happen after we look for i,
	   since it should be possible to find i == learner->hash[c] */
	  UNSETMARK(&cp->hash[c]); 

	  /* now refile */
	  if( i != &cp->hash[c] ) {
	    if( MARKEDP(i) ) {
	      /* swap */
	      memcpy(&temp_item, i, sizeof(cp_item_t));
	      memcpy(i, &cp->hash[c], sizeof(cp_item_t));
	      memcpy(&cp->hash[c], &temp_item, sizeof(cp_item_t));
	    } else {
	      /* copy and clear */
	      memcpy(i, &cp->hash[c], sizeof(cp_item_t));
	      memset(&cp->hash[c], 0, sizeof(cp_item_t));
	    }
	  } 
	  /* now &cp->hash[c] is marked iff there was a swap */
	}
      }
      cp->max_tokens = (1<<cp->max_hash_bits);
    } else {
      u_options &= ~(1<<U_OPTION_GROWHASH); /* it's the law */
      errormsg(E_WARNING,
	       "the token hash table is nearly full, slowing down.\n"); 
    }
  }
  return 1;
}

void init_category_pair(category_pair_t *cp) {
  cp->max_tokens = default_max_tokens;
  cp->max_hash_bits = default_max_hash_bits;
  cp->unique_token_count = 0;

  cp->hash = (cp_item_t *)malloc(cp->max_tokens * sizeof(cp_item_t));
  if( !cp->hash ) {
    errormsg(E_FATAL, "failed to allocate %li bytes for pair hash.\n",
	     (sizeof(cp_item_t) * ((long int)cp->max_tokens)));
  }
}

void free_category_pair(category_pair_t *cp) {
  if( cp->hash ) {
    free(cp->hash);
    cp->hash = NULL;
  }
}

/***********************************************************
 * DIVERGENCE CALCULATIONS                                 *
 ***********************************************************/
void edge_divergences(category_pair_t *cp, score_t *div_01, score_t *div_10) {
  cp_item_t *i, *e;
  double sum01 = 0.0;
  double sum10 = 0.0;

  double safetycheck0 = 1.0;
  double safetycheck1 = 1.0;

  e = cp->hash + cp->max_tokens;
  for(i = cp->hash; i != e; i++) {
    if( FILLEDP(i) ) {
      sum01 += (i->lam[0] - i->lam[1]) * 
	exp(i->lam[0] + i->ref - cp->cat[0].logZ);
      sum10 += (i->lam[1] - i->lam[0]) * 
	exp(i->lam[1] + i->ref - cp->cat[1].logZ);

      safetycheck0 += (exp(i->lam[0]) - 1.0) * exp(i->ref);
      safetycheck1 += (exp(i->lam[1]) - 1.0) * exp(i->ref);
    }
  }
  *div_01 = cp->cat[1].logZ - cp->cat[0].logZ + sum01;
  *div_10 = cp->cat[0].logZ - cp->cat[1].logZ + sum10;

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    /* this is a quick check to verify that the normalizing constants are
       roughly correct. */
    fprintf(stdout, "safetycheck: %f %f should both be close to zero\n",
	    log(safetycheck0) - cp->cat[0].logZ,
	    log(safetycheck1) - cp->cat[1].logZ);
  }

  fprintf(stdout, "# D(P0|P1) = %f\tD(P1|P0) = %f\n", 
	  nats2bits(*div_01), nats2bits(*div_10));

}

void calculate_divergences(category_pair_t *cp, score_t beta,
			   score_t div_Qbeta_P[2],
			   score_t *logZbeta,
			   score_t *Psibeta) {

  double logQbetainc;
  double Zbeta = 1.0;
  double sum0 = 0.0;
  double sum1 = 0.0;
  double sum2 = 0.0;
  cp_item_t *i, *e;

  e = cp->hash + cp->max_tokens;
  for(i = cp->hash; i != e; i++) {
    if( FILLEDP(i) ) {
      logQbetainc = beta * i->lam[0] + (1.0 - beta) * i->lam[1];

      Zbeta += (exp(logQbetainc) - 1.0) * exp(i->ref);

      sum0 += (logQbetainc - i->lam[0]) * exp(logQbetainc + i->ref);
      sum1 += (logQbetainc - i->lam[1]) * exp(logQbetainc + i->ref);

      sum2 += (i->lam[1] - i->lam[0]) * exp(logQbetainc + i->ref);
    }
  }

  *logZbeta = log(Zbeta);
  div_Qbeta_P[0] = sum0/Zbeta +  cp->cat[0].logZ - (*logZbeta);
  div_Qbeta_P[1] = sum1/Zbeta +  cp->cat[1].logZ - (*logZbeta);

  *Psibeta = sum2/Zbeta + cp->cat[0].logZ - cp->cat[1].logZ;

  /* the divergences can be calculated two slightly different ways,
     either by directly using the definition as above, or by using the
     expression containing Psibeta. We do both and average, which
     might hopefully make the result more accurate. */

  div_Qbeta_P[0] = 
    (div_Qbeta_P[0] + 
     cp->cat[0].logZ - (*logZbeta) + (1.0 - beta) * (*Psibeta))/2.0;

  div_Qbeta_P[1] = 
    (div_Qbeta_P[1] + 
     cp->cat[1].logZ - (*logZbeta) - beta * (*Psibeta))/2.0;

  fprintf(stdout, "%f %f %f %f %f %f\n", 
	  beta, nats2bits(*logZbeta), nats2bits(*Psibeta), 
	  nats2bits(div_Qbeta_P[0]), nats2bits(div_Qbeta_P[1]),
	  nats2bits(div_Qbeta_P[1] - div_Qbeta_P[0]));

}



/***********************************************************
 * FILE MANAGEMENT FUNCTIONS                               *
 ***********************************************************/
bool_t load_category_dump(const char *filename, category_pair_t *cp, int c) {
  FILE *input;
  char buf[MAGIC_BUFSIZE];
  int extra_lines = 0;
  bool_t ok = (bool_t)1;

  long unsigned int id;
  weight_t lam, dig_ref;
  int count;

  cp_item_t *i;
  hash_count_t linecount;

  if( (c != 0) && (c != 1) ) {
    errormsg(E_WARNING, "too many categories.\n");
    return 0;
  }

  inputline = 0;
  inputfile = (char *)filename;

  input = fopen(filename, "rb");
  if( input ) {
    set_iobuf_mode(input);

    cp->cat[c].fullfilename = strdup(filename);
    ok = load_category_header(input, &(cp->cat[c]));
    if( ok ) {
      if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	  strncmp(buf, MAGIC_DUMP, strlen(MAGIC_DUMP)) != 0 ) {
	errormsg(E_ERROR, 
		 "%s is not a dbacl model dump! (see dbacl(1) -d switch)\n",
		 filename);
	ok = (bool_t)0;
      } else if( (cp->cat[c].max_order != 1) ) {
	/* we don't support complicated models, only uniform reference
	   measures and unigram features */
	errormsg(E_ERROR, "the category %s is not supported (model too complex)\n",
		 cp->cat[c].filename);
	ok = (bool_t)0;
      } else {
	linecount = 0;
	while( ok && fill_textbuf(input, &extra_lines) ) {
	  inputline++;
	  /* we don't parse the full line, only the numbers */
	  if( sscanf(textbuf, MAGIC_DUMPTBL_i,
		     &lam, &dig_ref, &count, &id) == 4 ) {
	    linecount++;

	    i = find_in_category_pair(cp, id);

	    if( i && !FILLEDP(i) &&
		((100 * cp->unique_token_count) >= 
		 (HASH_FULL * cp->max_tokens)) && grow_category_pair(cp) ) {
	      i = find_in_category_pair(cp,id);
	      /* new i, go through all tests again */
	    }

	    if( i ) {
	      /* we cannot allow collisions, so we simply drop 
		 the weight if the slot is full */
	      switch(c) {
	      case 0:
		if( !FILLEDP(i) ) {
		  SET(i->id,id);
		  i->lam[c] = lam;
		  i->ref = dig_ref;
		  i->typ.cls |= 0x01;
		  if( cp->unique_token_count < K_TOKEN_COUNT_MAX )
		    { cp->unique_token_count++; } else { overflow_warning = 1; }
		}
		break;
	      case 1:
		if( FILLEDP(i) ) {
		  if( fabs(dig_ref - i->ref) > 0.00001 ) {
		    errormsg(E_WARNING, 
			     "unequal reference measures! New token ignored (id=%lx).\n",
			     id);
		  } else {
		    i->lam[c] = lam;
		    i->typ.cls++;
		  }
		} else {
		  SET(i->id,id);
		  i->lam[c] = lam;
		  i->ref = dig_ref;
		  i->typ.cls |= 0x02;
		  if( cp->unique_token_count < K_TOKEN_COUNT_MAX )
		    { cp->unique_token_count++; } else { overflow_warning = 1; }
		}
	      default:
		break;
	      }
	    } else {
	      errormsg(E_ERROR, "ran out of hashes.\n");
	      ok = (bool_t)0;
	    }

	  } else if( *textbuf ) {
	    errormsg(E_ERROR, "unable to parse line %ld of %s\n",
		     inputline, cp->cat[c].fullfilename);
	    ok = (bool_t)0;
	  }
	}
	if( ok && (linecount != cp->cat[c].model_unique_token_count) ) {
	  errormsg(E_ERROR, 
		   "incorrect number of features, expectig %ld, got %ld\n",
		   (long int)cp->cat[c].model_unique_token_count, inputline);
	  ok = (bool_t)0;
	}
      }
    }
    fclose(input);
  } else {
    errormsg(E_ERROR, "couldn't open %s\n", filename);
  }

  return ok;
}


bool_t process_dump_pair(const char *d1, const char *d2, category_pair_t *cp) {

  score_t div01, div10;
  score_t beta;
  score_t div_Qbeta_P[2] = {0.0, 0.0};
  score_t logZbeta;
  score_t Psibeta;
  score_t chernoff_rate = 0.0;
  score_t chernoff_beta = 0.0;

  if( u_options & (1<<U_OPTION_VERBOSE) ) {
    fprintf(stdout, "processing (%s, %s)\n", d1, d2);
  }

  if( load_category_dump(d1, cp, 0) && load_category_dump(d2, cp, 1) ) {
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      fprintf(stdout, "loaded successfully (%s, %s)\n", d1, d2);
    }

    edge_divergences(cp, &div01, &div10);

    fprintf(stdout, 
	    "# beta | logZ_beta | Psi_beta | D(Q_beta|P_0) | D(Q_beta|P_1) | t \n");
    for(beta = 0.0; beta < 1.0; beta += default_stepsize) {
      if( chernoff_beta <= 0.0 ) {
	/* we haven't crossed the intersection yet, this occurs
	 the first time the divergence order is inverted */
	chernoff_rate = div_Qbeta_P[0] + div_Qbeta_P[1];
	calculate_divergences(cp, beta, div_Qbeta_P, &logZbeta, &Psibeta);
	if( div_Qbeta_P[0] <= div_Qbeta_P[1] ) {
	  chernoff_beta = beta - default_stepsize/2;
	  chernoff_rate = (chernoff_rate + div_Qbeta_P[0] + div_Qbeta_P[1])/4;
	}
      } else {
	calculate_divergences(cp, beta, div_Qbeta_P, &logZbeta, &Psibeta);
      }
    }
    calculate_divergences(cp, 1.0, div_Qbeta_P, &logZbeta, &Psibeta);
    
    fprintf(stdout,
	    "# chernoff_rate %f chernoff_beta %f\n",
	    nats2bits(chernoff_rate), chernoff_beta);

  }
  return (bool_t)0;
}


/***********************************************************
 * MAIN FUNCTIONS                                          *
 ***********************************************************/
int set_option(int op, char *optarg) {
  int c = 0;
  switch(op) {
  case 'V':
    fprintf(stdout, "hypex version %s\n", VERSION);
    fprintf(stdout, COPYBLURB, "hypex");
    exit(0);
    break;
  case 'h': /* select memory size in powers of 2 */
    default_max_hash_bits = atoi(optarg);
    if( default_max_hash_bits > MAX_HASH_BITS ) {
      errormsg(E_WARNING,
	       "maximum hash size will be 2^%d\n", 
	       MAX_HASH_BITS);
      default_max_hash_bits = MAX_HASH_BITS;
    }
    default_max_tokens = (1<<default_max_hash_bits);
    c++;
    break;
  case 'H': /* select memory size in powers of 2 */
    default_max_grow_hash_bits = atoi(optarg);
    if( default_max_grow_hash_bits > MAX_HASH_BITS ) {
      errormsg(E_WARNING,
	       "maximum hash size will be 2^%d\n", 
	       MAX_HASH_BITS);
      default_max_grow_hash_bits = MAX_HASH_BITS;
    }
    default_max_grow_tokens = (1<<default_max_grow_hash_bits);
    u_options |= (1<<U_OPTION_GROWHASH);
    c++;
    break;
  case 's': /* select stepsize */
    default_stepsize = atof(optarg);
    if( (default_stepsize <= 0.0) ||
	(default_stepsize >= 1.0) ) {
      errormsg(E_WARNING,
	       "stepsize must be between 0.0 and 1.0, using 0.1\n");
      default_stepsize = 0.1;
    }
    c++;
    break;
  case 'v':
    u_options |= (1<<U_OPTION_VERBOSE);
    break;
  default:
    c--;
    break;
  }
  return c;
}

void sanitize_options() {

  /* consistency checks */

  /* decide if we need some options */

}


int main(int argc, char **argv) {

  signed char op;
  category_pair_t cp;
  int i, j;

  progname = "hypex";
  inputfile = "";
  inputline = 0;

  init_category_pair(&cp);

  /* set up internationalization */
  if( !setlocale(LC_ALL, "") ) {
    errormsg(E_WARNING,
	    "could not set locale, internationalization disabled\n");
  } else {
    if( u_options & (1<<U_OPTION_VERBOSE) ) {
      errormsg(E_WARNING,
	      "international locales not supported\n");
    }
  }

#if defined(HAVE_GETPAGESIZE)
  system_pagesize = getpagesize();
#endif
  if( system_pagesize == -1 ) { system_pagesize = BUFSIZ; }

  /* parse the options */
  while( (op = getopt(argc, argv, 
		      "H:h:s:Vv")) > -1 ) {
    set_option(op, optarg);
  }

  /* end option processing */
  sanitize_options();

  init_buffers();

  /* now process each pair of files on the command line */
  if( (optind > -1) && *(argv + optind) ) {
    for(i = optind; i < argc; i++) {
      for(j = i + 1; j < argc; j++) {
	process_dump_pair(argv[i], argv[j], &cp);
      }
    }
  } else {
    errormsg(E_ERROR, "missing model dumps! Use dbacl(1) with -d switch\n");
    usage(argv);
    exit(0);
  }
  
  free_category_pair(&cp);

  cleanup_buffers();

  return exit_code;
}


