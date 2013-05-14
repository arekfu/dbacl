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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "dbacl.h"

extern options_t m_options; 
extern charparser_t m_cp; 
extern options_t u_options; 

extern empirical_t empirical;

extern category_t cat[MAX_CAT];
extern category_count_t cat_count;

extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;

extern char *extn;
extern long system_pagesize;

/***********************************************************
 * MISCELLANEOUS FUNCTIONS                                 *
 ***********************************************************/

char *sanitize_path(char *in, char *extension) {
  char *q;
  char *path;
  charbuf_len_t l;

  if( !extension ) { extension = ""; }
  /* this bit likely fails in DOS ;-) */
  path = getenv(DEFAULT_CATPATH);
  if( path && (*in != '/') && (*in != '.') ) {
    l = strlen(path);
    q = (char *)malloc(l + strlen(in) + 3 + strlen(extension));
    strcpy(q, path);
    if( q[l - 1] != '/' ) {
      q[l] = '/'; 
      q[l + 1] = 0;
    }
    strcat(q, in);
    strcat(q, extension);
  } else {
    q = (char *)malloc(strlen(in) + strlen(extension) + 1);
    strcpy(q, in);
    strcat(q, extension);
  }
  return q;
}

#define MOPTION(x,opt) (((opt) & (1<<(x))) ? ("|" #x) : "")


char *print_charparser_type(charparser_t mcp, char *buf) {
  strcpy(buf, 
	 (mcp == CP_DEFAULT) ? "CP_DEFAULT" :
	 (mcp == CP_CHAR) ? "CP_CHAR" :
	 (mcp == CP_ALPHA) ? "CP_ALPHA" :
	 (mcp == CP_ALNUM) ? "CP_ALNUM" :
	 (mcp == CP_GRAPH) ? "CP_GRAPH" :
	 (mcp == CP_CEF) ? "CP_CEF" :
	 (mcp == CP_ADP) ? "CP_ADP" :
	 (mcp == CP_CEF2) ? "CP_CEF2" : "CP_UNKNOWN");
  return buf;
}


char *print_model_options(options_t opt, charparser_t mcp,  char *buf) {
  print_charparser_type(mcp, buf);
  strcat(buf, MOPTION(M_OPTION_REFMODEL, opt));
  strcat(buf, MOPTION(M_OPTION_TEXT_FORMAT, opt));
  strcat(buf, MOPTION(M_OPTION_MBOX_FORMAT, opt));
  strcat(buf, MOPTION(M_OPTION_XML, opt));
  strcat(buf, MOPTION(M_OPTION_I18N, opt));
  strcat(buf, MOPTION(M_OPTION_CASEN, opt));
  strcat(buf, MOPTION(M_OPTION_CALCENTROPY, opt));
  strcat(buf, MOPTION(M_OPTION_MULTINOMIAL, opt));
  strcat(buf, MOPTION(M_OPTION_HEADERS, opt));
  strcat(buf, MOPTION(M_OPTION_PLAIN, opt));
  strcat(buf, MOPTION(M_OPTION_NOPLAIN, opt));
  strcat(buf, MOPTION(M_OPTION_SHOW_LINKS, opt));
  strcat(buf, MOPTION(M_OPTION_SHOW_ALT, opt));
  strcat(buf, MOPTION(M_OPTION_HTML, opt));
  strcat(buf, MOPTION(M_OPTION_XHEADERS, opt));
  strcat(buf, MOPTION(M_OPTION_THEADERS, opt));
  strcat(buf, MOPTION(M_OPTION_SHOW_SCRIPT, opt));
  strcat(buf, MOPTION(M_OPTION_SHOW_STYLE, opt));
  strcat(buf, MOPTION(M_OPTION_SHOW_HTML_COMMENTS, opt));
  strcat(buf, MOPTION(M_OPTION_USE_STDTOK, opt));
  strcat(buf, MOPTION(M_OPTION_ATTACHMENTS, opt));
  strcat(buf, MOPTION(M_OPTION_WARNING_BAD, opt));
  strcat(buf, MOPTION(M_OPTION_NGRAM_STRADDLE_NL, opt));
  return buf;
}

char *print_user_options(options_t opt, char *buf) {
  strcpy(buf, MOPTION(U_OPTION_CLASSIFY, opt));
  strcat(buf, MOPTION(U_OPTION_LEARN, opt));
  strcat(buf, MOPTION(U_OPTION_FASTEMP, opt));
  strcat(buf, MOPTION(U_OPTION_CUTOFF, opt));
  strcat(buf, MOPTION(U_OPTION_VERBOSE, opt));
  strcat(buf, MOPTION(U_OPTION_STDIN, opt));
  strcat(buf, MOPTION(U_OPTION_SCORES, opt));
  strcat(buf, MOPTION(U_OPTION_POSTERIOR, opt));
  strcat(buf, MOPTION(U_OPTION_FILTER, opt));
  strcat(buf, MOPTION(U_OPTION_DEBUG, opt));
  strcat(buf, MOPTION(U_OPTION_DUMP, opt));
  strcat(buf, MOPTION(U_OPTION_APPEND, opt));
  strcat(buf, MOPTION(U_OPTION_DECIMATE, opt));
  strcat(buf, MOPTION(U_OPTION_GROWHASH, opt));
  strcat(buf, MOPTION(U_OPTION_INDENTED, opt));
  strcat(buf, MOPTION(U_OPTION_NOZEROLEARN, opt));
  strcat(buf, MOPTION(U_OPTION_MMAP, opt));
  strcat(buf, MOPTION(U_OPTION_CONFIDENCE, opt));
  strcat(buf, MOPTION(U_OPTION_VAR, opt));
  return buf;
}

error_code_t sanitize_model_options(options_t *mopt, charparser_t *mcp,
				    category_t *cat) {
  options_t mask;

  /* things that always override mopt */
  mask = 
    (1<<M_OPTION_WARNING_BAD)|
    (1<<M_OPTION_SHOW_HTML_COMMENTS)|
    (1<<M_OPTION_SHOW_SCRIPT)|
    (1<<M_OPTION_SHOW_STYLE)|
    (1<<M_OPTION_HEADERS)|
    (1<<M_OPTION_XHEADERS)|
    (1<<M_OPTION_THEADERS)|
    (1<<M_OPTION_SHOW_ALT)|
    (1<<M_OPTION_SHOW_FORMS)|
    (1<<M_OPTION_NOHEADERS)|
    (1<<M_OPTION_ATTACHMENTS)|
    (1<<M_OPTION_SHOW_LINKS)|
    (1<<M_OPTION_NGRAM_STRADDLE_NL)|
    (1<<M_OPTION_REFMODEL)|
    (1<<M_OPTION_I18N)|
    (1<<M_OPTION_CASEN)|
    (1<<M_OPTION_USE_STDTOK)|
    (1<<M_OPTION_CALCENTROPY)|
    (1<<M_OPTION_MULTINOMIAL);
  *mopt |= (cat->model.options & mask);

  /* things that are problematic */

  if( *mopt & (1<<M_OPTION_TEXT_FORMAT) ) {
    if( cat->model.options & (1<<M_OPTION_MBOX_FORMAT) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T email, forcing -T text\n",
	      cat->filename);
    }
  } else if( *mopt & (1<<M_OPTION_MBOX_FORMAT) ) {
    if( cat->model.options & (1<<M_OPTION_TEXT_FORMAT) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T text, forcing -T email\n",
	      cat->filename);
    }
  } else {
    *mopt |= (cat->model.options & ((1<<M_OPTION_TEXT_FORMAT)|(1<<M_OPTION_MBOX_FORMAT)));
  }

  if( *mopt & (1<<M_OPTION_XML) ) {
    if( cat->model.options & (1<<M_OPTION_HTML) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T html, forcing -T xml\n",
	      cat->filename);
    }
  } else if( *mopt & (1<<M_OPTION_HTML) ) {
    if( cat->model.options & (1<<M_OPTION_XML) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T xml, forcing -T html\n",
	      cat->filename);
    }
  } else {
    *mopt |= (cat->model.options & ((1<<M_OPTION_XML)|(1<<M_OPTION_HTML)));
  }

  if( *mopt & (1<<M_OPTION_PLAIN) ) {
    if( cat->model.options & (1<<M_OPTION_NOPLAIN) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T email:noplain, forcing -T email:plain\n",
	      cat->filename);
    }
  } else if( *mopt & (1<<M_OPTION_NOPLAIN) ) {
    if( cat->model.options & (1<<M_OPTION_PLAIN) ) {
      errormsg(E_WARNING,
	      "category %s was learned with -T email:plain, forcing -T email:noplain\n",
	      cat->filename);
    }
  } else {
    *mopt |= (cat->model.options & ((1<<M_OPTION_NOPLAIN)|(1<<M_OPTION_PLAIN)));
  }

/*   mask =  */
/*     (1<<M_OPTION_CHAR_ALPHA)| */
/*     (1<<M_OPTION_CHAR_ALNUM)| */
/*     (1<<M_OPTION_CHAR_CHAR)| */
/*     (1<<M_OPTION_CHAR_GRAPH)| */
/*     (1<<M_OPTION_CHAR_ADP)| */
/*     (1<<M_OPTION_CHAR_CEF); */

/*   if( (*mopt & mask) && ((cat->m_options & mask) != (*mopt & mask)) ) { */
/*     errormsg(E_FATAL, */
/* 	    "category %s has incompatible token set (check -e switch)\n", */
/* 	    cat->filename); */
/*     return 0; */
/*   } else { */
/*     *mopt |= (cat->m_options & mask); */
/*   } */
  if( (*mcp != CP_DEFAULT) && (*mcp != cat->model.cp) ) {
    errormsg(E_FATAL,
	    "category %s has incompatible token set (check -e switch)\n",
	    cat->filename);
    return 0;
  } else {
    *mcp = cat->model.cp;
  }

  return 1;
}

regex_count_t load_regex(char *buf) {
  char *p;
  regex_count_t c;

  /* set up the submatch bitmap */
  re[regex_count].submatches |= 0;
  re[regex_count].flags = 0;

  p = strrchr(buf, '|');
  if( p && ( *(--p) == '|') ) {
    /* assume string ends in ||12345, use as bitmap */
    *p = '\0';
    for(p += 2; *p; p++) {
      /* assume ascii number positions */
      if( (*p > '9') || (*p < '1')) {
	if( *p != '\n' ) {
	  errormsg(E_WARNING,
		  "could not decode || suffix for '%s'.\n", buf);
	}
      } else {
	re[regex_count].submatches |= (1<<(*p - '0'));
      }
    }
  } else { /* no bitmap specified */
    re[regex_count].submatches = ~0;
  }

  /* remove trailing newline */
  if( buf[strlen(buf) - 1] == '\n' ) { 
    buf[strlen(buf) - 1] = '\0'; 
  }
  /* GNU regexes use ordinary strings */
  /* it's a regex - see if we've got it already */
  for(c = 0; c < regex_count; c++) {
    if( strcmp(re[c].string, buf) == 0 ) {
      break;
    }
  }
  if( c >= regex_count ) { /* not found */
    /* add it to our list */
    if( strchr(buf, '(') ) {
      re[regex_count].string = strdup(buf);
    } else {
      char *dup = (char *)malloc(strlen(buf)+2);
      if( dup ) {
	sprintf(dup, "(%s)", buf);
	errormsg(E_WARNING,
		 "no captures found in regex, converting to '%s'\n",
		 dup);
      }
      re[regex_count].string = dup;
    }
    if( !re[regex_count].string ) {
      errormsg(E_FATAL,
	       "could not prepare regular expression '%s'.\n",
	       buf);
    }
    /* and compile the regex */
    if( regcomp(&re[regex_count].regex, 
		re[regex_count].string, REG_EXTENDED) != 0 ) {
      errormsg(E_FATAL, /* not much point going on */
	      "could not compile regular expression '%s'.\n", 
	      re[regex_count].string);
    } else {
      regex_count++;
      if( regex_count >= MAX_RE ) { 
	errormsg(E_FATAL, "too many regular expressions\n");
	/* no point in going on */
      }
    }
  }
  return c;
}

void free_all_regexes() {
  regex_count_t k;
  for(k = 0; k < regex_count; k++) {
    regfree(&re[k].regex);
  }
}


/***********************************************************
 * EMPIRICAL DISTRIBUTION OF TOKENS                        *
 ***********************************************************/

/* initialize global learner object */
void init_empirical(empirical_t *emp, hash_count_t dmt, hash_bit_count_t dmhb) {

    /* some constants */
    emp->max_tokens = dmt;
    emp->max_hash_bits = dmhb;
    emp->full_token_count = 0;
    emp->unique_token_count = 0;
    emp->track_features = 0;
    emp->feature_stack_top = 0;
    emp->hashfull_warning = 0;

    /* allocate room for hash */
    emp->hash = (h_item_t *)calloc(emp->max_tokens, sizeof(h_item_t));
    if( !emp->hash ) {
      errormsg(E_FATAL,
	       "not enough memory? I couldn't allocate %li bytes\n", 
	       (sizeof(h_item_t) * ((long int)emp->max_tokens)));
    }

    MADVISE(emp->hash, sizeof(h_item_t) * emp->max_tokens, MADV_RANDOM);

}

void free_empirical(empirical_t *emp) {
  if( emp->hash ) {
    free(emp->hash);
  }
}

void clear_empirical(empirical_t *emp) {
    token_stack_t i;

    if( emp->track_features ) {
	/* this may actually be slower than a global memset */ 
	for(i = 0; i < emp->feature_stack_top; i++) {
	    memset(emp->feature_stack[i], 0, sizeof(h_item_t));
	}
    } else {
	memset(emp->hash, 0, sizeof(h_item_t) * emp->max_tokens);
    }

    emp->full_token_count = 0;
    emp->unique_token_count = 0;
    emp->feature_stack_top = 0;

    if( u_options & (1<<U_OPTION_FASTEMP) ) {
      emp->track_features = 1;
    }
}

h_item_t *find_in_empirical(empirical_t *emp, hash_value_t id) {
  register h_item_t *i, *loop;
  /* start at id */
  i = loop = &emp->hash[id & (emp->max_tokens - 1)];

  while( FILLEDP(i) ) {
    if( EQUALP(i->id,id) ) {
      return i; /* found id */
    } else {
      i++; /* not found */
      /* wrap around */
      i = (i >= &emp->hash[emp->max_tokens]) ? emp->hash : i; 
      if( i == loop ) {
	return NULL; /* when hash table is full */
      }
    }
  }

  /* empty slot, so not found */

  return i; 
}

/* calculates the entropy of the full empirical measure */
score_t empirical_entropy(empirical_t *emp) {
  token_stack_t j;
  hash_count_t i;
  score_t e = 0.0;

  if( emp->track_features && 
      (emp->feature_stack_top < MAX_TOKEN_LINE_STACK) ) {
    for(j = 0; j < emp->feature_stack_top; j++) {
      e += ((score_t)emp->feature_stack[j]->count) * 
	log((score_t)emp->feature_stack[j]->count);
    }
  } else {
    for(i = 0; i < emp->max_tokens; i++) {
      if( FILLEDP(&emp->hash[i]) ) {
	e += ((score_t)emp->hash[i].count) * 
	  log((score_t)emp->hash[i].count);
      }
    }
  }
  e = e/emp->full_token_count - log((score_t)emp->full_token_count);

  return -e;
}

/***********************************************************
 * CATEGORY FUNCTIONS                                      *
 ***********************************************************/

/* initialize to zero. fullfilename specified elsewhere */
void init_category(category_t *cat) {
    char *p;

    cat->model_full_token_count = 0;
    cat->model_unique_token_count = 0;
    cat->score = 0.0;
    cat->score_div = 0.0;
    cat->score_s2 = 0.0;
    cat->score_shannon = 0.0;
    cat->shannon = 0.0;
    cat->shannon_s2 = 0.0;
    cat->alpha = 0.0;
    cat->beta = 0.0;
    cat->mu = 0.0;
    cat->s2 = 0.0;
    cat->prior = 0.0;
    cat->complexity = 0.0;
    cat->fcomplexity = 0;
    cat->fmiss = 0;
    cat->max_order = 0;
    memset(cat->mediacounts, 0, sizeof(token_count_t)*TOKEN_CLASS_MAX);
    p = strrchr(cat->fullfilename, '/');
    if( p ) {
      cat->filename = p + 1; /* only keep basename */
    } else {
      cat->filename = cat->fullfilename;
    }
    cat->filename = strdup(cat->filename);
    if( extn && *extn ) { 
      p = strstr(cat->filename, extn);
      if( p ) { *p = '\0'; }
    }
    cat->retype = 0;
    cat->model.type = simple;
    cat->model.options = 0;
    cat->model.cp = 0;
    cat->model.dt = 0;
    cat->c_options = 0;
    cat->hash = NULL;
    cat->mmap_offset = 0;
    cat->mmap_start = NULL;
}

bool_t create_category_hash(category_t *cat, FILE *input, int protf) {
  hash_count_t i, j;

  if( u_options & (1<<U_OPTION_MMAP) ) {
    cat->mmap_offset = ftell(input);
    if( cat->mmap_offset > 0 ) {
      cat->mmap_start = 
	(byte_t *)MMAP(0, sizeof(c_item_t) * cat->max_tokens + 
		       cat->mmap_offset,
		       protf, MAP_SHARED, fileno(input), 0);
      if( cat->mmap_start == MAP_FAILED ) { cat->mmap_start = NULL; }
      if( cat->mmap_start ) {
	cat->hash = (c_item_t *)(cat->mmap_start + cat->mmap_offset);
	MADVISE(cat->hash, sizeof(c_item_t) * cat->max_tokens, 
		MADV_SEQUENTIAL|MADV_WILLNEED);
	/* lock the pages to prevent swapping - on Linux, this
	   works without root privs so long as the user limits
	   are big enough - mine are unlimited ;-) 
	   On other OSes, root may me necessary. If we can't
	   lock, it doesn't really matter, but cross validations
	   and multiple classifications are a _lot_ faster with locking. */
	MLOCK(cat->hash, sizeof(c_item_t) * cat->max_tokens);
	cat->c_options |= (1<<C_OPTION_MMAPPED_HASH);
      }
    }
  }

  if( !cat->hash ) {
    cat->c_options &= ~(1<<C_OPTION_MMAPPED_HASH);
    /* allocate hash table */
    cat->hash = (c_item_t *)malloc(sizeof(c_item_t) * cat->max_tokens);
    if( !cat->hash ) {
      errormsg(E_ERROR, "not enough memory for category %s\n", 
	       cat->filename);
      return 0;
    }

    MADVISE(cat->hash, sizeof(c_item_t) * cat->max_tokens, 
	    MADV_SEQUENTIAL);

    /* read in hash table */
    i = cat->max_tokens;
    j = 0;
    while(!ferror(input) && !feof(input) && (j < i) ) {
      j += fread(cat->hash + j, sizeof(c_item_t), i - j, input);
    }

    if( j < i ) {
      errormsg(E_ERROR, "corrupt category? %s\n",
	       cat->fullfilename);
      free(cat->hash);
      return 0;
    }

  }
  return 1;
}


void free_category_hash(category_t *cat) {
  if( cat->hash ) {
    if( cat->mmap_start != NULL ) {
      MUNMAP(cat->mmap_start, cat->max_tokens * sizeof(c_item_t) + 
	     cat->mmap_offset);
      cat->mmap_start = NULL;
      cat->mmap_offset = 0;
      cat->hash = NULL;
    }
    if( cat->hash ) {
      free(cat->hash);
      cat->hash = NULL;
    }
  }
}

/* frees the resrouces associated with a category */
void free_category(category_t *cat) {
  free_category_hash(cat);
  if( cat->filename ) { free(cat->filename); }
  if( cat->fullfilename ) { free(cat->fullfilename); }
}

/* turns purely random text into a category of its own */
void init_purely_random_text_category(category_t *cat) {
  alphabet_size_t i, j;
  weight_t z = -log((double)(ASIZE - AMIN));

#if defined DIGITIZE_DIGRAMS
  digitized_weight_t zz = PACK_DIGRAMS(z);
#endif
    
  for(i = AMIN; i < ASIZE; i++) {
    for(j = AMIN; j < ASIZE; j++) {
#if defined DIGITIZE_DIGRAMS
      cat->dig[i][j] = zz;
#else
      cat->dig[i][j] = z;
#endif
    }
  }

  z = -log((double)(ASIZE - AMIN));
#if defined DIGITIZE_DIGRAMS
  zz = PACK_DIGRAMS(z);
#endif
  for(j = AMIN; j < ASIZE; j++) {
#if defined DIGITIZE_DIGRAMS
    cat->dig[(alphabet_size_t)DIAMOND][j] = zz;
#else
    cat->dig[(alphabet_size_t)DIAMOND][j] = z;
#endif
  } 

  /* not needed: set DIAMOND-DIAMOND score for completeness only */

#if defined DIGITIZE_DIGRAMS
  cat->dig[(alphabet_size_t)DIAMOND][(alphabet_size_t)DIAMOND] = DIGITIZED_WEIGHT_MIN;
#else
  cat->dig[(alphabet_size_t)DIAMOND][(alphabet_size_t)DIAMOND] = log(0.0);
#endif

  cat->logZ = 0.0;
  cat->divergence = 0.0;
  cat->delta = 0.0;
  cat->renorm = 0.0;
  cat->hash = NULL;
  cat->mmap_start = NULL;
  cat->mmap_offset = 0;
  cat->model.type = simple;
  cat->max_order = 1;
  cat->model.options = 0;
  cat->model.cp = 0;
  cat->model.dt = 0;
}

c_item_t *find_in_category(category_t *cat, hash_value_t id) {
    register c_item_t *i, *loop;

    if( cat->hash ) {
	/* start at id */
	i = loop = &cat->hash[id & (cat->max_tokens - 1)];

	while( FILLEDP(i) ) {
	    if( EQUALP(NTOH_ID(i->id),id) ) {
		return i; /* found id */
	    } else {
		i++; /* not found */
		/* wrap around */
		i = (i >= &cat->hash[cat->max_tokens]) ? cat->hash : i; 
		if( i == loop ) {
		    return NULL; /* when hash table is full */
		}
	    }
	}
	return i;
    } else {
	return NULL;
    }
}

/* for each loaded category, this calculates the score. 
   Tokens have the format
   DIAMOND t1 DIAMOND t2 ... tn DIAMOND CLASSEP class NUL */
void score_word(char *tok, token_type_t tt, regex_count_t re) {
  category_count_t i = 0;
  weight_t multinomial_correction = 0.0;
  weight_t shannon_correction = 0.0;
  weight_t lambda, ref, oldscore;
  bool_t apply;
  alphabet_size_t pp, pc, len;
  hash_value_t id;
  char *q;
  register c_item_t *k = NULL;
  h_item_t *h = NULL;

  /* we skip "empty" tokens */
  for(q = tok; q && *q == DIAMOND; q++);
  if( q && (*q != EOTOKEN) ) { 

    id = hash_full_token(tok);

    if( (m_options & (1<<M_OPTION_CALCENTROPY)) ) {
      /* add the token to the hash */

      h = find_in_empirical(&empirical, id);
      if( h ) {
 	if( FILLEDP(h) ) {
	  if( h->count < K_TOKEN_COUNT_MAX ) {
	    h->count++;

#if defined SHANNON_STIRLING
	    shannon_correction = 1.0 + log((weight_t)h->count * 
					   (weight_t)(h->count - 1))/2.0;
#else
	    shannon_correction += log((weight_t)h->count)*(weight_t)h->count
	      - log((weight_t)(h->count - 1))*(weight_t)(h->count - 1);
#endif
	  }
 	} else {
 	  if( /* !FILLEDP(i) && */
 	     ((100 * empirical.unique_token_count) <
 	      (HASH_FULL * empirical.max_tokens) )) {
 	    /* fill the empirical hash */
 	    SET(h->id,id);
	    empirical.unique_token_count += 
	      ( empirical.unique_token_count < K_TOKEN_COUNT_MAX ) ? 1 : 0;
	    h->count = 1;
#if defined SHANNON_STIRLING
	    shannon_correction = 1.0 - log(2.0 * M_PI)/2.0;
#else
	    shannon_correction = 0;
#endif
 	  } else {
 	    /* hash full */
 	    h = NULL; 
 	    if( !empirical.hashfull_warning ) {
	      errormsg(E_WARNING,
 		      "empirical hash full, calculation may be skewed. " 
 		      "Try option -h %d\n", 
 		      (empirical.max_hash_bits + 1));
 	      empirical.hashfull_warning = 1;
 	    }
 	    return; /* pretend word doesn't exist */
 	  }

	  if( empirical.track_features ) {
	    if( empirical.feature_stack_top < MAX_TOKEN_LINE_STACK ) {
	      empirical.feature_stack[empirical.feature_stack_top++] = h;
	    } else {
	      empirical.track_features = 0;
	      empirical.feature_stack_top = 0;
	    }
	  }
 	}
	empirical.full_token_count += 
	  ( empirical.full_token_count < K_TOKEN_COUNT_MAX ) ? 1 : 0;
	
      }
    }

    /* now do scoring for all available categories */
    for(i = 0; i < cat_count; i++) {

      oldscore = cat[i].score;
      lambda = 0.0;
      ref = 0.0;

      /* see if this token is for us. The rule is: a category either
	 uses the standard tokenizer (in that case re = INVALID_RE),
	 or it uses only those regexes which are listed in the retype
	 bitmap. Since re = 0 is taken by the standard tokenizer, 
	 this occurs when re > 0 and we have to subtract 1 to check
	 the bitmap. Simple, really ;-) */
      apply = ( ((re == INVALID_RE) && 
		 (tt.order <= cat[i].max_order) && !cat[i].retype) ||
		((re > 0) && 
		 (cat[i].retype & (1<<(re-1)))) );
      if( apply ) {

	/* if token found, add its lambda weight */
	k = find_in_category(&cat[i], id);
	if( k ) {
	  lambda = UNPACK_LAMBDA(NTOH_LAMBDA(k->lam));
	}

	if( tt.order == 1 ) {
	  /* now compute the reference weight from digram model,
	     (while duplicating the digitization error) */
	  pp = (unsigned char)*tok;
	  CLIP_ALPHABET(pp);
	  q = tok + 1;
	  len = 1;
	  while( *q != EOTOKEN ) {
	    if( *q == '\r' ) {
	      q++;
	      continue;
	    }
	    pc = (unsigned char)*q;
	    CLIP_ALPHABET(pc);
	    ref += UNPACK_DIGRAMS(cat[i].dig[pp][pc]);
	    pp = pc;
	    q++;
	    if( *q != DIAMOND ) { len++; }
	  }
	  ref += UNPACK_DIGRAMS(cat[i].dig[RESERVED_TOKLEN][len]) -
	    UNPACK_DIGRAMS(cat[i].dig[RESERVED_TOKLEN][0]);
	  ref = UNPACK_RWEIGHTS(PACK_RWEIGHTS(ref));

	}

	/* update the complexity */
	/* note: complexity has nothing to do with Kolmogorov's definition */
	/* this is actually very simple in hindsight, but took
	   me a long time to get right. Different versions of dbacl
	   compute the complexity in different ways, and I kept changing
	   the method because I wasn't happy. 

	   In previous versions, complexity is an integer, which begs
	   the question "what does it count?".  For simple models
	   (max_order = 1) this is easy: we count the number of
	   tokens. But for max_order > 1, it's not obvious, because we
	   need to divide by 1/max_order asymptotically.

	   One way is to increment the complexity if we encounter a
	   token of order max_order. This is correct for Markovian
	   models and corresponds to the dbacl.ps writeup, but causes
	   trouble in some edge cases. For example, if we classify a
	   very short document, there might not be enough tokens to
	   make sense.  This actually occurs when dbacl must classify
	   individual lines, and some lines contain one or two tokens
	   only.  Worse, dbacl used to renormalize at the same time as
	   updating the complexity, which increases the likelihood of
	   having a negative divergence score estimate in the first
	   few iterations - very bad.  Finally, the complexity is
	   nearly meaningless for models built up from regular
	   expressions, because both the start and the end of each
	   line contains incomplete n-grams (recall regexes can't
	   straddle newlines).

	   So to solve these problems, some previous versions of dbacl
	   counted always the order 1 tokens. Asymptotically, this
	   makes no difference, but again it fails on edge
	   cases. Firstly, doing this means that the complexity for a
	   simple model is the same as the complexity for an n-gram
	   model for any n, so that makes it hard to compare mixed
	   models because n-gram model scores are consitently biased
	   for n > 1. Another problem is again with regexes, because
	   the incomplete n-gram tokens at the start and end of each
	   line add up to a pretty large error over thousands of
	   tokens.

	   The solution to the above problems is twofold: first, we
	   renormalize after each token, regardless of its order. Of
	   course this means we must divide logZ by the number of
	   tokens per complexity unit, ie renorm = delta * logZ with
	   delta = 1/max_order. Once I realized this it was obvious
	   that the complexity should be also incremented by delta for
	   every token. As a side effect, the complexity is now a real
	   number, and actually measures not just the max_order token
	   count, but also the fraction of incomplete n-grams. This
	   seems like the right way to go, especially for models based
	   on regexes, since now we also count the incomplete n-grams
	   at both ends of the line, which adds up to quite a bit over
	   many lines. */

	cat[i].fcomplexity++; /* don't actually need this, but nice to have */
	cat[i].complexity += cat[i].delta;

	/* now adjust the score */
	switch(cat[i].model.type) {
	case simple:
	  multinomial_correction = h ?
	    (log((weight_t)cat[i].complexity) - log((weight_t)h->count)) : 0.0;
	  cat[i].score += 
	    lambda + multinomial_correction + ref - cat[i].renorm;
	  break;
	case sequential:
	default:
	  cat[i].score += lambda + ref - cat[i].renorm;
	  if( tt.order == cat[i].max_order ) {
	    cat[i].score_shannon += shannon_correction;
	  }
	  break;
	}

	if( !k || !NTOH_ID(k->id) ) {
	  /* missing data */
	  cat[i].fmiss++;
	}

	if( tt.order == 1 ) {
	  /* sample variance */
	  cat[i].score_s2 += (cat[i].score - oldscore) * (cat[i].score - oldscore);
	  /* only count medium for 1-grams */
	  cat[i].mediacounts[tt.cls]++;
	}

      }

      if( u_options & (1<<U_OPTION_DUMP) ) {
	if( u_options & (1<<U_OPTION_SCORES) ) {
	  fprintf(stdout, " %8.2f * %-6.1f\t",  
		  -sample_mean(cat[i].score, cat[i].complexity),
		  cat[i].complexity);
	} else if( u_options & (1<<U_OPTION_POSTERIOR) ) {
	  fprintf(stdout, " %8.2f\t", oldscore - cat[i].score);
	} else if( u_options & (1<<U_OPTION_VAR) ) {
	  fprintf(stdout, " %8.2f * %-6.1f # %-8.2f\t",
		  -sample_mean(cat[i].score,cat[i].complexity),
		  cat[i].complexity,
		  sample_variance(cat[i].score_s2, cat[i].score,
				  cat[i].complexity)/cat[i].complexity);
	} else {
	  fprintf(stdout, 
		  "%7.2f %7.2f %7.2f %7.2f %8lx\t", 
		  lambda, ref, apply ? -cat[i].renorm : 0.0, 
		  multinomial_correction,
		  (long unsigned int)((k && apply) ? NTOH_ID(k->id) : 0));
	}
      }

    }

    if( u_options & (1<<U_OPTION_DUMP) ) {
      print_token(stdout, tok);
      if( re > 0 ) {
	fprintf(stdout, "<re=%d>\n", re);
      } else {
	fprintf(stdout, "\n");
      }
    }

  }
}

/*
 * Returns 2 * min[ F(obs), 1 - F(obs) ], and calls it 
 * the "confidence". In reality, this is a type of p-value,
 * which is zero when obs = +/- infinity, and 1 at the median.
 * Under the null hypothesis obs ~ Gamma(alpha, beta), this
 * p-value is uniformly distributed. This is chosen so that
 * we can combine our p-palues with Fisher's chi^2 test, viz.
 * -2\sum_i=1^n \log X_i ~ chi^2(2n), if all X_i are IID uniform.
 *
 */
confidence_t gamma_pvalue(category_t *cat, double obs) {
  double m;
  if( (obs <= 0.0) || (cat->alpha <= 0.0) || (cat->beta <= 0.0) ) {
    return 0; /* bogus value */
  } else if( obs > 120.0 ) {
    /* large values can produce underflow in gamma_tail() */
    return 0; /* bogus value */
  }

  m = gamma_tail(cat->alpha, cat->beta, obs);

  /* uncomment below to get one sided confidence */
  /*   return (int)(1000.0 * m); */
  /* result is per-thousand */
  return (confidence_t)(2000.0 * ((m > 0.5) ? 1.0 - m : m));
}

/***********************************************************
 * FILE MANAGEMENT FUNCTIONS                               *
 ***********************************************************/
error_code_t load_category_header(FILE *input, category_t *cat) {
  char buf[MAGIC_BUFSIZE];
  char scratchbuf[MAGIC_BUFSIZE];
  short int shint_val, shint_val2;
  long int lint_val1, lint_val2, lint_val3;

  if( input ) {
    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	strncmp(buf, MAGIC1, MAGIC1_LEN) ) {
      errormsg(E_ERROR,
	       "not a dbacl " SIGNATURE " category file [%s]\n",
	       cat->fullfilename);
      return 0;
    } 

    init_category(cat); /* changes filename */

    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	(sscanf(buf, MAGIC2_i, &cat->divergence, &cat->logZ, 
		&shint_val, scratchbuf) < 4) ) {
      errormsg(E_ERROR, "bad category file [2]\n");
      return 0;
    }
    cat->max_order = (token_order_t)shint_val;
    cat->delta = 1.0/(score_t)(cat->max_order);
    cat->renorm = cat->delta * cat->logZ;
    if( scratchbuf[0] == 'm' ) {
      cat->model.type = simple;
    } else {
      cat->model.type = sequential;
    }

    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	(sscanf(buf, MAGIC3, 
		&shint_val,
		&lint_val1,
		&lint_val2,
		&lint_val3) < 4) ) {
      errormsg(E_ERROR, "bad category file [3]\n");
      return 0;
    }
    cat->max_hash_bits = (token_order_t)shint_val;
    cat->model_full_token_count = (token_count_t)lint_val1;
    cat->model_unique_token_count = (token_count_t)lint_val2;
    cat->model_num_docs = (document_count_t)lint_val3;

    cat->max_tokens = (1<<cat->max_hash_bits);

    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	(sscanf(buf, MAGIC8_i, 
		&cat->shannon, &cat->shannon_s2) < 2) ) {
      errormsg(E_ERROR, "bad category file [8]\n");
      return 0;
    }

    if( !fgets(buf, MAGIC_BUFSIZE, input) ||
	(sscanf(buf, MAGIC10_i, 
		&cat->alpha, &cat->beta,
		&cat->mu, &cat->s2) < 4) ) {
      errormsg(E_ERROR, "bad category file [10]\n");
      return 0;
    }

    /* see if there are any regexes */
    while(fgets(buf, MAGIC_BUFSIZE, input)) {
      if( strncmp(buf, MAGIC6, 2) == 0 ) {
	break;
      } else if( strncmp(buf, MAGIC5_i, 8) == 0 ) {

	/* if regex can't be compiled, load_regex() exits */
	cat->retype |= (1<<load_regex(buf + RESTARTPOS));

      } else if( strncmp(buf, MAGIC4_i, 10) == 0) {
	if( sscanf(buf, MAGIC4_i, &lint_val1, &shint_val, &shint_val2, scratchbuf) == 4 ) {
	  cat->model.options = (options_t)lint_val1;
	  cat->model.cp = (charparser_t)shint_val;
	  cat->model.dt = (digtype_t)shint_val2;
	}
      }

      /* finished with current line, get next one */
    }

    /* if this category did not register a regex, it wants
       the default processing, so we flag this */
    if( !cat->retype ) {
      cat->model.options |= (1<<M_OPTION_USE_STDTOK);
    }

    /* if we haven't read a character class, use alpha */
    if( cat->model.cp == CP_DEFAULT ) {
      if( cat->model.options & (1<<M_OPTION_MBOX_FORMAT) ) {
	cat->model.cp = CP_ADP;
      } else {
	cat->model.cp = CP_ALPHA;
      }
    }

    /* if we're here, success! */
    return 1;
  }
  return 0;
}


error_code_t explicit_load_category(category_t *cat, char *openf, int protf) {
  hash_count_t i, j;

  FILE *input;

  /* this is needed in case we try to open with write permissions,
     which would otherwise create the file */

  input = fopen(cat->fullfilename, "rb");
  if( input && (strcmp(openf, "rb") != 0) ) {
    input = freopen(cat->fullfilename, openf, input);
  }

  if( input ) {

    if( !load_category_header(input, cat) ) {
      fclose(input);
      return 0;
    }

    /* read character frequencies */
    i = ASIZE * ASIZE;
    j = 0;
    while(!ferror(input) && !feof(input) && (j < i) ) {
      j += fread(cat->dig + j, SIZEOF_DIGRAMS, i - j, input);
    }
    if( j < i ) {
      errormsg(E_ERROR, "is this category corrupt: %s?\n",
	      cat->fullfilename);
      fclose(input);
      return 0;
    }

#if defined PORTABLE_CATS
    for(i = 0; i < ASIZE; i++) {
      for(j = 0; j < ASIZE; j++) {
	cat->dig[i][j] = NTOH_DIGRAM(cat->dig[i][j]);
      }
    }
#endif

    if( !create_category_hash(cat, input, protf) ) {
      fclose(input);
      return 0;
    }

    fclose(input);

    return 1;
  }

  return 0;
}


/* loads a category hash 
   returns 0 on failure, you should free the category in that case */
error_code_t load_category(category_t *cat) {
  return explicit_load_category(cat, "rb", PROT_READ);
}

/* loads a category file for potential read/write */
error_code_t open_category(category_t *cat) {
  return explicit_load_category(cat, "r+b", PROT_READ|PROT_WRITE);
}

error_code_t reload_category(category_t *cat) {
  if( cat ) {
    /* free the hash, but keep the cat->fullfilename */
    free_category_hash(cat);
    return load_category(cat) && 
      sanitize_model_options(&m_options,&m_cp,cat);
  }
  return 0;
}

void reload_all_categories() {
  category_count_t c;
  for(c = 0; c < cat_count; c++) {
    if( !reload_category(&cat[c]) ) {
      errormsg(E_FATAL,
	      "could not reload %s, exiting\n", cat[c].fullfilename);
    }
  }
}
