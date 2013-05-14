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

#include "dbacl.h"
#include "hmine.h"

char *progname;
char *inputfile;
long inputline;

int cmd = 0;

options_t u_options = 0;
options_t m_options = 0;

charparser_t m_cp = 0;
digtype_t m_dt = 0;

/* default value in case we don't have getpagesize() */
long system_pagesize = BUFSIZ;

category_t cat[MAX_CAT];
category_count_t cat_count = 0;

/* the myregex_t array contains both regexes (first half) and antiregexes
   (second half) */
myregex_t re[MAX_RE];
regex_count_t regex_count = 0;
regex_count_t antiregex_count = 0;

char *extn = "";
empirical_t empirical;

MBOX_State mbox;
XML_State xml;
HEADER_State head;

char *textbuf = NULL;
charbuf_len_t textbuf_len = 0;

#if defined HAVE_MBRTOWC
wchar_t *wc_textbuf = NULL;
charbuf_len_t wc_textbuf_len = 0;
#endif

char *aux_textbuf = NULL;
charbuf_len_t aux_textbuf_len = 0;

hash_bit_count_t default_max_hash_bits = 15;
hash_count_t default_max_tokens = (1<<15);

hash_bit_count_t default_max_grow_hash_bits = 16;
hash_count_t default_max_grow_tokens = (1<<16);

void *in_iobuf = NULL; /* only used for input stream */
void *out_iobuf = NULL; /* used for all category/learner file operations */

token_order_t ngram_order = 1;


decoding_cache b64_dc = {NULL, NULL, 0, 0};
decoding_cache qp_dc = {NULL, NULL, 0, 0};

#if defined HAVE_MBRTOWC
w_decoding_cache w_b64_dc = {NULL, NULL, 0, 0};
w_decoding_cache w_qp_dc = {NULL, NULL, 0, 0};
#endif
