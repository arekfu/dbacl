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


extern options_t u_options;
extern long system_pagesize;


/* #define DEBUG_TOKEN(s,d) print_token_delim(stdout, (s), (d)) */
#define DEBUG_TOKEN(s,d)


/***********************************************************
 * PARSING FUNCTIONS                                       *
 *                                                         *
 * The RFC 821,822,2821,2822 standards each define grammars*
 * which are subtly incompatible, most likely due to typos.*
 * For us, this poses a problem. The easiest solution is   *
 * to define four different parsing functions, and try them*
 * all out.                                                *
 ***********************************************************
 * The skip functions operate as follows: if line is NULL, *
 * the function returns NULL. Otherwise, line is returned  *
 * as a pointer to the first character after the skipped   *
 * pattern. If the patterns couldn't be traversed          *
 * successfully, the function returns NULL.                *
 ***********************************************************/

void print_token_delim(FILE *out, char *sym, token_delim_t *d) {
  char *p;
  if( sym && d) { 
    fprintf(out, "%s[", sym); 
    for(p = d->begin; p < d->end; p++) {
      fprintf(out, "%c", *p);
    }
    fprintf(out, "]\n");
  }
}

void print_state(FILE *out, hline_t *t) {
  fprintf(stdout, "%s%s%s%s%s%s%s%s\n",
	  (t->state & (1<<H_STATE_RFC821)) ? " 821" : "",
	  (t->state & (1<<H_STATE_RFC822)) ? " 822" : "",
	  (t->state & (1<<H_STATE_RFC2821)) ? " 2821" : "",
	  (t->state & (1<<H_STATE_RFC2821loc)) ? " 2821loc" : "",
	  (t->state & (1<<H_STATE_RFC2822)) ? " 2822" : "",
	  (t->state & (1<<H_STATE_RFC2822lax)) ? " 2822lax" : "",
	  (t->state & (1<<H_STATE_RFC2821lax)) ? " 2821lax" : "",
	  (t->state & (1<<H_STATE_RFC2822obs)) ? " 2822obs" : "");
}

/***********************************************************
 * MISC FUNCTIONS                                          *
 ***********************************************************/
void init_head_filter(HEADER_State *head) {
  memset(head, 0, sizeof(HEADER_State));
  head->hdata.textbuf_len = system_pagesize;
  head->hdata.textbuf = (char *)malloc(head->hdata.textbuf_len);
  head->hdata.textbuf_end = head->hdata.textbuf;
  head->hdata.curline = 0;

  head->hstack.max = 100;
  head->hstack.top = 0;
  head->hstack.hlines = (hline_t *)calloc(head->hstack.max, sizeof(hline_t));
}

void free_head_filter(HEADER_State *head) {
  if( head->hdata.textbuf ) { 
    free(head->hdata.textbuf);
    head->hdata.textbuf = NULL;
    head->hdata.textbuf_end = NULL;
    head->hdata.curline = 0;
  }
  if( head->hstack.hlines ) {
    free(head->hstack.hlines);
    head->hstack.top = 0;
  }
}

char *head_append_hline(HEADER_State *head, const char *line) {
  int l, n;
  char *tmp;
  if( head->hdata.textbuf && line ) {
    l = head->hdata.textbuf_end - head->hdata.textbuf;
    n = strlen(line);
    while( (l+n) >= head->hdata.textbuf_len ) {
      tmp = (char *)realloc(head->hdata.textbuf, 2 * head->hdata.textbuf_len);
      if( !tmp ) {
	errormsg(E_ERROR,
		 "not enough memory for input line (%d bytes)\n",
		 head->hdata.textbuf_len);
	return NULL;
      }
      head->hdata.textbuf = tmp;
      head->hdata.textbuf_end = tmp + l;
      head->hdata.textbuf_len *= 2;
    }

    if( !isblank(*line) ) {
      /* separate logical header lines by NUL chars */
      *head->hdata.textbuf_end++ = '\0';
      head->hdata.curline = head->hdata.textbuf_end - head->hdata.textbuf;
    }
    strcpy(head->hdata.textbuf_end, line); 
    head->hdata.textbuf_end += n;
    return head->hdata.textbuf + head->hdata.curline;
  }
  return NULL;
}

/***********************************************************
 * CONVERSION FUNCTIONS                                    *
 ***********************************************************/
int adjust_tz(struct tm *tim, char *p) {
  int s, a, i;

  typedef struct { char *nam; int offs; } zon_t;
  static zon_t zones[] = { 
    /* note: zone offsets are already added to printed date */
    {"UT", 0}, {"GMT", 0},
    {"EST", -5}, {"EDT", -4},
    {"CST", -6}, {"CDT", -5},
    {"MST", -7}, {"MDT", -6},
    {"PST", -8}, {"PDT", -7},
    {"A", -1}, {"B", -2}, {"C", -3}, {"D", -4},
    {"E", -5}, {"F", -6}, {"G", -7}, {"H", -8},
    {"I", -9}, {"K", -10}, {"L", -11}, {"M", -12},
    {"N", +1}, {"O", +2}, {"P", +3}, {"Q", +4},
    {"R", +5}, {"S", +6}, {"T", +7}, {"U", +8},
    {"V", +9}, {"W", +10}, {"X", +11}, {"Y", +12},
    {"Z", 0}
  };

  while( *p && !isalpha((int)*p) && (*p != '+') && (*p != '-') ) { p++; }
  switch(*p) {
  case '+':
  case '-':
    if( *p == '+' ) { s = -1; } else { s = +1; }
    while( *p && !isdigit((int)*p) ) { p++; }
    a = atoi(p);
    tim->tm_hour += s * (int)(a / 100);
    tim->tm_min += s * (int)(a % 100);
    break;
  default:
    a = -25;
    for(i = 0; i < sizeof(zones)/sizeof(zon_t); i++) {
      if( strncasecmp(p, zones[i].nam, strlen(zones[i].nam)) == 0 ) {
	a = -zones[i].offs;
	break;
      }
    }
    if( a == -25 ) {
      return -1;
    } else {
      tim->tm_hour += a;
    }
  }
  return 0;
}

/* not sure that we can use strptime(), so we do this manually :-( */
time_t cvt_date(token_delim_t *tok) {
  struct tm tim;
  time_t retval;
  char *p, *q;
  int i;

  static char *month_name[] = { 
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
    "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  static char *day_name[] = { 
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };

  p = tok->begin;
  if( !p ) { return -1; }

  while( *p && !isdigit((int)*p) ) { p++; }
  tim.tm_mday = atoi(p);
  if( (tim.tm_mday < 1) || (tim.tm_mday > 31) ) { return -1; }
  while( *p && !isalpha((int)*p) ) { p++; }
  tim.tm_mon = -1;
  for(i = 0; i < sizeof(month_name)/sizeof(char*); i++) {
    if( strncasecmp(p, month_name[i], 3) == 0 ) {
      tim.tm_mon = i;
      break;
    }
  }
  if( (tim.tm_mon < 0) || (tim.tm_mon > 11) ) { return -1; }
  while( *p && !isdigit((int)*p) ) { p++; }
  tim.tm_year = atoi(p) - 1900;
  if( (tim.tm_year < 0) || (tim.tm_year > 132) ) { return -1; }
  while( *p && !isspace((int)*p) ) { p++; }
  while( *p && !isdigit((int)*p) ) { p++; }
  tim.tm_hour = atoi(p);
  if( (tim.tm_hour < 0) || (tim.tm_hour > 23) ) { return -1; }
  while( *p && (*p != ':') ) { p++; }
  while( *p && !isdigit((int)*p) ) { p++; }
  tim.tm_min = atoi(p);
  if( (tim.tm_min < 0) || (tim.tm_min > 59) ) { return -1; }
  for(q = p; *q && (*q != ':'); q++);
  if( *q == ':' ) {
    for(p = q; *p && !isdigit((int)*p); p++);
    tim.tm_sec = atoi(p);
    if( (tim.tm_sec < 0) || (tim.tm_sec > 59) ) { return -1; }
  } else {
    tim.tm_sec = 0;
  }
  while( *p && isdigit((int)*p) ) { p++; }
  tim.tm_isdst = 0;
  /* verify weekday if present - this must be done before the time zone
   * adjustment!
   */
  for(q = tok->begin; *q && !isalnum((int)*q); q++); /* used later */
  if( isalpha((int)*q) ) {
    for(i = 0; i < sizeof(day_name)/sizeof(char*); i++) {
      if( strncasecmp(q, day_name[i], 3) == 0 ) {
	if( tim.tm_wday != i ) {
	  retval = -1;
	}
	break;
      }
    }
  }
  /* time zone parsing is messy */
  if( adjust_tz(&tim, p) > -1 ) {
    /* mktime assumes UTC */
    retval = mktime(&tim);
  } else {
    retval = -1;
  }

  return retval;
}

/***********************************************************
 * HEADER LINE FUNCTIONS                                   *
 ***********************************************************/
hline_t *mkh_ignore(HEADER_State *head, hline_t *t, char *line) {
  memset(t, 0, sizeof(hline_t));
  t->tag = hltIGN;
  t->data.ign.line = line;
  t->toffset = head->hdata.curline;

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "IGNORED\n");
  }

  return t;
}

hline_t *mkh_return_path(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRET;
  t->toffset = head->hdata.curline;

  if( parse_822_return(line, &t->data.ret.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_821_return_path_line(line, &t->data.ret.rfc821) ) {
    t->state |= (1<<H_STATE_RFC821);
  }

  if( parse_2822_return(line, &t->data.ret.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_return(line, &t->data.ret.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_return(line, &t->data.ret.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( parse_2821_return_path_line(line, &t->data.ret.rfc2821, RFC_2821_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2821);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RETURN-PATH");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_received(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRCV;
  t->toffset = head->hdata.curline;

  if( parse_822_received(line, &t->data.rcv.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_821_time_stamp_line(line, &t->data.rcv.rfc821) ) {
    t->state |= (1<<H_STATE_RFC821);
  }

  if( parse_2821_time_stamp_line(line, &t->data.rcv.rfc2821, RFC_2821_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2821);
  } else if( parse_2821_time_stamp_line(line, &t->data.rcv.rfc2821, RFC_2821_LAX) ) {
    t->state |= (1<<H_STATE_RFC2821lax);
  }

  if( parse_2822_received(line, &t->data.rcv.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_received(line, &t->data.rcv.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_received(line, &t->data.rcv.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }


  if( t->state & ((1<<H_STATE_RFC2821)|(1<<H_STATE_RFC2821lax)) ) {
    t->data.rcv.numsec = cvt_date(&t->data.rcv.rfc2821.datetime_);
  } else if( t->state & ((1<<H_STATE_RFC2822)|(1<<H_STATE_RFC2822obs)|(1<<H_STATE_RFC2822lax)) ) {
    t->data.rcv.numsec = cvt_date(&t->data.rcv.rfc2822.datetime_);
  } else if( t->state & (1<<H_STATE_RFC822) ) {
    t->data.rcv.numsec = cvt_date(&t->data.rcv.rfc822.datetime_);
  } else {
    t->data.rcv.numsec = -1;
  }

  if( t->data.dat.numsec == (time_t)(-1) ) {
    t->state |= (1<<H_STATE_BAD_DATA);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RECEIVED %ld", t->data.rcv.numsec);
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_reply_to(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRPT;
  t->toffset = head->hdata.curline;

  if( parse_822_reply_to(line, &t->data.rpt.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_reply_to(line, &t->data.rpt.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_reply_to(line, &t->data.rpt.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_reply_to(line, &t->data.rpt.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "REPLY-TO");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_from(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltFRM;
  t->toffset = head->hdata.curline;

  if( parse_822_from(line, &t->data.frm.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_from(line, &t->data.frm.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_from(line, &t->data.frm.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_from(line, &t->data.frm.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "FROM");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_sender(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltSND;
  t->toffset = head->hdata.curline;

  if( parse_822_sender(line, &t->data.snd.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_sender(line, &t->data.snd.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_sender(line, &t->data.snd.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_sender(line, &t->data.snd.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  } 

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "SENDER");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_reply_to(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRRT;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_reply_to(line, &t->data.rrt.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_reply_to(line, &t->data.rrt.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_reply_to(line, &t->data.rrt.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_reply_to(line, &t->data.rrt.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-REPLY-TO");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_from(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRFR;
  t->toffset = head->hdata.curline;

  if( parse_822_from(line, &t->data.rfr.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_from(line, &t->data.rfr.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_from(line, &t->data.rfr.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_from(line, &t->data.rfr.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-FROM");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_sender(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRSN;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_sender(line, &t->data.rsn.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_sender(line, &t->data.rsn.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_sender(line, &t->data.rsn.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_sender(line, &t->data.rsn.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT_SENDER");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_date(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltDAT;
  t->toffset = head->hdata.curline;

  if( parse_822_date(line, &t->data.dat.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_date(line, &t->data.dat.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_date(line, &t->data.dat.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_date(line, &t->data.dat.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( t->state & ((1<<H_STATE_RFC2822)|(1<<H_STATE_RFC2822obs)|(1<<H_STATE_RFC2822lax)) ) {
    t->data.dat.numsec = cvt_date(&t->data.dat.rfc2822.datetime_);
  } else if( t->state & (1<<H_STATE_RFC822) ) {
    t->data.dat.numsec = cvt_date(&t->data.dat.rfc822.datetime_);
  } else {
    t->data.dat.numsec = -1;
  }

  if( t->data.dat.numsec == (time_t)(-1) ) {
    t->state |= (1<<H_STATE_BAD_DATA);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "DATE %ld", t->data.dat.numsec);
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_date(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRDT;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_date(line, &t->data.rdt.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_date(line, &t->data.rdt.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_date(line, &t->data.rdt.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_date(line, &t->data.rdt.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-DATE");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_to(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltTO;
  t->toffset = head->hdata.curline;

  if( parse_822_to(line, &t->data.to.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_to(line, &t->data.to.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822); 
  } else if( parse_2822_to(line, &t->data.to.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs); 
  } else if( parse_2822_to(line, &t->data.to.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax); 
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "TO");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_to(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRTO;
  t->toffset = head->hdata.curline;
  
  if( parse_822_resent_to(line, &t->data.rto.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_to(line, &t->data.rto.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_to(line, &t->data.rto.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_to(line, &t->data.rto.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-TO");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_cc(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltCC;
  t->toffset = head->hdata.curline;

  if( parse_822_cc(line, &t->data.cc.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_cc(line, &t->data.cc.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_cc(line, &t->data.cc.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_cc(line, &t->data.cc.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "CC");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_cc(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRCC;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_cc(line, &t->data.rcc.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_cc(line, &t->data.rcc.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_cc(line, &t->data.rcc.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_cc(line, &t->data.rcc.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-CC");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_bcc(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltBCC;
  t->toffset = head->hdata.curline;

  if( parse_822_bcc(line, &t->data.bcc.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_bcc(line, &t->data.bcc.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_bcc(line, &t->data.bcc.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_bcc(line, &t->data.bcc.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "BCC");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_bcc(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRBC;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_bcc(line, &t->data.rbc.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_bcc(line, &t->data.rbc.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_bcc(line, &t->data.rbc.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_bcc(line, &t->data.rbc.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-BCC");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_message_id(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltMID;
  t->toffset = head->hdata.curline;

  if( parse_822_message_id(line, &t->data.mid.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_message_id(line, &t->data.mid.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_message_id(line, &t->data.mid.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_message_id(line, &t->data.mid.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "MESSAGE-ID");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_resent_message_id(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltRID;
  t->toffset = head->hdata.curline;

  if( parse_822_resent_message_id(line, &t->data.rid.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_resent_message_id(line, &t->data.rid.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_resent_message_id(line, &t->data.rid.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_resent_message_id(line, &t->data.rid.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "RESENT-MESSAGE-ID");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_in_reply_to(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltIRT;
  t->toffset = head->hdata.curline;

  if( parse_822_in_reply_to(line, &t->data.irt.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_in_reply_to(line, &t->data.irt.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_in_reply_to(line, &t->data.irt.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_in_reply_to(line, &t->data.irt.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "IN-REPLY-TO");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_references(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltREF;
  t->toffset = head->hdata.curline;

  if( parse_822_references(line, &t->data.ref.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_references(line, &t->data.ref.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_references(line, &t->data.ref.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_references(line, &t->data.ref.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "REFERENCES");
    print_state(stdout, t);
  }

  return t;
}

hline_t *mkh_subject(HEADER_State *head, hline_t *t, char *line) {

  memset(t, 0, sizeof(hline_t));
  t->tag = hltSBJ;
  t->toffset = head->hdata.curline;

  if( parse_822_subject(line, &t->data.sbj.rfc822) ) {
    t->state |= (1<<H_STATE_RFC822);
  }

  if( parse_2822_subject(line, &t->data.sbj.rfc2822, RFC_2822_STRICT) ) {
    t->state |= (1<<H_STATE_RFC2822);
  } else if( parse_2822_subject(line, &t->data.sbj.rfc2822, RFC_2822_OBSOLETE) ) {
    t->state |= (1<<H_STATE_RFC2822obs);
  } else if( parse_2822_subject(line, &t->data.sbj.rfc2822, RFC_2822_LAX) ) {
    t->state |= (1<<H_STATE_RFC2822lax);
  }

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "SUBJECT");
    print_state(stdout, t);
  }

  return t;
}


typedef struct {
  const char *hname;
  hline_t *(*mkh_fun)(HEADER_State *, hline_t *, char *);
} make_tbl_t;

hline_t *make_new_header(HEADER_State *head, hline_t *t, char *line) {
  int i;
  static make_tbl_t make_tbl[] = {
    { "Return-Path", mkh_return_path },
    { "Received", mkh_received },
    { "Reply-To", mkh_reply_to },
    { "From", mkh_from },
    { "Sender", mkh_sender },
    { "Resent-Reply-To", mkh_resent_reply_to },
    { "Resent-From", mkh_resent_from },
    { "Resent-Sender", mkh_resent_sender },
    { "Date", mkh_date },
    { "Resent-Date", mkh_resent_date },
    { "To", mkh_to },
    { "Resent-To", mkh_resent_to },
    { "Cc", mkh_cc },
    { "Resent-Cc", mkh_resent_cc },
    { "Bcc", mkh_bcc },
    { "Resent-Bcc", mkh_resent_bcc },
    { "Message-ID", mkh_message_id },
    { "Resent-Message-ID", mkh_resent_message_id },
    { "In-Reply-To", mkh_in_reply_to },
    { "References", mkh_references },
    { "Subject", mkh_subject },
  };

  if( (u_options & (1<<U_OPTION_DEBUG)) ) {
    fprintf(stdout, "%s", line);
  }

  for(i = 0; i < sizeof(make_tbl)/sizeof(make_tbl_t); i++) {
    if( strncasecmp(line, make_tbl[i].hname, strlen(make_tbl[i].hname)) == 0 ) {
      if( make_tbl[i].mkh_fun ) {
	return (*make_tbl[i].mkh_fun)(head, t, line);
      }
    }
  }

  return mkh_ignore(head, t, line);
}

hline_t *head_push_header(HEADER_State *head, char *line) {
  hline_t *tmp;

  if( !line || !*line ) { return NULL; }

  if( head->hstack.top >= head->hstack.max ) {
    tmp = (hline_t *)realloc(head->hstack.hlines, 2 * head->hstack.max);
    if( !tmp ) {
      errormsg(E_ERROR,
	       "could not grow header stack (%d)\n",
	       head->hstack.max * 2);
      return NULL;
    }
    head->hstack.hlines = tmp;
    head->hstack.max *= 2;
  }

  tmp = make_new_header(head, head->hstack.hlines + head->hstack.top, line);
  if( tmp ) { 
    head->hstack.top++; 
  }

  return tmp;
}

/***********************************************************
 * MORE UTILITY FUNCTIONS                                  *
 ***********************************************************/
char *unfold_token(char *buf, int buflen, char *tokb, char *toke, char delim) {
  if( buf && tokb && toke ) { 
    buflen--; /* need space for the NUL */
    for(; (tokb < toke) && isspace((int)*tokb); tokb++); /* skip leading whitespace */
    while( (tokb < toke) && (buflen > 0) ) {
      switch(*tokb) {
      case '\n':
      case '\r':
	for(; (tokb < toke) && isspace((int)*tokb); tokb++);
	*buf++ = ' ';
	buflen--;
	break;
      case '\0':
	/* ignore */
	break;
      default:
	if( *tokb != delim ) {
	  *buf++ = *tokb;
	  buflen--;
	} else {
	  *buf = '\0';
	  return (++tokb);
	}
      }
      tokb++;
    }
    *buf = '\0';
    return tokb;
  }
  return NULL;
}


/*
 * Assumes that buf contains a mailbox token or similar and uses
 * heuristics to check for an email address. Makes sure such an email
 * is bracketed <>. Returns true if email appears to exist.
 */
bool_t bracketize_mailbox(char *buf, int buflen) {
  /* a group ends in ; othewise it's a mailbox */
  char *p;
  int n;
  if( !strrchr(buf, ';') ) {
    p = strrchr(buf, '<');
    if(  !p || (p >= strrchr(buf, '>')) ) {
      n = strlen(buf);
      if( (n + 2) < buflen ) {
	memmove(buf + 1, buf, n);
	buf[0] = '<';
	buf[n+1] = '>';
	buf[n+2] = '\0';
      }
    }
    return 1;
  }
  return 0;
} 

/***********************************************************
 * TESTING                                                 *
 ***********************************************************/
#if defined TEST_PARSER

#define CHECK_EQ(x,y) if( (x) != (y) ) { \
                           fprintf(stdout, "%s != %s\n", #x, #y); \
                           return 1; \
                       }

#define CHECK_NEQ(x,y) if( (x) == (y) ) { \
                           fprintf(stdout, "%s == %s\n", #x, #y); \
                           return 1; \
                       }

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#endif

extern HEADER_State head;

extern char *textbuf;
extern charbuf_len_t textbuf_len;

/* call this as: cat sample.headers.2822.good | pcheck 2822 good */
int main(int argc, char **argv) {
  options_t mask = 0;
  hline_count_t i = 0;

  char *pptextbuf = NULL;
  int extra_lines = 0;
  hline_t *h = NULL;

#if defined(HAVE_GETPAGESIZE)
  system_pagesize = getpagesize();
#endif
  if( system_pagesize == -1 ) { system_pagesize = BUFSIZ; }

  init_buffers();
  init_head_filter(&head);
  set_iobuf_mode(stdin);

/*   u_options |= (1<<U_OPTION_DEBUG); */

  /* now start processing */
  while( fill_textbuf(stdin, &extra_lines) ) {
      if( (textbuf[0] == '\0') || 
	  (textbuf[0] == '\n') ||
	  ((textbuf[0] == '\r') && (textbuf[1] == '\n')) ) {
	break;
      } else {
	if( pptextbuf && !isblank(*textbuf) ) {
	  pptextbuf = head_append_hline(&head, textbuf);
	  h = head_push_header(&head, pptextbuf);
	}
      }
  }

  if( (argc < 2) ) {
    fprintf(stdout, "missing arguments.\n");
    return 1;
  }
  if( strcmp(argv[1], "821") == 0 ) {
    mask |= (1<<H_STATE_RFC821);
  } else if( strcmp(argv[1], "822") == 0 ) {
    mask |= (1<<H_STATE_RFC822);
  } else if( strcmp(argv[1], "2822") == 0 ) {
    mask |= (1<<H_STATE_RFC2822)|(1<<H_STATE_RFC2822obs);
  } else if( strcmp(argv[1], "2821") == 0 ) {
    mask |= (1<<H_STATE_RFC2821);
  }

  if( !mask ) {
    fprintf(stdout, "bad mask.\n");
    return 1;
  }

  if( strcmp(argv[2], "good") == 0 ) {
    for(i = 0; i < head.hstack.top; i++) {
      if( !(head.hstack.hlines[i].state & mask) ) {
	fprintf(stdout, "incorrectly parsed %dth header\n", i+1);
	return 1;
      }
    }
    return 0;
  } else {
    for(i = 0; i < head.hstack.top; i++) {
      if( head.hstack.hlines[i].state & mask ) {
	fprintf(stdout, "incorrectly parsed %d'th header\n", i+1);
	return 1;
      }
    }
    return 0;
  }

  return 1;
}
#endif
