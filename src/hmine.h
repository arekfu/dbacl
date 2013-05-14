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
#ifndef HFORGE_H
#define HFORGE_H

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>

typedef struct {
  char *begin;
  char *end;
} token_delim_t;

#define NULLTOKEN(t) ((t).begin && ((t).end > (t).begin))

/* format specific parse structures */

typedef struct {
  token_delim_t path_;
} parse_822_pth_t;

typedef struct {
  token_delim_t path_;
} parse_2822_pth_t;

typedef struct {
  token_delim_t from_;
  token_delim_t by_;
  token_delim_t via_;
  token_delim_t withl_;
  token_delim_t id_;
  token_delim_t for_;
  token_delim_t datetime_;
} parse_822_rcv_t;

typedef struct {
  token_delim_t naval_;
  token_delim_t from_;
  token_delim_t by_;
  token_delim_t via_;
  token_delim_t withl_;
  token_delim_t id_;
  token_delim_t for_;
  token_delim_t datetime_;
} parse_2822_rcv_t;

typedef struct {
  token_delim_t from_;
  token_delim_t by_;
  token_delim_t via_;
  token_delim_t with_;
  token_delim_t id_;
  token_delim_t for_;
  token_delim_t datetime_;
} parse_821_rcv_t;

typedef struct {
  token_delim_t from_;
  token_delim_t by_;
  token_delim_t via_;
  token_delim_t with_;
  token_delim_t smid_;
  token_delim_t for_;
  token_delim_t datetime_;
} parse_2821_rcv_t;

typedef struct {
  token_delim_t addressl_;
} parse_822_als_t;

typedef struct {
  token_delim_t addressl_;
} parse_2822_als_t;

typedef struct {
  token_delim_t mailboxl_;
} parse_822_mls_t;

typedef struct {
  token_delim_t mailboxl_;
} parse_2822_mls_t;

typedef struct {
  token_delim_t mailbox_;
} parse_822_mbx_t;

typedef struct {
  token_delim_t mailbox_;
} parse_2822_mbx_t;

typedef struct {
  token_delim_t datetime_;
} parse_822_dat_t;

typedef struct {
  token_delim_t datetime_;
} parse_2822_dat_t;

typedef struct {
  token_delim_t msg_id_;
} parse_822_mid_t;

typedef struct {
  token_delim_t msg_id_;
} parse_2822_mid_t;

typedef struct {
  token_delim_t refs_;
} parse_822_ref_t;

typedef struct {
  token_delim_t refs_;
} parse_2822_ref_t;

typedef struct {
  token_delim_t phrasel_;
} parse_822_pls_t;

typedef struct {
  token_delim_t text_;
} parse_822_txt_t;

typedef struct {
  token_delim_t text_;
} parse_2822_txt_t;

typedef struct {
  char *line;
} ignored_t;

/* generic parse structures */

typedef struct {
  parse_822_pth_t rfc821;
  parse_822_pth_t rfc822;
  parse_2822_pth_t rfc2821;
  parse_2822_pth_t rfc2822;
} return_path_t;

typedef struct {
  parse_821_rcv_t rfc821;
  parse_822_rcv_t rfc822;
  parse_2821_rcv_t rfc2821;
  parse_2822_rcv_t rfc2822;
  time_t numsec;
} received_t;

typedef struct {
  parse_822_als_t rfc822;
  parse_2822_als_t rfc2822;
} reply_to_t;

typedef struct {
  parse_822_mls_t rfc822;
  parse_2822_mls_t rfc2822;
} from_t;

typedef struct {
  parse_822_mbx_t rfc822;
  parse_2822_mbx_t rfc2822;
} sender_t;

typedef struct {
  parse_822_dat_t rfc822;
  parse_2822_dat_t rfc2822;
  time_t numsec;
} dat_t;

typedef struct {
  parse_822_als_t rfc822;
  parse_2822_als_t rfc2822;
} to_t;

typedef struct {
  parse_822_als_t rfc822;
  parse_2822_als_t rfc2822;
} cc_t;

typedef struct {
  parse_822_als_t rfc822;
  parse_2822_als_t rfc2822;
} bcc_t;

typedef struct {
  parse_822_mid_t rfc822;
  parse_2822_mid_t rfc2822;
} message_id_t;

typedef struct {
  parse_822_ref_t rfc822;
  parse_2822_ref_t rfc2822;
} in_reply_to_t;

typedef struct {
  parse_822_ref_t rfc822;
  parse_2822_ref_t rfc2822;
} references_t;

typedef struct {
  parse_822_txt_t rfc822;
  parse_2822_txt_t rfc2822;
} subject_t;


#define H_STATE_OK               1
#define H_STATE_BAD_LABEL        2
#define H_STATE_RFC2821          3
#define H_STATE_RFC2821loc       4
#define H_STATE_RFC2822          5
#define H_STATE_RFC2822obs       6
#define H_STATE_RFC821           7
#define H_STATE_RFC822           8
#define H_STATE_RFC2822lax       9
#define H_STATE_BAD_DATA         10
#define H_STATE_RFC2821lax       11

typedef enum {
  hltUNDEF, hltIGN,
  hltRET, hltRCV,
  hltRPT, hltFRM,
  hltSND, hltRRT,
  hltRFR, hltRSN,
  hltDAT, hltRDT,  
  hltTO, hltRTO,
  hltCC, hltRCC,
  hltBCC, hltRBC,
  hltMID, hltRID,
  hltIRT,
  hltREF, hltSBJ
} HLINE_Tag;

typedef enum {
  hleNONE, hleLABEL
} HLINE_Error;

typedef struct {
  options_t state;
  HLINE_Tag tag;
  charbuf_len_t toffset;
  union {
    ignored_t ign;
    return_path_t ret;
    received_t rcv;
    reply_to_t rpt;
    reply_to_t rrt;
    from_t frm;
    from_t rfr;
    sender_t snd;
    sender_t rsn;
    dat_t dat;
    dat_t rdt;
    to_t to;
    to_t rto;
    cc_t cc;
    cc_t rcc;
    bcc_t bcc;
    bcc_t rbc;
    message_id_t mid;
    message_id_t rid;
    in_reply_to_t irt;
    references_t ref;
    subject_t sbj;
  } data;
} hline_t;

typedef int hline_count_t;

typedef struct {
  struct {
    char *textbuf;
    char *textbuf_end;
    charbuf_len_t textbuf_len;
    charbuf_len_t curline;
  } hdata;
  struct {
    hline_t *hlines;
    hline_count_t max;
    hline_count_t top;
  } hstack;
} HEADER_State;

void init_head_filter(HEADER_State *head);
void free_head_filter(HEADER_State *head);
char *head_append_hline(HEADER_State *head, const char *what);
hline_t *head_push_header(HEADER_State *head, char *line);

void print_token_delim(FILE *out, char *sym, token_delim_t *d);

/* useful macros */
#define BOT if( tok ) { tok->begin = line; }
#define EOT if( tok ) { tok->end = line; }

/* #define DT(x) print_token_delim(stdout, (x), tok); */
#define DT(x) 

char *parse_822_return(char *line, parse_822_pth_t *p);
char *parse_822_received(char *line, parse_822_rcv_t *p);
char *parse_822_reply_to(char *line, parse_822_als_t *p);
char *parse_822_from(char *line, parse_822_mls_t *p);
char *parse_822_sender(char *line, parse_822_mbx_t *p);
char *parse_822_resent_reply_to(char *line, parse_822_als_t *p);
char *parse_822_resent_from(char *line, parse_822_mls_t *p);
char *parse_822_resent_sender(char *line, parse_822_mbx_t *p);
char *parse_822_date(char *line, parse_822_dat_t *p);
char *parse_822_resent_date(char *line, parse_822_dat_t *p);
char *parse_822_to(char *line, parse_822_als_t *p);
char *parse_822_resent_to(char *line, parse_822_als_t *p);
char *parse_822_cc(char *line, parse_822_als_t *p);
char *parse_822_resent_cc(char *line, parse_822_als_t *p);
char *parse_822_bcc(char *line, parse_822_als_t *p);
char *parse_822_resent_bcc(char *line, parse_822_als_t *p);
char *parse_822_message_id(char *line, parse_822_mid_t *p);
char *parse_822_resent_message_id(char *line, parse_822_mid_t *p);
char *parse_822_in_reply_to(char *line, parse_822_ref_t *p);
char *parse_822_references(char *line, parse_822_ref_t *p);
char *parse_822_keywords(char *line, parse_822_pls_t *p);
char *parse_822_subject(char *line, parse_822_txt_t *p);
char *parse_822_comments(char *line, parse_822_txt_t *p);

char *parse_821_return_path_line(char *line, parse_822_pth_t *p);
char *parse_821_time_stamp_line(char *line, parse_821_rcv_t *p);

#define O_2822_OBS                  1
#define O_2821_NO_SP_BEFORE_DATE    2
#define O_2822_ALLOW_NFQDN          3
#define O_2821_NO_PARENS_ADDRESS    4

#define RFC_2822_STRICT     0
#define RFC_2821_STRICT     0
#define RFC_2822_OBSOLETE   (1<<O_2822_OBS)
#define RFC_2822_LAX        (1<<O_2822_OBS)|(1<<O_2822_ALLOW_NFQDN)
#define RFC_2821_LAX        (1<<O_2821_NO_SP_BEFORE_DATE)|(1<<O_2822_ALLOW_NFQDN)|(1<<O_2821_NO_PARENS_ADDRESS)

char *parse_2822_return(char *line, parse_2822_pth_t *p, options_t opt);
char *parse_2822_received(char *line, parse_2822_rcv_t *p, options_t opt);
char *parse_2822_reply_to(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_from(char *line, parse_2822_mls_t *p, options_t opt);
char *parse_2822_sender(char *line, parse_2822_mbx_t *p, options_t opt);
char *parse_2822_resent_reply_to(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_resent_from(char *line, parse_2822_mls_t *p, options_t opt);
char *parse_2822_resent_sender(char *line, parse_2822_mbx_t *p, options_t opt);
char *parse_2822_date(char *line, parse_2822_dat_t *p, options_t opt);
char *parse_2822_resent_date(char *line, parse_2822_dat_t *p, options_t opt);
char *parse_2822_to(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_resent_to(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_cc(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_resent_cc(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_bcc(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_resent_bcc(char *line, parse_2822_als_t *p, options_t opt);
char *parse_2822_message_id(char *line, parse_2822_mid_t *p, options_t opt);
char *parse_2822_resent_message_id(char *line, parse_2822_mid_t *p, options_t opt);
char *parse_2822_in_reply_to(char *line, parse_2822_ref_t *p, options_t opt);
char *parse_2822_references(char *line, parse_2822_ref_t *p, options_t opt);
char *parse_2822_subject(char *line, parse_2822_txt_t *p, options_t opt);

char *parse_2821_return_path_line(char *line, parse_2822_pth_t *p, options_t opt);
char *parse_2821_time_stamp_line(char *line, parse_2821_rcv_t *p, options_t opt);

char *unfold_token(char *buf, int buflen, char *tokb, char *toke, char delim);
bool_t bracketize_mailbox(char *buf, int buflen);

#endif
