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

#include <locale.h>

#if defined HAVE_LANGINFO_H
#include <langinfo.h>
#if !defined CODESET
/* on OpenBSD, CODESET doesn't seem to be defined - 
   we use 3, which should be US-ASCII, but it's not ideal... */
#define CODESET 3
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#endif

#include "dbacl.h"
#include "util.h"
#include "hmine.h"

extern HEADER_State head;

extern char *textbuf;
extern charbuf_len_t textbuf_len;

extern options_t u_options;

extern char *progname;
extern char *inputfile;
extern long inputline;

extern int cmd;
extern long system_pagesize;

int exit_code = 0; /* default */


/***********************************************************
 * FUNNY FUNCTIONS                                         *
 ***********************************************************/
void print_mailbox(hline_t *h, char *buf, int buflen) {
  char *p = NULL;
  char *q = NULL;
  parse_2822_mls_t *mls = NULL;
  parse_2822_mbx_t *mbx = NULL;
  parse_2822_als_t *als = NULL;
  switch(h->tag) {
  case hltFRM: 
    p = "from"; 
    mls = &h->data.frm.rfc2822;
    break;
  case hltRFR: 
    p = "resend-from"; 
    mls = &h->data.rfr.rfc2822;
    break;
  case hltRPT: 
    p = "reply-to"; 
    als = &h->data.rpt.rfc2822;
    break;
  case hltRRT: 
    p = "resend-reply-to"; 
    als = &h->data.rrt.rfc2822;
    break;
  case hltSND: 
    p = "sender"; 
    mbx = &h->data.snd.rfc2822;
    break;
  case hltRSN: 
    p = "resend-sender"; 
    mbx = &h->data.rsn.rfc2822;
    break;
  case hltTO:  
    p = "to"; 
    als = &h->data.to.rfc2822;
    break;
  case hltRTO: 
    p = "resend-to"; 
    als = &h->data.rto.rfc2822;
    break;
  case hltCC:  
    p = "cc"; 
    als = &h->data.cc.rfc2822;
    break;
  case hltRCC: 
    p = "resend-cc"; 
    als = &h->data.rcc.rfc2822;
    break;
  case hltBCC: 
    p = "bcc"; 
    als = &h->data.bcc.rfc2822;
    break;
  case hltRBC: 
    p = "resend-bcc"; 
    als = &h->data.rbc.rfc2822;
    break;
  default:  
    /* nothing */
    break;
  }
  if( p && !(h->state & (1<<H_STATE_BAD_DATA)) ) {
    if( mls ) {
      q = unfold_token(buf, buflen, mls->mailboxl_.begin, mls->mailboxl_.end, ',');
      while( q ) {
	bracketize_mailbox(buf, buflen);
	fprintf(stdout, "%s %s\n", p, buf);
	if( q >= mls->mailboxl_.end ) { break; }
	q = unfold_token(buf, buflen, q, mls->mailboxl_.end, ',');
      }
    } else if( als ) {
      q = unfold_token(buf, buflen, als->addressl_.begin, als->addressl_.end, ',');
      while( q ) {
	bracketize_mailbox(buf, buflen);
	fprintf(stdout, "%s %s\n", p, buf);
	if( q >= als->addressl_.end ) { break; }
	q = unfold_token(buf, buflen, q, als->addressl_.end, ',');
      }
    } else if( mbx ) {
      q = unfold_token(buf, buflen, mbx->mailbox_.begin, mbx->mailbox_.end, ',');
      bracketize_mailbox(buf, buflen);
      fprintf(stdout, "%s %s\n", p, buf); 
    }
  }
}

void print_summary() {
  int i;
  char buf[1024];
/*   time_t numsec = (time_t)-1; */
/*   for(i = 0; i < head.hstack.top; i++) { */
/*     if( head.hstack.hlines[i].tag == hltRCV ) { */
/*       if( head.hstack.hlines[i].state & (1<<H_STATE_BAD_DATA) ) { */
/* 	break; */
/*       } else { */
/* 	if( numsec == (time_t)-1 ) {  */
/* 	  numsec = head.hstack.hlines[i].data.rcv.numsec; */
/* 	} */
/* 	printf("%ld ", head.hstack.hlines[i].data.rcv.numsec - numsec);	 */
/* 	numsec = head.hstack.hlines[i].data.rcv.numsec; */
/*       } */
/*     } */
/*   } */
/*   printf("\n"); */

  if( u_options & (1<<U_OPTION_HM_ADDRESSES) ) {
    /* we print out any mailboxes we find, preceded by type */
    for(i = 0; i < head.hstack.top; i++) {
      print_mailbox(head.hstack.hlines + i, buf, 1024);
    }
  }
}

/***********************************************************
 * MAIN FUNCTIONS                                          *
 ***********************************************************/

static void usage(char **argv) {
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "hmine [-vDa] [FILE]\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      checks FILE or STDIN for RFC822 header forgery.\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "hmine -V\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      prints program version.\n");
}

int hset_option(int op, char *optarg) {
  int c = 0;
  switch(op) {
  case 'a':
    u_options |= (1<<U_OPTION_HM_ADDRESSES);
    break;
  case 'D':
    u_options |= (1<<U_OPTION_DEBUG);
    break;
  case 'V':
    fprintf(stdout, "hmine version %s\n", VERSION);
    fprintf(stdout, COPYBLURB, "hmine");
    exit(1);
    break;
  default:
    c--;
    break;
  }
  return c;
}


void hprocess_file(FILE *input, HEADER_State *head) {
  char *pptextbuf = NULL;
  bool_t in_header = 1;
  int extra_lines = 0;
  hline_t *h = NULL;

  set_iobuf_mode(input);
  inputline = 0;

  /* now start processing */
  while( fill_textbuf(input, &extra_lines) ) {
    inputline++;

    if( u_options & (1<<U_OPTION_FILTER) ) {
	fprintf(stdout, "%s", textbuf);
    }

    if( in_header ) {
      if( (textbuf[0] == '\0') || 
	  (textbuf[0] == '\n') ||
	  ((textbuf[0] == '\r') && (textbuf[1] == '\n')) ) {
	in_header = 0;
      } else {
	if( pptextbuf && !isblank(*textbuf) ) {
	  h = head_push_header(head, pptextbuf);
	}
	pptextbuf = head_append_hline(head, textbuf);
      }
    } else {
      /* body of email */
    }


  }
}

void init_header_handling() {
  init_buffers();
  init_head_filter(&head);
}

void cleanup_header_handling() {
  free_head_filter(&head);
  cleanup_buffers();
}

int main(int argc, char **argv) {

  FILE *input;
  signed char op;

  void (*preprocess_fun)(void) = NULL;
  void (*postprocess_fun)(void) = print_summary;

  progname = "hmine";
  inputfile = "stdin";
  inputline = 0;

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

  init_signal_handling();

  /* parse the options */
  while( (op = getopt(argc, argv, 
		      "aDvV")) > -1 ) {
    hset_option(op, optarg);
  }


  /* set up callbacks */


  if( preprocess_fun ) { (*preprocess_fun)(); }


  init_header_handling();

  /* now process only the first file on the command line,
     or if none provided read stdin */
  if( (optind > -1) && *(argv + optind) ) {
    /* if it's a filename, process it */
    input = fopen(argv[optind], "rb");
    if( input ) {
      inputfile = argv[optind];

      u_options |= (1<<U_OPTION_STDIN);

      if( (u_options & (1<<U_OPTION_VERBOSE)) && 
	  !(u_options & (1<<U_OPTION_CLASSIFY))) {
	fprintf(stdout, "processing file %s\n", argv[optind]);
      }

      /* set some initial options */
      hprocess_file(input, &head);

      fclose(input);

    } else { /* unrecognized file name */

      errormsg(E_ERROR, "couldn't open %s\n", argv[optind]);
      usage(argv);
      return 0;

    }

  }
  /* in case no files were specified, get input from stdin */
  if( !(u_options & (1<<U_OPTION_STDIN)) &&
      (input = fdopen(fileno(stdin), "rb")) ) {

    if( (u_options & (1<<U_OPTION_VERBOSE)) && 
	!(u_options & (1<<U_OPTION_CLASSIFY)) ) {
      fprintf(stdout, "taking input from stdin\n");
    }

    hprocess_file(input, &head);

    /* must close before freeing in_iobuf, in case setvbuf was called */
    fclose(input); 

  }
  
  if( postprocess_fun ) { (*postprocess_fun)(); }

  cleanup_header_handling();

  cleanup_signal_handling();

  return exit_code;
}
