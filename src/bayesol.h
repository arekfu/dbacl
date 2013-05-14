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

#ifndef BAYESOL_H
#define BAYESOL_H

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <regex.h>

#include "dbacl.h"

typedef double real_value_t;
typedef u_int8_t submatch_order_t;

#define BUFLEN ((charbuf_len_t)1024)

/* options */
#define OPTION_RISKSPEC     1
#define OPTION_SCORES       4
#define OPTION_VERBOSE      5
#define INPUT_FROM_CMDLINE  6
#define OPTION_FILTER       10
#define OPTION_DEBUG        15
#define OPTION_I18N         16
#define OPTION_SCORES_EX    17

/* when the ratio of complexities is below this value, flag a problem */
#define MEANINGLESS_THRESHOLD 0.9

/* macros */

/* data structures */

typedef struct lvec {
  char *re;
  char *ve;
  bool_t found;
  struct lvec *next;
  real_value_t sm[MAX_SUBMATCH];
} LossVector;

typedef struct regm {
  regex_t reg;

  LossVector *lv;
  struct regm *next;
} RegMatch;

typedef struct {
  category_count_t num_cats;
  category_count_t num_priors;
  char* catname[MAX_CAT];
  real_value_t prior[MAX_CAT];
  LossVector* loss_list[MAX_CAT];
  RegMatch *regs;
  real_value_t cross_entropy[MAX_CAT];
  real_value_t complexity[MAX_CAT];
  real_value_t loss_matrix[MAX_CAT][MAX_CAT];
} Spec;



#endif
