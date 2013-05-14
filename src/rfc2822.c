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
 * PARSING FUNCTIONS RFC 2822/2821                         *
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
char *skip_onetwodigit(char *line) {
  if( !line || !isdigit((int)line[0]) ) { return NULL; }
  line++;
  if( isdigit((int)line[0]) ) {
    line++;
  }
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
 * RFC 2822 LEXICAL TOKENS                                 *
 ***********************************************************/

/* fws parsing is slightly wrong: we don't check for CRLF as a unit,
 * instead we accept isspace(). It's possible the OS has mangled the actual CRLF.
 */
static __inline__
char *skip_2822_fws(char *line) {
  if( !line ) { return NULL; }
  while( !isblank((int)*line) ) {
    if( !isspace((int)*line) ) { return NULL; }
    line++;
  }
  line++;
  while( isblank((int)*line) ) { line++; }
  return line;
}

/* the grammar sometimes allows constructs such as token1 token2, where
 * token1 = t1 [CFWS] and token2 = CFWS t2. By the time token1 was parsed,
 * token2 cannot be obtained anymore! We could change the grammar, or we could
 * inspect token pairs critically to prevent this special case. The easiest solution
 * is to allow FWS "undo" and use it just before CFWS. Use this carefully only if you
 * know you can't fall off the beginning of the string.
 */
static __inline__
char *unskip_2822_fws(char *line) {
  while( line && isspace((int)line[-1]) ) {
    line--;
  }
  return line;
}

static __inline__
char *skip_2822_cfws(char *line, int min) {
  char *tmp;
  if( !line ) { return NULL; }
  while(min-- > 0) {
    if( isspace((int)*line ) ) {
      line = skip_2822_fws(line);
    } else if( *line == '(' ) {
      line = skip_recursive(line, '(', ')', '\\');
    } else {
      return NULL;
    }
  }
  /* below space is optional */
  while( line ) {
    if( isspace((int)*line ) ) {
      tmp = skip_2822_fws(line);
      if( tmp ) { 
	line = tmp; 
      } else {
	break;
      }
    } else if( *line == '(' ) {
      line = skip_recursive(line, '(', ')', '\\');
    } else {
      break;
    }
  }
  return line;
}



static __inline__
char *skip_2822_obs_list(char *line, token_delim_t *tok, 
			 char *(*skip)(char *,token_delim_t *, options_t opt), options_t opt) {
  char *tmp;
  BOT;
  tmp = (*skip)(line, NULL, opt);
  if( tmp ) {
    line = tmp;
  }
  line = skip_2822_cfws(line, 0);
  line = skip_single_char(line, ',');
  line = skip_2822_cfws(line, 0);
  do {
    tmp = (*skip)(line, NULL, opt);
    if( tmp ) {
      line = tmp;
    }
    tmp = skip_2822_cfws(line, 0);
    tmp = skip_single_char(tmp, ',');
    tmp = skip_2822_cfws(tmp, 0);
    if( tmp ) {
      line = tmp;
    }
  } while( tmp );
  EOT;
  DT("2822_obs_list");
  return line;
}

char *skip_2822_seq(char *line, token_delim_t *tok, 
		    char *(*skip)(char *,token_delim_t *, options_t), 
		    char delim, options_t opt) {
  char *tmp;
  BOT;
  line = (*skip)(line, NULL, opt);
  tmp = skip_single_char(line, delim);
  while( tmp ) {
    line = (*skip)(tmp, NULL, opt);
    tmp = skip_single_char(line, delim);
  }
  EOT;
  DT("2822_seq");
  return line;
}

static char rfc2822_atom[256] = {
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
char *skip_2822_1atext(char *line, token_delim_t *tok, options_t opt) {
  if( !line || (rfc2822_atom[(unsigned int)*line] != *line) ) { return NULL; }
  BOT;
  while( rfc2822_atom[(unsigned int)*line] == *line ) {
    line++;
  }
  EOT;
  DT("2822_atext");
  return line;
}

static __inline__
char *skip_2822_quoted_string(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_recursive(line, '"', '"', '\\');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_quoted_string");
  return line;
}

static __inline__
char *skip_2822_atom(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_2822_1atext(line, NULL, opt);
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_atom");
  return line;
}

static __inline__
char *skip_2822_word(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  if( !line ) { return NULL; }
  BOT;
  line = skip_2822_quoted_string(line, NULL, opt);
  if( !line ) {
    line = skip_2822_atom(tmp, NULL, opt);
  }
  EOT;
  DT("2822_word");
  return line;
}

static __inline__
char *skip_2822_dot_atom_text(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_seq(line, tok, skip_2822_1atext, '.', opt);
  DT("2822_dot_atom_text");
  return line;
}

static __inline__
char *skip_2822_dot_atom(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_2822_dot_atom_text(line, tok, opt);
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_dot_atom");
  return line;
}

static __inline__
char *skip_2822_domain_literal(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_cfws(line, 0);
  if( !line || (*line != '[') ) { return NULL; }
  BOT;
  line = skip_recursive(line, '[', ']', '\\');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_domain_literal");
  return line;
}

static __inline__
char *skip_2822_obs_domain(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_seq(line, tok, skip_2822_atom, '.', opt);
  DT("2822_obs_domain");
  return line;
}

static __inline__
char *skip_2822_domain(char *line, token_delim_t *tok, options_t opt) {
  if( !line ) { return NULL; }
  BOT;
  if( *line == '[' ) {
    line = skip_2822_domain_literal(line, NULL, opt);
  } else {
    /* we try obs form before normal form, as both could work
       and normal form would be smaller */
    if( opt & (1<<O_2822_OBS) ) {
      line = skip_2822_obs_domain(line, NULL, opt);
    } else {
      line = skip_2822_dot_atom(line, NULL, opt);
    }
  }
  EOT;
  DT("2822_domain");
  return line;
}

static __inline__
char *skip_2822_multicomma(char *line) {
  char *tmp;
  tmp = skip_2822_cfws(line, 0);
  while( tmp ) {
    line = tmp;
    tmp = skip_single_char(line, ',');
    tmp = skip_2822_cfws(tmp, 0);
  }
  return line;
}

static __inline__
char *skip_2822_obs_domain_list(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  opt |= (1<<O_2822_OBS);
  BOT;
  line = skip_single_char(line, '@');
  line = skip_2822_domain(line, NULL, opt);
  do {
    tmp = skip_2822_multicomma(line);
    tmp = skip_single_char(tmp, '@');
    tmp = skip_2822_domain(tmp, NULL, opt);
    if( tmp ) {
      line = tmp;
    }
  } while( tmp );
  EOT;
  DT("2822_obs_domain_list");
  return line;
}

static __inline__
char *skip_2822_obs_route(char *line, token_delim_t *tok, options_t opt) {
  opt |= (1<<O_2822_OBS);
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_2822_obs_domain_list(line, NULL, opt);
  line = skip_single_char(line, ':');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_obs_route");
  return line;
}

static __inline__
char *skip_2822_obs_local_part(char *line, token_delim_t *tok, options_t opt) {
  opt |= (1<<O_2822_OBS);
  line = skip_2822_seq(line, tok, skip_2822_word, '.', opt);
  DT("2822_obs_local_part");
  return line;
}

static __inline__
char *skip_2822_local_part(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  if( !line ) { return NULL; }
  BOT;
  line = skip_2822_quoted_string(line, NULL, opt);
  if( !line ) {
    line = skip_2822_dot_atom(tmp, NULL, opt);
  }
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_obs_local_part(tmp, NULL, opt);
  }
  EOT;
  DT("2822_local_part");
  return line;
}

static __inline__
char *skip_2822_addr_spec(char *line, token_delim_t *tok, options_t opt) {
  BOT;
  line = skip_2822_local_part(line, NULL, opt);
  line = skip_single_char(line, '@');
  line = skip_2822_domain(line, NULL, opt);
  EOT;
  DT("2822_addr_spec");
  return line;
}

static __inline__
char *skip_2822_obs_angle_addr(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  opt |= (1<<O_2822_OBS);
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_single_char(line, '<');
  tmp = skip_2822_obs_route(line, NULL, opt);
  if( tmp ) { 
    line = tmp;
  }
  line = skip_2822_addr_spec(line, NULL, opt);
  line = skip_single_char(line, '>');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_obs_angle_addr");
  return line;
}

static __inline__
char *skip_2822_obs_path(char *line, token_delim_t *tok, options_t opt) {
  opt |= (1<<O_2822_OBS);
  line = skip_2822_obs_angle_addr(line, tok, opt);
  DT("822_obs_angle_addr");
  return line;
}

static __inline__
char *skip_2822_path(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_single_char(line, '<');
  line = skip_2822_cfws(line, 0);
  if( line && (*line != '>') ) {
    line = skip_2822_addr_spec(line, NULL, opt);
  }
  line = skip_single_char(line, '>');
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_obs_path(tmp, NULL, opt);
  }
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_path");
  return line;
}

static __inline__
char *skip_2822_angle_addr(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_single_char(line, '<');
  line = skip_2822_addr_spec(line, NULL, opt);
  line = skip_single_char(line, '>');
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_obs_angle_addr(tmp, NULL, opt);
  }
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_angle_addr");
  return line;
}

/* from the docs, 
 * item-value      =       1*angle-addr / addr-spec /
 *                         atom / domain / msg-id
 * Unfortunately, some of these elements overlap, so we have to be careful
 * how we decide the match. Here's how we deal with this:
 * angle-addr is identical to msg-id, so we never match msg-id.
 * atom is a specialization of domain, so we never match atom.
 * addr-spec is only a partial generalization of domain, so we try to match 
 * addr-spec first, and if that fails we match domain.
 */
static __inline__
char *skip_2822_item_value(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  if( !line ) { return NULL; }
  BOT;
  switch(*line) {
  case '<':
    /* both angle_addr and msg_id turn out to be identical productions, so we
     * don't bother with msg_id */
    line = skip_2822_angle_addr(line, NULL, opt);
    tmp = skip_2822_angle_addr(line, NULL, opt);
    while( tmp ) {
      line = tmp;
      tmp = skip_2822_angle_addr(line, NULL, opt);
    }      
    break;
  case '"':
    line = skip_2822_addr_spec(line, tok, opt);
    break;
  default:
    /* prefer addr-spec over domain, and domain includes atom */
    tmp = skip_2822_addr_spec(line, tok, opt);
    if( tmp ) {
      line = tmp;
    } else {
      line = skip_2822_domain(line, tok, opt);
    }
    break;
  }
  EOT;
  DT("2822_item_value");
  return line;
}

static __inline__
char *skip_2822_item_name(char *line, token_delim_t *tok) {
  if( !line || !isalpha((int)*line) ) { return NULL; }
  BOT;
  while( isalnum((int)*line) || (*line == '-') ) {
    line++;
  }
  EOT;
  DT("2822_item_name");
  return line;
}

static __inline__
char *skip_2822_name_val_pair(char *line, token_delim_t *tok, options_t opt) {
  BOT;
  line = skip_2822_item_name(line, NULL);
  line = skip_2822_cfws(line, 1);
  line = skip_2822_item_value(line, NULL, opt);
  /* some values eat whitespace aggressively, but we don't want it */
  line = unskip_2822_fws(line);
  EOT;
  DT("2822_name_val_pair");
  return line;
}

static __inline__
char *skip_2822_name_val_list(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  line = skip_2822_cfws(line, 0);
  BOT;
  /* the name_val_pair() unskips space at the end, which is useful
   * for parsing, but must be compensated at the end */
  tmp = skip_2822_name_val_pair(line, NULL, opt);
  if( tmp ) {
    line = tmp;
    do {
      tmp = skip_2822_cfws(line, 1);
      tmp = skip_2822_name_val_pair(tmp, NULL, opt);
      if( tmp ) {
	line = tmp;
      }
    } while( tmp );
  }
  EOT;
  DT("2822_name_val_list");
  /* name_val_list may have unskipped the space at the end, so we skip forward again */
  line = skip_2822_cfws(line,0);
  return line;
}

static char *day_name[] = { 
  "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

static __inline__
char *skip_2822_day_of_week(char *line, options_t opt) {
  if( !line ) { return NULL; }
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  } else if( isblank((int)*line) ) {
    line = skip_2822_fws(line);
  }
  line = skip_string_list(line, day_name, sizeof(day_name)/sizeof(char*));
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  return line;
}

static __inline__
char *skip_2822_day(char *line, options_t opt) {
  if( !line ) { return NULL; }
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  } else if( isblank((int)*line) ) {
    line = skip_2822_fws(line);
  }
  line = skip_onetwodigit(line);
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  return line;
}

static char *month_name[] = { 
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
  "Aug", "Sep", "Oct", "Nov", "Dec"
};

static __inline__
char *skip_2822_month(char *line, options_t opt) {
  if( (opt & (1<<O_2822_OBS)) ) {
    line = unskip_2822_fws(line); /* may have been eaten by day */
    line = skip_2822_cfws(line, 1);
  } else {
    line = skip_2822_fws(line);
  }
  line = skip_string_list(line, month_name, sizeof(month_name)/sizeof(char*));
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 1);
  } else {
    line = skip_2822_fws(line);
  }
  return line;
}

static __inline__
char *skip_2822_year(char *line, options_t opt) {
  char *tmp = line;
  line = skip_fourdigit(line);
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_cfws(tmp, 0);
    line = skip_twodigit(line);
    line = skip_2822_cfws(line, 0);
  }
  return line;
}

static __inline__
char *skip_2822_date(char *line, options_t opt) {
  line = skip_2822_day(line, opt);
  line = skip_2822_month(line, opt);
  line = skip_2822_year(line, opt);
  return line;
}

static __inline__
char *skip_2822_hour(char *line, options_t opt) {
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  line = skip_twodigit(line);
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  return line;
}

static __inline__
char *skip_2822_minute(char *line, options_t opt) {
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  line = skip_twodigit(line);
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  return line;
}

static __inline__
char *skip_2822_second(char *line, options_t opt) {
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  line = skip_twodigit(line);
  /* note: obsolete format allows [cfws] after 2DIGIT, but we don't */ 
  return line;
}


static __inline__
char *skip_2822_time_of_day(char *line, options_t opt) {
  char *tmp;
  line = skip_2822_hour(line, opt);
  line = skip_single_char(line, ':');
  line = skip_2822_minute(line, opt);
  tmp = skip_single_char(line, ':');
  if( tmp ) {
    line = skip_2822_second(tmp, opt);
  }
  return line;
}

static char *zone_name[] = { 
  "UT", "GMT", "EST", "EDT", "CST", "CDT", "MST",
  "MDT", "PST", "PDT"
};

static __inline__
char *skip_2822_obs_zone_name(char *line) {
  return skip_string_list(line, zone_name, sizeof(zone_name)/sizeof(char*));
}

static __inline__
char *skip_2822_obs_zone(char *line) {
  if( !line ) { return NULL; }
  if( isalpha((int)line[0]) ) {
    if( !isalpha((int)line[1]) ) {
      line++;
    } else {
      line = skip_2822_obs_zone_name(line);
    }
  }
  return line;
}

static __inline__
char *skip_2822_zone(char *line, options_t opt) {
  char *tmp = line;
  if( !line ) { return NULL; }
  if( (*line == '+') || (*line == '-') ) {
    line = skip_fourdigit(line + 1);
  } else if( opt & (1<<O_2822_OBS) ) {
    line = skip_2822_obs_zone(tmp);
  }
  return line;
}

static __inline__
char *skip_2822_time(char *line, options_t opt) {
  line = skip_2822_time_of_day(line, opt);
  if( opt & (1<<O_2822_OBS) ) {
    line = unskip_2822_fws(line); /* space may have been eaten */
  }
  line = skip_2822_fws(line);
  line = skip_2822_zone(line, opt);
  return line;
}


static __inline__
char *skip_2822_date_time(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  BOT;
  tmp = skip_2822_day_of_week(line, opt);
  if( tmp ) {
    line = skip_single_char(tmp, ',');
  }
  line = skip_2822_date(line, opt);
  if( opt & (1<<O_2822_OBS) ) {
    line = unskip_2822_fws(line);
  }
  line = skip_2822_fws(line);
  line = skip_2822_time(line, opt);  
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_date_time");
  return line;
}

static __inline__
char *skip_2822_obs_phrase(char *line, options_t opt) {
  char *tmp;
  opt |= (1<<O_2822_OBS);
  line = skip_2822_word(line, NULL, opt);
  if( !line ) { return NULL; }
  if( *line == '.') {
    tmp = line + 1;
  } else if( isspace((int)*line) ) {
    tmp = skip_2822_cfws(line, 1);
  } else {
    tmp = skip_2822_word(line, NULL, opt);
  }
  while( tmp ) {
    line = tmp;
    if( *line == '.') {
      tmp = line + 1;
    } else if( isspace((int)*line) ) {
      tmp = skip_2822_cfws(line, 1);
    } else {
      tmp = skip_2822_word(line, NULL, opt);
    }
  }
  return line;
}

static __inline__
char *skip_2822_phrase(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  BOT;
  /* obsolete can contain period, so we must try it first,
     otherwise we might match (successfully) only part of the 
     phrase (without period), and fail to match future tokens correctly */
  if( opt & (1<<O_2822_OBS) ) {
    line = skip_2822_obs_phrase(line, opt);
  } else {
    line = skip_2822_word(line, NULL, opt);
    tmp = skip_2822_word(line, NULL, opt);
    while( tmp ) {
      line = tmp;
      tmp = skip_2822_word(line, NULL, opt);
    }
  }
  EOT;
  DT("2822_phrase");
  return line;
}

static __inline__
char *skip_2822_display_name(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_phrase(line, tok, opt);
  return line;
}

static __inline__
char *skip_2822_name_addr(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  BOT;
  tmp = skip_2822_display_name(line, NULL, opt);
  if( tmp ) {
    line = tmp;
  }
  line = skip_2822_angle_addr(line, NULL, opt);
  EOT;
  DT("2822_name_addr");
  return line;
}

static __inline__
char *skip_2822_mailbox(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  BOT;
  line = skip_2822_addr_spec(line, NULL, opt);
  if( !line ) {
    line = skip_2822_name_addr(tmp, NULL, opt);
  }
  EOT;
  DT("2822_mailbox");
  return line;
}

static __inline__
char *skip_2822_mailbox_list(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  line = skip_2822_seq(line, tok, skip_2822_mailbox, ',', opt);
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_obs_list(tmp, tok, skip_2822_mailbox, opt);
  }
  return line;
}

static __inline__
char *skip_2822_group(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  BOT;
  line = skip_2822_display_name(line, NULL, opt);
  line = skip_single_char(line, ':');
  tmp = skip_2822_mailbox_list(line, NULL, opt);
  if( tmp ) {
    line = tmp;
  } else {
    tmp = skip_2822_cfws(line, 1);
    if( tmp ) {
      line = tmp;
    }
  }
  line = skip_single_char(line, ';');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_group");
  return line;
}

static __inline__
char *skip_2822_address(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  BOT;
  line = skip_2822_mailbox(line, NULL, opt);
  if( !line ) {
    line = skip_2822_group(tmp, NULL, opt);
  }
  EOT;
  DT("2822_address");
  return line;
}

static __inline__
char *skip_2822_address_list(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  line = skip_2822_seq(line, tok, skip_2822_address, ',', opt);
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_obs_list(tmp, tok, skip_2822_address, opt);
  }
  return line;
}

static __inline__
char *skip_2822_id_left(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  if( !line ) { return NULL; }
  if( *line == '"' ) {
    line = skip_recursive(line, '"', '"', '\\');
  } else {
    line = skip_2822_dot_atom_text(line, NULL, opt);
  }
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_local_part(tmp, NULL, opt);
  }
  return line;
}

static __inline__
char *skip_2822_id_right(char *line, token_delim_t *tok, options_t opt) {
  char *tmp = line;
  if( !line ) { return NULL; }
  if( *line == '[' ) {
    line = skip_recursive(line, '[', ']', '\\');
  } else {
    line = skip_2822_dot_atom_text(line, NULL, opt);
  }
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_domain(tmp, NULL, opt);
  }
  return line;
}

static __inline__
char *skip_2822_msg_id(char *line, token_delim_t *tok, options_t opt) {
  line = skip_2822_cfws(line, 0);
  BOT;
  line = skip_single_char(line, '<');
  line = skip_2822_id_left(line, NULL, opt);
  line = skip_single_char(line, '@');
  line = skip_2822_id_right(line, NULL, opt);
  line = skip_single_char(line, '>');
  EOT;
  line = skip_2822_cfws(line, 0);
  DT("2822_msg_id");
  return line;
}

static __inline__
char *skip_2822_refs(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  BOT;
  line = skip_2822_msg_id(line, NULL, opt);
  tmp =  skip_2822_msg_id(line, NULL, opt);
  while( tmp ) {
    line = tmp;
    tmp =  skip_2822_msg_id(line, NULL, opt);
  }
  EOT;
  DT("2822_refs");
  return line;
}

static __inline__
char *skip_2822_text(char *line, token_delim_t *tok) {
  if( !line ) { return NULL; }
  BOT;
  while( *line ) { line++; }
  EOT;
  DT("2822_text");
  return line;
}

static __inline__
char *skip_2822_field_name(char *line, char *name, options_t opt) {
  line = skip_single_string(line, name);
  if( (opt & (1<<O_2822_OBS)) ) {
    line = skip_2822_cfws(line, 0);
  }
  line = skip_single_char(line, ':');
  return line;
}

static __inline__
char *skip_2822_end_of_line(char *line) {
  /* check for CRLF, but OS may have mangled/removed it */
  if( !line || (strlen(line) > 2) ) { return NULL; }
  while( isspace((int)*line) ) { line++; }
  return (*line) ? NULL : line;
}

/***********************************************************
 * FULL RFC 822 HEADER LINES                               *
 ***********************************************************/

char *parse_2822_return(char *line, parse_2822_pth_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_pth_t));
  line = skip_2822_field_name(line, "Return-Path", opt);
  line = skip_2822_path(line, &p->path_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_received(char *line, parse_2822_rcv_t *p, options_t opt) {
  char *tmp;
  memset(p, 0, sizeof(parse_2822_rcv_t));

  line = skip_2822_field_name(line, "Received", opt);
  tmp = line;
  line = skip_2822_name_val_list(line, &p->naval_, opt);
  line = skip_single_char(line, ';');
  line = skip_2822_date_time(line, &p->datetime_, opt);
  if( (opt & (1<<O_2822_OBS)) && !line ) {
    line = skip_2822_name_val_list(tmp, &p->naval_, opt);
  }
  return skip_2822_end_of_line(line);
}

char *parse_2822_from(char *line, parse_2822_mls_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_mls_t));

  line = skip_2822_field_name(line, "From", opt);
  line = skip_2822_mailbox_list(line, &p->mailboxl_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_from(char *line, parse_2822_mls_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_from(line, p, opt);
}

char *parse_2822_sender(char *line, parse_2822_mbx_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_mbx_t));

  line = skip_2822_field_name(line, "Sender", opt);
  line = skip_2822_mailbox(line, &p->mailbox_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_sender(char *line, parse_2822_mbx_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_sender(line, p, opt);
}

char *parse_2822_reply_to(char *line, parse_2822_als_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_als_t));

  line = skip_2822_field_name(line, "Reply-To", opt);
  line = skip_2822_address_list(line, &p->addressl_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_reply_to(char *line, parse_2822_als_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_reply_to(line, p, opt);
}

char *parse_2822_to(char *line, parse_2822_als_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_als_t));

  line = skip_2822_field_name(line, "To", opt);
  line = skip_2822_address_list(line, &p->addressl_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_to(char *line, parse_2822_als_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_to(line, p, opt);
}

char *parse_2822_cc(char *line, parse_2822_als_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_als_t));

  line = skip_2822_field_name(line, "Cc", opt);
  line = skip_2822_address_list(line, &p->addressl_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_cc(char *line, parse_2822_als_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_cc(line, p, opt);
}

char *parse_2822_bcc(char *line, parse_2822_als_t *p, options_t opt) {
  char *tmp;
  memset(p, 0, sizeof(parse_2822_als_t));

  line = skip_2822_field_name(line, "Bcc", opt);
  tmp = skip_2822_address_list(line, &p->addressl_, opt);
  if( tmp ) {
    line = tmp;
  } else {
    line = skip_2822_cfws(line, 0);
  }

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_bcc(char *line, parse_2822_als_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_bcc(line, p, opt);
}

char *parse_2822_message_id(char *line, parse_2822_mid_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_mid_t));

  line = skip_2822_field_name(line, "Message-ID", opt);
  line = skip_2822_msg_id(line, &p->msg_id_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_message_id(char *line, parse_2822_mid_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_message_id(line, p, opt);
}

char *parse_2822_date(char *line, parse_2822_dat_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_dat_t));

  line = skip_2822_field_name(line, "Date", opt);
  line = skip_2822_date_time(line, &p->datetime_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_resent_date(char *line, parse_2822_dat_t *p, options_t opt) {
  line = skip_single_string(line, "Resent-");
  return parse_2822_date(line, p, opt);
}

char *parse_2822_in_reply_to(char *line, parse_2822_ref_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_ref_t));

  line = skip_2822_field_name(line, "In-Reply-To", opt);
  line = skip_2822_refs(line, &p->refs_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_references(char *line, parse_2822_ref_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_ref_t));

  line = skip_2822_field_name(line, "References", opt);
  line = skip_2822_refs(line, &p->refs_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2822_subject(char *line, parse_2822_txt_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_txt_t));

  line = skip_2822_field_name(line, "Subject", opt);
  line = skip_2822_text(line, &p->text_);

  return skip_2822_end_of_line(line);
}


/***********************************************************
 * RFC 2821 LEXICAL TOKENS                                 *
 ***********************************************************/


static __inline__
char *skip_2821_sub_domain(char *line, token_delim_t *tok, options_t opt) {
  if( !line || !isalnum((int)*line) ) { return NULL; }
  BOT;
  line++;
  while( isalnum((int)*line) || (*line == '-') ) {
    line++;
  }
  EOT;
  if( line[-1] == '-' ) { return NULL; }
  DT("2821_sub_domain");
  return line;
}

/* this is not correct, but we won't quibble */
static __inline__
char *skip_2821_address_literal(char *line) {
  return skip_recursive(line, '[', ']', '\\');
}

static __inline__
char *skip_2821_domain(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  if( !line ) { return NULL; }
  BOT;
  if( *line == '[' ) {
    line = skip_2821_address_literal(line);
  } else {
    line = skip_2821_sub_domain(line, NULL, opt);
    tmp = skip_single_char(line, '.');
    tmp = skip_2822_seq(tmp, NULL, skip_2821_sub_domain, '.', opt);
    if( tmp ) {
      line = tmp;
    } else if( !(opt & (1<<O_2822_ALLOW_NFQDN)) ) {
      line = NULL;
    }
  }
  EOT;
  DT("821_domain");
  return line;
}

static __inline__
char *skip_2821_at_domain(char *line, token_delim_t *tok, options_t opt) {
  BOT;
  line = skip_single_char(line, '@');
  line = skip_2821_domain(line, NULL, opt);
  EOT;
  DT("2821_at_domain");
  return line;
}

static __inline__
char *skip_2821_local_part(char *line, options_t opt) {
  if( !line ) { return NULL; }
  if( *line == '"' ) {
    return skip_recursive(line, '"', '"', '\\');
  }
  return skip_2822_dot_atom_text(line, NULL, opt);
}

static __inline__
char *skip_2821_mailbox(char *line, token_delim_t *tok, options_t opt) {
  BOT;
  line = skip_2821_local_part(line, opt);
  line = skip_single_char(line, '@');
  line = skip_2821_domain(line, NULL, opt);
  EOT;
  DT("2821_mailbox");
  return line;
}

static __inline__
char *skip_2821_adl(char *line, token_delim_t *tok, options_t opt) {
  BOT;
  line = skip_2822_seq(line, tok, skip_2821_at_domain, ',', opt);
  EOT;
  DT("2821_adl");
  return line;
}

static __inline__
char *skip_2821_path(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  if( !line || (*line != '<') ) { return NULL; }
  BOT;
  line = skip_single_char(line, '<');
  tmp = skip_2821_adl(line, NULL, opt);
  if( tmp ) {
    line = skip_single_char(line, ':');
  }
  line = skip_2821_mailbox(line, NULL, opt);
  line = skip_single_char(line, '>');
  EOT;
  DT("2821_path");
  return line;
}


static __inline__
char *skip_2821_reverse_path(char *line, token_delim_t *tok, options_t opt) {
  return skip_2821_path(line, tok, opt);
}

static __inline__
char *skip_2821_tcp_info(char *line, options_t opt) {
  if( !line ) { return NULL; }
  if( *line == '[' ) {
    return skip_2821_address_literal(line);
  }
  line = skip_2821_domain(line, NULL, opt);
  line = skip_2822_fws(line);
  return skip_2821_address_literal(line);
}

static __inline__
char *skip_2821_extended_domain(char *line, token_delim_t *tok, options_t opt) {
  char *tmp;
  if( !line ) { return NULL; }
  BOT;
  if( *line == '[' ) {
    line = skip_2821_address_literal(line);
    line = skip_2822_fws(line);
    line = skip_single_char(line, '(');
    line = skip_2821_tcp_info(line, opt);
    line = skip_single_char(line, ')');
  } else {
    line = skip_2821_domain(line, NULL, opt);
    tmp = skip_2822_fws(line);
    tmp = skip_single_char(tmp, '(');
    tmp = skip_2821_tcp_info(tmp, opt);
    tmp = skip_single_char(tmp, ')');
    if( tmp ) {
      line = tmp;
    } else if( opt & (1<<O_2821_NO_PARENS_ADDRESS) ) {
      tmp = skip_2822_fws(line);
      tmp = skip_2821_address_literal(tmp);
      if( tmp ) {
	line = tmp;
      }
    }
  }
  EOT;
  DT("2821_extended_domain");
  return line;
}

static __inline__
char *skip_2821_from_domain(char *line, token_delim_t *tok, options_t opt) {
  line = skip_single_string(line, "from");
  line = skip_2822_fws(line);
  line = skip_2821_extended_domain(line, tok, opt);
  line = skip_2822_cfws(line, 1);
  DT("2821_from_domain");
  return line;
}

static __inline__
char *skip_2821_by_domain(char *line, token_delim_t *tok, options_t opt) {
  line = skip_single_string(line, "by");
  line = skip_2822_fws(line);
  line = skip_2821_extended_domain(line, tok, opt);
  line = skip_2822_cfws(line, 1);
  DT("2821_by_domain");
  return line;
}

static __inline__
char *skip_2821_link(char *line, token_delim_t *tok, options_t opt) {
  return skip_2822_atom(line, tok, opt);
}

static __inline__
char *skip_2821_protocol(char *line, token_delim_t *tok, options_t opt) {
  return skip_2822_atom(line, tok, opt);
}

static __inline__
char *skip_2821_string(char *line, token_delim_t *tok, options_t opt) {
  if( !line ) { return NULL; }
  BOT;
  if( *line == '"' ) {
    line = skip_recursive(line, '"', '"', '\\');
  } else {
    line = skip_2822_atom(line, NULL, opt);
  }
  EOT;
  DT("2821_string");
  return line;
}

static __inline__
char *skip_2821_string_msg_id(char *line, token_delim_t *tok, options_t opt) {
  if( !line ) { return NULL; }
  if( *line == '<' ) {
    /* should we allow obsolete msg-id forms? */
    line = skip_2822_msg_id(line, tok, opt);
  } else {
    line = skip_2821_string(line, tok, opt);
  }
  return line;
}

static __inline__
char *skip_2821_path_mailbox(char *line, token_delim_t *tok, options_t opt) {
  if( !line ) { return NULL; }
  if( *line == '<' ) {
    line = skip_2821_path(line, tok, opt);
  } else {
    line = skip_2821_mailbox(line, tok, opt);
  }
  return line;
}

/***********************************************************
 * FULL RFC 2821 HEADER LINES                              *
 ***********************************************************/
char *parse_2821_return_path_line(char *line, parse_2822_pth_t *p, options_t opt) {
  memset(p, 0, sizeof(parse_2822_pth_t));
  line = skip_single_string(line, "Return-Path:");
  line = skip_2822_fws(line);
  line = skip_2821_reverse_path(line, &p->path_, opt);

  return skip_2822_end_of_line(line);
}

char *parse_2821_time_stamp_line(char *line, parse_2821_rcv_t *p, options_t opt) {
  char *tmp;
  memset(p, 0, sizeof(parse_2821_rcv_t));
  line = skip_single_string(line, "Received:");
  line = skip_2822_fws(line);

  line = skip_2821_from_domain(line, &p->from_, opt);
  line = skip_2821_by_domain(line, &p->by_, opt);

  tmp = skip_single_string(line, "via");
  if( tmp ) {
    line = skip_2822_fws(tmp);
    line = skip_2821_link(line, &p->via_, opt);
    tmp = unskip_2822_fws(line); /* link eats trailing space */
    tmp = skip_2822_cfws(line, 1);
    if( tmp ) {
      line = tmp;
    } else if( !(opt & (1<<O_2821_NO_SP_BEFORE_DATE)) ) {
      line = NULL;
    }
  }

  tmp = skip_single_string(line, "with");
  if( tmp ) {
    line = skip_2822_fws(tmp);
    line = skip_2821_protocol(line, &p->with_, opt);
    tmp = unskip_2822_fws(line); /* protocol eats trailing space */
    tmp = skip_2822_cfws(line, 1);
    if( tmp ) {
      line = tmp;
    } else if( !(opt & (1<<O_2821_NO_SP_BEFORE_DATE)) ) {
      line = NULL;
    }
  }

  tmp = skip_single_string(line, "id");
  if( tmp ) {
    line = skip_2822_fws(tmp);
    line = skip_2821_string_msg_id(line, &p->smid_, opt);
    tmp = unskip_2822_fws(line); /* id eats trailing space */
    tmp = skip_2822_cfws(line, 1);
    if( tmp ) {
      line = tmp;
    } else if( !(opt & (1<<O_2821_NO_SP_BEFORE_DATE)) ) {
      line = NULL;
    }
  }

  tmp = skip_single_string(line, "for");
  if( tmp ) {
    line = skip_2822_fws(tmp);
    line = skip_2821_path_mailbox(line, &p->for_, opt);
    tmp = unskip_2822_fws(line); /* previous eats trailing space */
    tmp = skip_2822_cfws(line, 1);
    if( tmp ) {
      line = tmp;
    } else if( !(opt & (1<<O_2821_NO_SP_BEFORE_DATE)) ) {
      line = NULL;
    }
  }

  line = skip_single_char(line, ';');
  line = skip_2822_fws(line);
  line = skip_2822_date_time(line, &p->datetime_, opt);

  return skip_2822_end_of_line(line);
}
