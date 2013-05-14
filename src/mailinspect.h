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
#ifndef MAILINSPECT_H
#define MAILINSPECT_H

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dbacl.h"

typedef long seek_t;
typedef u_int32_t line_count_t;
typedef u_int32_t email_count_t;
typedef u_int16_t rank_t;

#define INITIAL_LIST_SIZE 256
#define PIPE_BUFLEN 2048
#define HEADER_BUFLEN 128



#define SORT_INCREASING  1
#define SORT_DECREASING -1

#define TAGRE_INCLUDE   0
#define TAGRE_EXCLUDE   1

#define MAX_FORMATS     2
#define MAX_SCORES      4

/* make sure these options don't interfere with those options 
   defined in dbacl.h which we want to use */

#define U_OPTION_INTERACTIVE 29
#define U_OPTION_REVSORT     30
#define U_OPTION_FORMAT      31

#define STATE_TAGGED       2
#define STATE_LIMITED      3


/* data structures */
typedef struct {
  char from[HEADER_BUFLEN];
  char subject[HEADER_BUFLEN];
} email_header;

typedef struct {
  seek_t seekpos;
  char *description[MAX_FORMATS];
  weight_t score[MAX_SCORES];
  char state;
} mbox_item;

typedef struct {
  mbox_item *list;
  mbox_item **llist;
  email_header header;
  email_count_t list_size;
  email_count_t num_limited;
  email_count_t num_emails;
  int sortedby;
  char *filename;
  unsigned char index_format;
  unsigned char score_type;
} Emails;

typedef struct {
  int num_rows;
  int num_cols;
  bool_t delay_sigwinch;
  int highlighted;
  email_count_t first_visible;
  char *fkey_cmd[10];
} display_state;


void redraw_current_state();

#endif
