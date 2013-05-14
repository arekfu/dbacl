/* 
 * Copyright (C) 2004 Laird Breyer
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
#include <ctype.h>
#include <stdlib.h>

#include "dbacl.h"
#include "hmine.h"
#include "util.h"


/***********************************************************
 * PARSING FUNCTIONS RFC 822/821                           *
 * The skip functions operate as follows: if line is NULL, *
 * the function returns NULL. Otherwise, line is returned  *
 * as a pointer to the first character after the skipped   *
 * pattern. If the patterns couldn't be traversed          *
 * successfully, the function returns NULL.                *
 ***********************************************************/

/***********************************************************
 * GENERAL LEXICAL TOKENS                                  *
 ***********************************************************/

static __inline__
char *skip_recursive(char *line, char opening, char closing, char quote) {
  int c = 1;
  if( !line || !(*line == opening) ) { return NULL; }

  line++;
  while( *line ) {
    /* when closing == opening, we never increment c */
    if( *line == closing ) {
      c--;
    } else if( *line == opening ) {
      c++;
    } else if( *line == quote ) {
      line++;
    }
    line++;
    if( c <= 0 ) {
      break;
    }
  }
  return (c == 0) ? line : NULL;
}

static __inline__
char *skip_single_char(char *line, char what) {
  if( !line || (*line != what) ) { return NULL; }
  return ++line;
}

static __inline__
char *skip_twodigit(char *line) {
  if( !line || !isdigit((int)line[0]) || !isdigit((int)line[1]) ) { return NULL; }
  line += 2;
  return line;
}

static __inline__
char *skip_fourdigit(char *line) {
  if( !line || !isdigit((int)line[0]) || !isdigit((int)line[1]) ||
      !isdigit((int)line[2]) || !isdigit((int)line[3]) ) { return NULL; }
  line += 4;
  return line;
}

static __inline__
char *skip_single_string(char *line, char *what) {
  if( !line || (strncasecmp(line, what, strlen(what)) != 0) ) { return NULL; }
  return line + strlen(what);
}

static __inline__
char *skip_string_list(char *line, char *list[], int n) {
  int i;
  if( !line ) { return NULL; }
  for(i = 0; i < n; i++) {
    if( strncasecmp(line, list[i], strlen(list[i])) == 0 ) {
      return line + strlen(list[i]);
    }
  }
  return NULL;
}

/***********************************************************
 * RFC 822 LEXICAL TOKENS                                  *
 ***********************************************************/

/* comments can contain basically any character, except an unquoted
 * \r. However, a fold can exist within a comment, so \r\n is
 * acceptable. Since the OS may have converted this sequence, it's
 * possible this shows up as \r only. So we accept unquoted \r in
 * comments also. */
static __inline__
char *skip_822_comment(char *line, token_delim_t *tok) {
  BOT;
  line = skip_recursive(line, '(', ')', '\\');
  EOT;
  DT("822_comment");
  return line;
}

/* This accepts not just LWSP-char, but also \r, \n, because folds can
 * happen just about anywhere.  Also, parsing is slightly wrong: we
 * don't check for "\r\n" as a fold, but accept "\r", or "\n"; this
 * isn't just lazyness: it's also possible the OS has already
 * converted end of line sequences before we see them. */
static __inline__
char *skip_822_lwsp(char *line, int min) {
  if( !line ) { return NULL; }
  for(; min-- > 0; line++) {
    if( !isspace((int)*line) ) { return NULL; }
  }
  while(line) {
    if( isspace((int)*line) ) {
      line++;
    } else if( *line == '(' ) {
      line = skip_822_comment(line, NULL);
    } else {
      break;
    }
  }
  return line;
}

/*
 * parsing is slightly wrong: we don't check for "\r\n[ \t]+"
 * sequences, this isn't just lazyness: it's also possible the OS has
 * converted end of line sequences before we see them. */
static __inline__
char *skip_822_linwsp(char *line) {
  return skip_822_lwsp(line, 1);
}

static __inline__
char *assert_char(char *line, char c) {
  return (line && (*line == c)) ? line : NULL;
}

static __inline__
char *skip_822_sharp(char *line, token_delim_t *tok, 
		     char *(*skip)(char *,token_delim_t *), int min) {
  char *tmp;
  BOT;
  while( line && (min > 0) ) {
    while( line && (*line == ',') ) {
      min--;
      line = skip_822_lwsp(line, 0);
    }
    line = (*skip)(line, NULL);
    line = skip_822_lwsp(line, 0);
    min--;
  }
  tmp = line;
  while( tmp ) {
    while( tmp && (*tmp == ',') ) {
      tmp = skip_822_lwsp(tmp + 1, 0);
    }
    if( tmp ) { line = tmp; }
    tmp = (*skip)(tmp, NULL);
    tmp = skip_822_lwsp(tmp, 0);
    if( tmp ) { line = tmp; }
  }
  EOT;
  DT("822_sharp");
  return line;
}

char *skip_822_seq(char *line, token_delim_t *tok, 
	       char *(*skip)(char *,token_delim_t *), char delim) {
  char *tmp;
  BOT;
  line = (*skip)(line, NULL);
  tmp = skip_822_lwsp(line, 0);
  tmp = skip_single_char(tmp, delim);
  while( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = (*skip)(line, NULL);
    tmp = skip_822_lwsp(line, 0);
    tmp = skip_single_char(tmp, delim);
  }
  EOT;
  DT("821_seq");
  return line;
}

static char rfc822_atom[256] = {
   -1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0, '!',   0, '#', '$', '%', '&','\'',   0,   0, '*', '+',   0, '-',   0, '/',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',   0,   0,   0, '=',   0, '?',
    0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',   0,   0,   0, '^', '_',
  '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',  0,
};

static __inline__
char *skip_822_atom(char *line, token_delim_t *tok) {
  if( !line || (rfc822_atom[(unsigned int)*line] != *line) ) { return NULL; }
  BOT;
  while( rfc822_atom[(unsigned int)*line] == *line ) {
    line++;
  }
  EOT;
  DT("822_atom");
  return line;
}

static __inline__
char *skip_822_quoted_string(char *line, token_delim_t *tok) {
  BOT;
  line = skip_recursive(line, '"', '"', '\\');
  EOT;
  DT("822_quoted_string");
  return line;
}

/* normally text can't include "\r\n", but we're lenient ;) */
static __inline__
char *skip_822_text(char *line, token_delim_t *tok) {
  if( !line ) { return NULL; }
  BOT;
  while( *line ) { line++; }
  EOT;
  DT("822_text");
  return line;
}

static __inline__
char *skip_822_domain_literal(char *line, token_delim_t *tok) {
  BOT;
  line = skip_recursive(line, '[', ']', '\\');
  EOT;
  DT("822_domain_literal");
  return line;
}

static __inline__
char *skip_822_word(char *line, token_delim_t *tok) {
  BOT;
  if( line && (*line == '"') ) {
    line = skip_822_quoted_string(line, NULL);
  } else {
    line = skip_822_atom(line, NULL);
  }
  EOT;
  DT("822_word");
  return line;
}

static __inline__
char *skip_822_phrase(char *line, token_delim_t *tok) {
  char *tmp;
  BOT;
  line = skip_822_word(line, NULL);
  tmp = skip_822_lwsp(line, 0);
  tmp = skip_822_word(tmp, NULL);
  while(tmp) {
    line = tmp;
    tmp = skip_822_lwsp(line, 0);
    tmp = skip_822_word(tmp, NULL);
  }
  EOT;
  DT("822_phrase");
  return line;
}

static __inline__
char *skip_822_phrase_list(char *line, token_delim_t *tok, int min) {
  line = skip_822_sharp(line, tok, skip_822_phrase, min);
  DT("822_phrase_list");
  return line;
}

static __inline__
char *skip_822_domain_ref(char *line, token_delim_t *tok) {
  return skip_822_atom(line, tok);
}

static __inline__
char *skip_822_sub_domain(char *line, token_delim_t *tok) {
  BOT;
  if( line && (*line == '[') ) {
    line = skip_822_domain_literal(line, NULL);
  } else {
    line = skip_822_domain_ref(line, NULL);
  }
  EOT;
  DT("822_sub_domain");
  return line;
}

static __inline__
char *skip_822_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_seq(line, tok, skip_822_sub_domain, '.');
  EOT;
  DT("822_domain");
  return line;
}

static __inline__
char *skip_822_local_part(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_seq(line, tok, skip_822_word, '.');
  EOT;
  DT("822_local_part");
  return line;
}

static __inline__
char *skip_822_addr_spec(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_local_part(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, '@');
  line = skip_822_lwsp(line, 0);
  line = skip_822_domain(line, NULL);
  EOT;
  DT("822_addr_spec");
  return line;
}

static __inline__
char *skip_822_msg_id(char *line, token_delim_t *tok) {
  BOT;
  line = skip_single_char(line, '<');
  line = skip_822_lwsp(line, 0);
  line = skip_822_addr_spec(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, '>');
  EOT;
  DT("822_msg_id");
  return line;
}

static __inline__
char *skip_822_at_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_single_char(line, '@');
  line = skip_822_lwsp(line, 0);
  line = skip_822_domain(line, NULL);
  EOT;
  DT("822_at_domain");
  return line;
}

static __inline__
char *skip_822_route(char *line, token_delim_t *tok) {
  line = skip_822_sharp(line, tok, skip_822_at_domain, 1);
  line = skip_822_lwsp(line,0);
  line = skip_single_char(line, ':');
  EOT;
  DT("822_route");
  return line;
}

static __inline__
char *skip_822_route_addr(char *line, token_delim_t *tok) {
  char *tmp;
  BOT;
  line = skip_single_char(line, '<');
  line = skip_822_lwsp(line, 0);
  tmp = skip_822_route(line, NULL);
  if( tmp ) { 
    line = tmp; 
    line = skip_822_lwsp(line, 0);
  }
  line = skip_822_addr_spec(line, NULL);
  line = skip_single_char(line, '>');
  EOT;
  DT("822_route_addr");
  return line;
}

static __inline__
char *skip_822_mailbox(char *line, token_delim_t *tok) {
  char *tmp = line;
  BOT;
  line = skip_822_addr_spec(line, NULL);
  if( !line ) {
    line = skip_822_phrase(tmp, NULL);
    line = skip_822_lwsp(line, 0);
    line = skip_822_route_addr(line, NULL);
  }
  EOT;
  DT("822_mailbox");
  return line;
}


static __inline__
char *skip_822_mailbox_list(char *line, token_delim_t *tok, int min) {
  line = skip_822_sharp(line, tok, skip_822_mailbox, min);
  DT("822_mailbox_list");
  return line;
}

static __inline__
char *skip_822_field_name(char *line, char *field_no_colon) {
  line = skip_single_string(line, field_no_colon);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, ':');
  return line;
}

static __inline__
char *skip_822_group(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_phrase(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, ':');
  line = skip_822_lwsp(line, 0);
  line = skip_822_sharp(line, tok, skip_822_mailbox, 0);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, ';');
  EOT;
  DT("822_group");
  return line;
}

static __inline__
char *skip_822_address(char *line, token_delim_t *tok) {
  char *tmp;
  BOT;
  tmp = skip_822_mailbox(line, NULL);
  if( tmp ) {
    line = tmp;
  } else {
    line = skip_822_group(line, NULL);
  }
  EOT;
  DT("822_address");
  return line;
}

static __inline__
char *skip_822_address_list(char *line, token_delim_t *tok, int min) {
  line = skip_822_sharp(line, tok, skip_822_address, min);
  DT("822_address_list");
  return line;
}

static __inline__
char *skip_822_refs(char *line, token_delim_t *tok) {
  BOT;
  while( line ) {
    if( *line == '<' ) {
      line = skip_822_msg_id(line, NULL);
    } else {
      line = skip_822_phrase(line, NULL);
    }
    line = skip_822_lwsp(line, 0);
  }
  EOT;
  DT("822_address");
  return line;
}

static __inline__
char *skip_822_hour(char *line, token_delim_t *tok) {
  char *tmp;
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 0);

  line = skip_single_char(line, ':');
  line = skip_822_lwsp(line, 0);

  line = skip_twodigit(line);

  tmp = skip_822_lwsp(line, 0);
  tmp = skip_single_char(tmp, ':');
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_twodigit(line);
  }
  return line;
}

static __inline__
char *skip_822_zone_name(char *line) {
  static char *zone_name[] = { 
    "UT", "GMT", "EST", "EDT", "CST", "CDT", "MST",
    "MDT", "PST", "PDT"
  };
  return skip_string_list(line, zone_name, sizeof(zone_name)/sizeof(char*));
}

static __inline__
char *skip_822_zone(char *line, token_delim_t *tok) {
  if( !line ) { return NULL; }
  BOT;
  if( isalpha((int)line[0]) ) {
    if( !isalpha((int)line[1]) ) {
      line++;
    } else {
      line = skip_822_zone_name(line);
    }
  } else if( (line[0] == '+') || (line[0] == '-') ) {
    line = skip_822_lwsp(line, 0);
    line = skip_fourdigit(line);
  } else {
    line = NULL;
  }
  EOT;
  DT("822_zone");
  return line;
}

static __inline__
char *skip_822_time(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_hour(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_822_zone(line, NULL);
  EOT;
  DT("822_time");
  return line;
}


static __inline__
char *skip_822_day_name(char *line) {
  static char *day_name[] = { 
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
  };
  return skip_string_list(line, day_name, sizeof(day_name)/sizeof(char*));
}


static __inline__
char *skip_822_month_name(char *line) {
  static char *month_name[] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
    "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  return skip_string_list(line, month_name, sizeof(month_name)/sizeof(char*));
}

static __inline__
char *skip_822_date(char *line, token_delim_t *tok) {
  BOT;
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 0);
  line = skip_822_month_name(line);
  line = skip_822_lwsp(line, 0);
  line = skip_twodigit(line);
  EOT;
  DT("822_date");
  return line;
}

static __inline__
char *skip_822_date_time(char *line, token_delim_t *tok) {
  char *tmp;
  BOT;
  tmp = skip_822_day_name(line);
  if( tmp ) { 
    line = skip_822_lwsp(tmp, 0);
    line = skip_single_char(line, ',');
    line = skip_822_lwsp(line, 0);
  }
  line = skip_822_date(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_822_time(line, NULL);
  EOT;
  DT("822_date_time");
  return line;
}

static __inline__
char *skip_822_end_of_line(char *line) {
  /* there should be nothing left except CRLF or folding space, and
   * the CRLF could be absent or mangled by OS. */
  line = skip_822_lwsp(line,0);
  if( line && *line ) { return NULL; }
  return line;
}

/***********************************************************
 * FULL RFC 822 HEADER LINES                               *
 ***********************************************************/

char *parse_822_return(char *line, parse_822_pth_t *p) {
  memset(p, 0, sizeof(parse_822_pth_t));
  line = skip_822_field_name(line, "Return-path");
  line = skip_822_lwsp(line, 0);
  line = skip_822_route_addr(line, &p->path_);

  return skip_822_end_of_line(line);
}

char *parse_822_received(char *line, parse_822_rcv_t *p) {
  char *tmp;
  memset(p, 0, sizeof(parse_822_rcv_t));
  line = skip_822_field_name(line, "Received");
  line = skip_822_lwsp(line, 0);

  tmp = skip_single_string(line, "from");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_domain(line, &p->from_);
    line = skip_822_lwsp(line, 0);
  }

  tmp = skip_single_string(line, "by");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_domain(line, &p->by_);
    line = skip_822_lwsp(line, 0);
  }

  tmp = skip_single_string(line, "via");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_atom(line, &p->via_);
    line = skip_822_lwsp(line, 0);
  }

  tmp = skip_single_string(line, "with");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_atom(line, &p->withl_);
    line = skip_822_lwsp(line, 0);
  }
  tmp = skip_single_string(line, "with");
  while( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_atom(line, NULL);
    p->withl_.end = line;
    line = skip_822_lwsp(line, 0);
    tmp = skip_single_string(line, "with");
  }
  
  tmp = skip_single_string(line, "id");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_msg_id(line, &p->id_);
    line = skip_822_lwsp(line, 0);
  }

  tmp = skip_single_string(line, "for");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_822_addr_spec(line, &p->for_);
    line = skip_822_lwsp(line, 0);
  }

  line = skip_single_char(line, ';');
  line = skip_822_lwsp(line, 0);
  line = skip_822_date_time(line, &p->datetime_);

  return skip_822_end_of_line(line);
}

char *parse_822_reply_to(char *line, parse_822_als_t *p) {
  memset(p, 0, sizeof(parse_822_als_t));
  line = skip_822_field_name(line, "Reply-To");
  line = skip_822_lwsp(line, 0);
  line = skip_822_address_list(line, &p->addressl_, 1);

  return skip_822_end_of_line(line);
}

char *parse_822_from(char *line, parse_822_mls_t *p) {
  memset(p, 0, sizeof(parse_822_mls_t));
  line = skip_822_field_name(line, "From");
  line = skip_822_lwsp(line, 0);
  line = skip_822_mailbox_list(line, &p->mailboxl_, 1);

  return skip_822_end_of_line(line);
}

char *parse_822_sender(char *line, parse_822_mbx_t *p) {
  memset(p, 0, sizeof(parse_822_mbx_t));
  line = skip_822_field_name(line, "Sender");
  line = skip_822_lwsp(line, 0);
  line = skip_822_mailbox_list(line, &p->mailbox_, 1);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_reply_to(char *line, parse_822_als_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_reply_to(line, p);
}

char *parse_822_resent_from(char *line, parse_822_mls_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_from(line, p);
}

char *parse_822_resent_sender(char *line, parse_822_mbx_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_sender(line, p);
}


char *parse_822_date(char *line, parse_822_dat_t *p) {
  memset(p, 0, sizeof(parse_822_dat_t));
  line = skip_822_field_name(line, "Date");
  line = skip_822_lwsp(line, 0);
  line = skip_822_date_time(line, &p->datetime_);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_date(char *line, parse_822_dat_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_date(line, p);
}

char *parse_822_to(char *line, parse_822_als_t *p) {
  memset(p, 0, sizeof(parse_822_als_t));
  line = skip_822_field_name(line, "To");
  line = skip_822_lwsp(line, 0);
  line = skip_822_address_list(line, &p->addressl_, 1);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_to(char *line, parse_822_als_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_to(line, p);
}

char *parse_822_cc(char *line, parse_822_als_t *p) {
  memset(p, 0, sizeof(parse_822_als_t));
  line = skip_822_field_name(line, "cc");
  line = skip_822_lwsp(line, 0);
  line = skip_822_address_list(line, &p->addressl_, 1);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_cc(char *line, parse_822_als_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_cc(line, p);
}


char *parse_822_bcc(char *line, parse_822_als_t *p) {
  memset(p, 0, sizeof(parse_822_als_t));
  line = skip_822_field_name(line, "bcc");
  line = skip_822_lwsp(line, 0);
  line = skip_822_address_list(line, &p->addressl_, 0);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_bcc(char *line, parse_822_als_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_bcc(line, p);
}

char *parse_822_message_id(char *line, parse_822_mid_t *p) {
  memset(p, 0, sizeof(parse_822_mid_t));
  line = skip_822_field_name(line, "Message-ID");
  line = skip_822_lwsp(line, 0);
  line = skip_822_msg_id(line, &p->msg_id_);

  return skip_822_end_of_line(line);
}

char *parse_822_resent_message_id(char *line, parse_822_mid_t *p) {
  line = skip_single_string(line, "Resent-");
  return parse_822_message_id(line, p);
}

char *parse_822_in_reply_to(char *line, parse_822_ref_t *p) {
  memset(p, 0, sizeof(parse_822_ref_t));
  line = skip_822_field_name(line, "In-Reply-To");
  line = skip_822_lwsp(line, 0);
  line = skip_822_refs(line, &p->refs_);

  return skip_822_end_of_line(line);
}

char *parse_822_references(char *line, parse_822_ref_t *p) {
  memset(p, 0, sizeof(parse_822_ref_t));
  line = skip_822_field_name(line, "References");
  line = skip_822_lwsp(line, 0);
  line = skip_822_refs(line, &p->refs_);

  return skip_822_end_of_line(line);
}

char *parse_822_keywords(char *line, parse_822_pls_t *p) {
  memset(p, 0, sizeof(parse_822_pls_t));
  line = skip_822_field_name(line, "Keywords");
  line = skip_822_lwsp(line, 0);
  line = skip_822_phrase_list(line, &p->phrasel_, 0);

  return skip_822_end_of_line(line);
}

char *parse_822_subject(char *line, parse_822_txt_t *p) {
  memset(p, 0, sizeof(parse_822_txt_t));
  line = skip_822_field_name(line, "Subject");
  line = skip_822_lwsp(line, 0);
  line = skip_822_text(line, &p->text_);

  return skip_822_end_of_line(line);
}

char *parse_822_comments(char *line, parse_822_txt_t *p) {
  memset(p, 0, sizeof(parse_822_txt_t));
  line = skip_822_field_name(line, "Comments");
  line = skip_822_lwsp(line, 0);
  line = skip_822_text(line, &p->text_);

  return skip_822_end_of_line(line);
}

/***********************************************************
 * RFC 821 LEXICAL TOKENS                                  *
 ***********************************************************/

static __inline__
char *skip_821_name(char *line, token_delim_t *tok) {
  if( !line || !isalpha((int)*line) ) { return NULL; }
  BOT;
  while( isalnum((int)*line) || (*line == '-') ) {
    line++;
  }
  EOT;
  DT("821_name");
  return line;
}

static __inline__
char *skip_821_number(char *line, token_delim_t *tok) {
  if( !line || !isdigit((int)*line) ) { return NULL; }
  BOT;
  while( isdigit((int)*line) ) {
    line++;
  }
  EOT;
  DT("821_number");
  return line;
}

static __inline__
char *skip_821_dotnum(char *line, token_delim_t *tok) {
  if( !line ) { return NULL; }
  BOT;
  if( isdigit((int)*line) ) { line++; } else { return NULL; }
  if( isdigit((int)*line) ) { line++; }
  if( isdigit((int)*line) ) { line++; }
  if( *line != '.' ) { return NULL; }
  if( isdigit((int)*line) ) { line++; } else { return NULL; }
  if( isdigit((int)*line) ) { line++; }
  if( isdigit((int)*line) ) { line++; }
  if( *line != '.' ) { return NULL; }
  if( isdigit((int)*line) ) { line++; } else { return NULL; }
  if( isdigit((int)*line) ) { line++; }
  if( isdigit((int)*line) ) { line++; }
  if( *line != '.' ) { return NULL; }
  if( isdigit((int)*line) ) { line++; } else { return NULL; }
  if( isdigit((int)*line) ) { line++; }
  if( isdigit((int)*line) ) { line++; }
  EOT;
  DT("821_dotnum");
  return line;
}

static __inline__
char *skip_821_element(char *line, token_delim_t *tok) {
  BOT;
  if( !line ) { 
    return NULL;
  } else if( *line == '#' ) {
    line = skip_single_char(line, '#');
    line = skip_822_lwsp(line, 0);
    line = skip_821_number(line, NULL);
  } else if( *line == '[' ) {
    line = skip_single_char(line, '[');
    line = skip_822_lwsp(line, 0);
    line = skip_821_dotnum(line, NULL);
    line = skip_822_lwsp(line, 0);
    line = skip_single_char(line, ']');
  } else {
    line = skip_821_name(line, NULL);
  }
  EOT;
  DT("821_element");
  return line;
}

static __inline__
char *skip_821_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_seq(line, tok, skip_821_element, '.');
  EOT;
  DT("821_domain");
  return line;
}

static __inline__
char *skip_821_string(char *line, token_delim_t *tok) {
  return skip_822_atom(line, tok);
}

static __inline__
char *skip_821_dot_string(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_seq(line, tok, skip_821_string, '.');
  EOT;
  DT("821_dot_string");
  return line;
}

static __inline__
char *skip_821_local_part(char *line, token_delim_t *tok) {
  if( !line ) { return NULL; }
  BOT;
  if( *line == '"' ) {
    line = skip_822_quoted_string(line, NULL);
  } else {
    line = skip_821_dot_string(line, NULL);
  }
  EOT;
  DT("821_local_part");
  return line;
}

static __inline__
char *skip_821_mailbox(char *line, token_delim_t *tok) {
  BOT;
  line = skip_821_local_part(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, '@');
  line = skip_822_lwsp(line, 0);
  line = skip_821_domain(line, NULL);
  EOT;
  DT("821_mailbox");
  return line;
}

static __inline__
char *skip_821_at_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_single_char(line, '@');
  line = skip_822_lwsp(line, 0);
  line = skip_821_domain(line, NULL);
  EOT;
  DT("821_at_domain");
  return line;
}

static __inline__
char *skip_821_adl(char *line, token_delim_t *tok) {
  BOT;
  line = skip_822_seq(line, tok, skip_821_at_domain, ',');
  EOT;
  DT("821_adl");
  return line;
}

static __inline__
char *skip_821_path(char *line, token_delim_t *tok) {
  char *tmp;
  if( !line || (*line != '<') ) { return NULL; }
  BOT;
  line = skip_single_char(line, '<');
  line = skip_822_lwsp(line, 0);
  tmp = skip_821_adl(line, NULL);
  if( tmp ) {
    line = skip_822_lwsp(tmp, 0);
    line = skip_single_char(line, ':');
    line = skip_822_lwsp(line, 0);
  }
  line = skip_821_mailbox(line, NULL);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, '>');
  EOT;
  DT("821_path");
  return line;
}

static __inline__
char *skip_821_reverse_path(char *line, token_delim_t *tok) {
  return skip_821_path(line, tok);
}

static __inline__
char *skip_821_link(char *line, token_delim_t *tok) {
  return skip_822_atom(line, tok);
}

static __inline__
char *skip_821_protocol(char *line, token_delim_t *tok) {
  return skip_822_atom(line, tok);
}

static __inline__
char *skip_821_from_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_single_string(line, "from");
  line = skip_822_lwsp(line, 1);
  line = skip_821_domain(line, tok);
  EOT;
  line = skip_822_lwsp(line, 1);
  DT("821_from_domain");
  return line;
}

static __inline__
char *skip_821_by_domain(char *line, token_delim_t *tok) {
  BOT;
  line = skip_single_string(line, "by");
  line = skip_822_lwsp(line, 1);
  line = skip_821_domain(line, tok);
  EOT;
  line = skip_822_lwsp(line, 1);
  DT("821_by_domain");
  return line;
}

static __inline__
char *skip_821_date(char *line, token_delim_t *tok) {
  BOT;
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 1);
  line = skip_822_month_name(line);
  line = skip_822_lwsp(line, 1);
  line = skip_twodigit(line);
  EOT;
  DT("821_date");
  return line;
}

static __inline__
char *skip_821_time(char *line, token_delim_t *tok) {
  BOT;
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, ':');
  line = skip_822_lwsp(line, 0);
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 0);
  line = skip_single_char(line, ':');
  line = skip_822_lwsp(line, 0);
  line = skip_twodigit(line);
  line = skip_822_lwsp(line, 1);
  line = skip_822_zone_name(line);
  EOT;
  DT("821_time");
  return line;
}

static __inline__
char *skip_821_daytime(char *line, token_delim_t *tok) {
  BOT;
  line = skip_821_date(line, NULL);
  line = skip_822_lwsp(line, 1);
  line = skip_821_time(line, NULL);
  EOT;
  DT("821_daytime");
  return line;
}

/***********************************************************
 * FULL RFC 821 HEADER LINES                               *
 ***********************************************************/
char *parse_821_return_path_line(char *line, parse_822_pth_t *p) {
  memset(p, 0, sizeof(parse_822_pth_t));
  line = skip_single_string(line, "Return-Path:");
  line = skip_822_lwsp(line, 1);
  line = skip_821_reverse_path(line, &p->path_);

  return skip_822_end_of_line(line);
}

char *parse_821_time_stamp_line(char *line, parse_821_rcv_t *p) {
  char *tmp;
  memset(p, 0, sizeof(parse_821_rcv_t));
  line = skip_single_string(line, "Received:");
  line = skip_822_lwsp(line, 1);

  line = skip_821_from_domain(line, &p->from_);
  line = skip_821_by_domain(line, &p->by_);

  tmp = skip_single_string(line, "via");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 1);
    line = skip_821_link(line, &p->via_);
    line = skip_822_lwsp(line, 1);
  }

  tmp = skip_single_string(line, "with");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 1);
    line = skip_821_protocol(line, &p->with_);
    line = skip_822_lwsp(line, 1);
  }

  tmp = skip_single_string(line, "id");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 1);
    line = skip_821_string(line, &p->id_);
    line = skip_822_lwsp(line, 1);
  }

  tmp = skip_single_string(line, "for");
  if( tmp ) {
    line = skip_822_lwsp(tmp, 1);
    line = skip_821_path(line, &p->for_);
    line = skip_822_lwsp(line, 1);
  }

  line = skip_single_char(line, ';');
  line = skip_822_lwsp(line, 1);
  line = skip_821_daytime(line, &p->datetime_);

  return skip_822_end_of_line(line);
}
