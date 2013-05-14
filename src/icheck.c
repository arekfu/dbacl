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

#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "dbacl.h" /* make sure this is last */

extern options_t u_options;
extern options_t m_options;
extern digtype_t m_dt;

extern category_t cat[MAX_CAT];
extern category_count_t cat_count;
extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;
extern empirical_t empirical;

extern int cmd;
char *progname = "icheck";
char *inputfile = "";
long inputline = 0;

extern long system_pagesize;
extern void *in_iobuf;
extern void *out_iobuf;

void dump(category_t *mycat) {
  c_item_t *i, *e;
  if( u_options & (1<<U_OPTION_DUMP) ) {
    fprintf(stdout, "# lambda | id\n");
    e = mycat->hash + mycat->max_tokens;
    for(i = mycat->hash; i != e; i++) {
      if( FILLEDP(i) ) {
	fprintf(stdout, "%9.3f %8lx\n",
		UNPACK_LAMBDA(NTOH_LAMBDA(i->lam)), 
		(long unsigned int)NTOH_ID(i->id));
      }
    }
  }
}

void integrity_check2(category_t *mycat) {
  int i,j;
  float five;

  if( m_dt == DT_UNIFORM ) {
    five = UNPACK_DIGRAMS(PACK_DIGRAMS(-log(ASIZE - AMIN)));

    for(i = AMIN; i < ASIZE; i++) {
      for(j = AMIN; j < ASIZE; j++) {
	if( fabs(five - UNPACK_DIGRAMS(cat->dig[i][j])) > 0.01 ) {
	  errormsg(E_ERROR, "Laplace digram failure (dig[%d][%d]=%f)", 
		   i, j, UNPACK_DIGRAMS(cat->dig[i][j]));
	  exit(1);
	}
      }
    }

  }
}

void integrity_check1(category_t *mycat) {
  hash_count_t c;
  hash_count_t n = 0;

  for(c = 0; c < mycat->max_tokens; c++) {
    if( FILLEDP(&(mycat->hash[c])) ) {
      if( UNPACK_LWEIGHTS(mycat->hash[c].lam) < 0.0 ) {
	errormsg(E_ERROR, "negative lambda weight c = %ld, %s\n", 
		 (long int)c, mycat->fullfilename);
	exit(1);
      }
      n++;
    }
  }
  if( n != mycat->model_unique_token_count ) {
    errormsg(E_ERROR, "wrong number of filled slots %s\n", mycat->fullfilename);
    exit(1);
  }
}


/* This code intended for integrity checks returns 0 on success, and 1
 * on failure.
 */
int main(int argc, char **argv) {
  int i;

  if( argc < 2) { exit(1); }

  for(i = 1; i < argc; i++) {
    if( argv[i] ) {
      if( argv[i][0] == '-' ) {
	switch(argv[i][1]) {
	case 'd':
	  u_options |= (1<<U_OPTION_DUMP);
	  break;
	case 'u':
	  m_dt = DT_UNIFORM;
	  break;
	default:
	  errormsg(E_ERROR, "unrecognized option %s\n", argv[i]);
	  exit(1);
	  break;
	}
      } else {
	cat[cat_count].fullfilename = argv[i];
	if( !load_category(&cat[cat_count]) ) {
	  errormsg(E_ERROR, "couldn't load %s\n", cat[cat_count].fullfilename);
	  exit(1);
	}

	integrity_check1(&cat[cat_count]);
	integrity_check2(&cat[cat_count]);

	dump(&cat[cat_count]);

	cat_count++;
      }
    }
  }

  exit(0);
}
