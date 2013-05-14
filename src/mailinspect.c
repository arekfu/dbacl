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

/* 
 * Note to regenerate the Makefile and configure script:
 * aclocal
 * autoconf
 * touch NEWS README AUTHORS ChangeLog
 * automake -a
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined HAVE_UNISTD_H
#include <unistd.h> 
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

#include <signal.h> /* defines SIGWINCH equals 28 */

/* we need lots of libraries for interactive mode */
#if defined HAVE_LIBSLANG && defined HAVE_LIBREADLINE

#define COMPILE_INTERACTIVE_MODE 1

#endif

/* now back to our regularly scheduled program */
#if defined COMPILE_INTERACTIVE_MODE

#include <slang.h>
#include <readline/readline.h>
#include <readline/history.h> 

#endif

#include "util.h"
#include "mailinspect.h" 

/* global variables */

extern hash_bit_count_t default_max_hash_bits;
extern hash_count_t default_max_tokens;

/* needed for scoring */
extern category_t cat[MAX_CAT];
extern category_count_t cat_count;

extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;
extern regex_count_t antiregex_count;

extern empirical_t empirical;

/* regexes used for tagging */
myregex_t tagre[MAX_RE];
char tagre_inclex[MAX_RE];
regex_count_t tagre_count = 0;

Emails emails;

extern MBOX_State mbox; /* this should only be used read-only */
extern XML_State xml;

/* for option processing */
extern char *optarg;
extern int optind, opterr, optopt;

extern options_t u_options;
extern options_t m_options;
extern charparser_t m_cp;
extern char *extn;

extern char *progname;
extern char *inputfile;
extern long inputline;

char *execute_command = NULL;
FILE *mbox_handle;
bool_t pipe_done;
int regcomp_flags;

display_state disp; /* used interactively */

extern long system_pagesize;

extern int cmd;

/***********************************************************
 * MISCELLANEOUS FUNCTIONS                                 *
 ***********************************************************/

static void usage(char **argv) {
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "mailinspect -c CATEGORY FILE [-s command]\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      sorts the emails found in the mbox folder named FILE\n");
  fprintf(stderr, 
	  "      by order of importance relative to CATEGORY.\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "mailinspect -V\n");
  fprintf(stderr, 
	  "\n");
  fprintf(stderr, 
	  "      prints program version.\n");
}


void load_rc_file() {
  FILE *input;
  char name[128];
  char buf[PIPE_BUFLEN];
  int n;

  name[0] = '\0';
  strcat(name, getenv("HOME"));
  strcat(name, "/.mailinspectrc");

  if( (input = fopen(name, "rb")) ) {
    while( !feof(input) ) {
      fgets(buf, PIPE_BUFLEN, input);
      if( buf[0] == 'F' ) {
	n = atoi(&buf[1]) - 1;
	if( (n <= 9) && (n >= 0) ) {
	  if( disp.fkey_cmd[n] ) {
	    free(disp.fkey_cmd[n]);
	  }
	  disp.fkey_cmd[n] = strdup(strchr(buf, ' ') + 1);
	  /* rid of trailing newline */
	  disp.fkey_cmd[n][strlen(disp.fkey_cmd[n]) - 1] = '\0'; 
	}
      }
    }
    fclose(input);
  }
}

/***********************************************************
 * EMAIL LIST CONSTRUCTION                                 *
 ***********************************************************/

void init_emails() {
  if( (emails.list = malloc(INITIAL_LIST_SIZE * sizeof(mbox_item))) == NULL ) {
    errormsg(E_FATAL, "couldn't allocate memory for emails\n");
  }
  if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
    if( (emails.llist = malloc(INITIAL_LIST_SIZE * sizeof(mbox_item *))) == NULL ) {
      errormsg(E_FATAL, "couldn't allocate memory for emails\n");
    }
  }
  emails.list_size = INITIAL_LIST_SIZE;
  emails.num_emails = 0;
  emails.num_limited = 0;
  emails.sortedby = 1;
}

void free_emails() {
  email_count_t i, j;

  for(i = 0; i < emails.num_emails; i++) {
    for(j = 0; j < MAX_FORMATS; j++) {
      free(emails.list[i].description[j]);
    }
  }
  free(emails.list);
  free(emails.llist);
}

void build_scores(weight_t *s) {
  double lambda;

  lambda = (cat[0].model_num_docs > 0) ? 
    (cat[0].model_full_token_count / cat[0].model_num_docs) : 100;

  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    cat[0].score_shannon = 
      -( cat[0].score_shannon / ((score_t)cat[0].complexity) -
	 log((score_t)cat[0].complexity) );
    cat[0].score_div = 
      -(cat[0].score/cat[0].complexity + cat[0].score_shannon);
  }

  /* various experimental scoring systems */
  s[0] = -cat[0].score - log_poisson(cat[0].complexity, lambda); /* arbitrary */
  s[1] = -cat[0].score / cat[0].complexity;
  s[2] = cat[0].score_div;
  s[3] = (weight_t)gamma_pvalue(&cat[0], cat[0].score_div)/10;
}

/* formats descriptions for email.index_format > 0 */
void build_other_descriptions(char **d) {
  char *p;

  /* format some extra descriptions */
  if( (d[1] = malloc(HEADER_BUFLEN * sizeof(char))) ) {
    d[1][0] = '\0';
    strcat(d[1], "[");
    strncat(d[1], emails.header.from + 6, 20);
    strcat(d[1], "] ");
    strncat(d[1], emails.header.subject + 9, 60);

    /* clean up embedded newlines */
    for(p = d[1]; *p; p++) { if( *p == '\n' ) { *p = ' '; } }
    *(--p) = '\n';
  }
}

/* fills the emails list one line at a time */
char *process_email_line(char *textbuf) {
  regex_count_t r;

  if( mbox.prev_line_empty ) {
    if( strncmp(textbuf, "From ", 5) == 0 ) {

      /* save the previous email's score etc */
      if( emails.num_emails > 0 ) {
	
	emails.list[emails.num_emails - 1].state &= ~(1<<STATE_TAGGED);
	build_scores(emails.list[emails.num_emails - 1].score);
	build_other_descriptions(emails.list[emails.num_emails - 1].description);

	if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
	  emails.llist[emails.num_emails - 1] = &emails.list[emails.num_emails - 1];
	}
      }

      /* reset calculations */
      cat[0].score = 0.0;
      cat[0].complexity = 0.0;
      cat[0].fcomplexity = 0;

      if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
	clear_empirical(&empirical);
      }

      /* now increment email number */
      emails.num_emails++;
      if( emails.num_emails >= emails.list_size ) {
	emails.list_size *= 2;

	if( (emails.list = realloc(emails.list, 
				   emails.list_size * sizeof(mbox_item))) == NULL ) {
	  errormsg(E_FATAL,
		  "couldn't allocate memory for emails, failed at %ld bytes\n", 
		  (long)emails.list_size * sizeof(mbox_item));
	}
	if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
	  if( (emails.llist = realloc(emails.llist, 
				      emails.list_size * sizeof(mbox_item *))) == NULL ) {
	    errormsg(E_FATAL,
		    "couldn't allocate memory for emails, failed at %ld bytes\n", 
		    (long)emails.list_size * sizeof(mbox_item *));
	  }
	}
      }
      if( emails.list[emails.num_emails - 1].state & (1<<STATE_LIMITED) ) {
	++emails.num_limited; 
      }

      /* fill in basic info for this new email */
      /* note that seek position is exactly at the end of the from line */
      emails.list[emails.num_emails - 1].description[0] = strdup(textbuf);
      emails.list[emails.num_emails - 1].seekpos = mbox_handle ? ftell(mbox_handle) : 0;
      if( !tagre_count ) {
	emails.list[emails.num_emails - 1].state |= (1<<STATE_LIMITED);
      } else {
	if( tagre_inclex[0] == TAGRE_INCLUDE ) {
	  emails.list[emails.num_emails - 1].state &= ~(1<<STATE_LIMITED);
	} else {
	  emails.list[emails.num_emails - 1].state |= (1<<STATE_LIMITED);
	}
      }
      memset(emails.header.from, 0, HEADER_BUFLEN);
      memset(emails.header.subject, 0, HEADER_BUFLEN);
    } 
  } 

  /* next, we glean information from the header */
  if( mbox.state == msHEADER ) {
    if( strncasecmp(textbuf, "From:", 5) == 0 ) {
      strncpy(emails.header.from, textbuf, HEADER_BUFLEN);
    } else if( strncasecmp(textbuf, "Subject:", 8) == 0 ) {
      strncpy(emails.header.subject, textbuf, HEADER_BUFLEN);
    }
  }

  /* next, we see if the body contains stuff we explicitly want or not */
  for(r = 0; r < tagre_count; r++) {
    if( regexec(&tagre[r].regex, textbuf, 0, NULL, 0) == 0 ) {
      if( tagre_inclex[r] == TAGRE_INCLUDE) {
	emails.list[emails.num_emails - 1].state |= (1<<STATE_LIMITED);
      } else {
	emails.list[emails.num_emails - 1].state &= ~(1<<STATE_LIMITED);
      }
    }
  }

  return textbuf;
}

/* this is a companion function for process_email_line() */
void process_last_email() {

  if( emails.num_emails > 0 ) {
    
    emails.list[emails.num_emails - 1].state &= ~(1<<STATE_TAGGED);
    build_scores(emails.list[emails.num_emails - 1].score);
    build_other_descriptions(emails.list[emails.num_emails - 1].description);

    if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
      emails.llist[emails.num_emails - 1] = &emails.list[emails.num_emails - 1];
    }
  }

  cat[0].score = 0.0;
  cat[0].complexity = 0;
  cat[0].fcomplexity = 0;
  
  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    clear_empirical(&empirical);
  }

}

/***********************************************************
 * EMAIL SCORING AND SORTING                               *
 ***********************************************************/

/* used by qsort() */
int compare_scores(const void *a, const void *b) {

  if( emails.sortedby * (((mbox_item *)a)->score[emails.score_type] - 
			 ((mbox_item *)b)->score[emails.score_type]) < 0.0 ) {
    return -1;
  } else if( ((mbox_item *)a)->score[emails.score_type] == 
	     ((mbox_item *)b)->score[emails.score_type] ) {
    return 0;
  } else {
    return +1;
  }

}

/* call this after a limit change */
void recalculate_limited() {
  email_count_t i;
  emails.num_limited = 0;
  for(i = 0; i < emails.num_emails; i++) {
    if( emails.list[i].state & (1<<STATE_LIMITED) ) {
      emails.llist[emails.num_limited++] = &emails.list[i];
    }
  }
}

void reverse_sort() {
  email_count_t i;
  mbox_item temp;

  for( i = 0; i <= emails.num_emails / 2; i++ ) {
    memcpy(&temp, &emails.list[i], sizeof(mbox_item));
    memcpy(&emails.list[i], &emails.list[emails.num_emails - i - 1], sizeof(mbox_item));
    memcpy(&emails.list[emails.num_emails - i - 1], &temp, sizeof(mbox_item));
  }

  emails.sortedby = -emails.sortedby;

  recalculate_limited();
}

/* make all emails visible at once */
void limit_all() {
  email_count_t i;
  for(i = 0; i < emails.num_emails; i++) {
    emails.list[i].state |= (1<<STATE_LIMITED);
  }

  recalculate_limited();
}

/* if positive, returns true iff the regex matches within the email body 
   if not positive, returns true iff the regex does not match */
bool_t grep_single_email(FILE *input, regex_t *re, 
			 bool_t positive, mbox_item *which) {
  bool_t done;
  char buf[PIPE_BUFLEN];

  fseek(input, which->seekpos, SEEK_SET);
  done = 0;
  while( !done ) {
    fgets(buf, PIPE_BUFLEN, input);
    if( feof(input) || (strncmp(buf, "From ", 5) == 0) ) {
      done = 1;
    } else {
      if( regexec(re, buf, 0, NULL, 0) == 0 ) {
	return (positive) ? 1 : 0;
      }
    }
  }
  /* if we're here, then we found no match */
  return (positive) ? 0 : 1;
}

/* opens the current filename and for every limited
   message, looks to see if it matches (or not) the regex.
   Messages which fail the test are hidden */
void limit_on_regex(bool_t positive, char *regex) {
  FILE *input;
  email_count_t i;
  mbox_item *w;
  regex_t re;

  /* GNU regexes use regular strings */
  if( regcomp(&re, regex, regcomp_flags) != 0 ) {
    errormsg(E_WARNING,
	    "could not compile regular expression '%s', ignored\n", 
	    regex);
  }

  if( emails.filename ) {
    input = fopen(emails.filename, "rb");

    for(i = 0; i < emails.num_limited; i++) {
      w = emails.llist[i];
      if( grep_single_email(input, &re, positive, w) ) {
	w->state |= (1<<STATE_LIMITED);
      } else {
	w->state &= ~(1<<STATE_LIMITED);
      }
    }
    fclose(input);
  }

  recalculate_limited();
}

int email_line_filter(MBOX_State *mbox, char *buf) {
  return mbox_line_filter(mbox, buf, &xml);
}

#if defined HAVE_MBRTOWC
int w_email_line_filter(MBOX_State *mbox, wchar_t *buf) {
  return w_mbox_line_filter(mbox, buf, &xml);
}
#endif

/* called when we want to read in and sort by category score */
void read_mbox_and_sort_list(FILE *input) {

  void (*word_fun)(char *, token_type_t, regex_count_t) = NULL;
  char *(*pre_line_fun)(char *) = NULL;
  void (*post_line_fun)(char *) = NULL;

  /* set up callbacks */
  word_fun = score_word;
  pre_line_fun = process_email_line;
  mbox_handle = input; 

  inputfile = emails.filename;

  /* process input file */
  reset_mbox_line_filter(&mbox);
  if( !(m_options & (1<<M_OPTION_I18N)) ) {
    process_file(input, email_line_filter, 
		 ((m_options & (1<<M_OPTION_XML)) ? xml_character_filter : NULL), 
		 word_fun, pre_line_fun, post_line_fun);
  } else {
#if defined HAVE_MBRTOWC
    w_process_file(input, w_email_line_filter, 
		   ((m_options & (1<<M_OPTION_XML)) ? w_xml_character_filter : NULL), 
		   word_fun, pre_line_fun, post_line_fun);
#endif
  }
  process_last_email(); /* don't forget last email */
  mbox_handle = NULL;
  /* sort the emails */
  qsort(emails.list, emails.num_emails, sizeof(mbox_item), compare_scores);

  if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
    limit_all();
  }
}


/***********************************************************
 * EMAIL TAGGING AND PIPING (NON-INTERACTIVE)              *
 ***********************************************************/

/* tags all emails */
void tag_all(bool_t tag) {
  email_count_t i;
  for(i = 0; i < emails.num_emails; i++) {
    if( !tag ) {
      emails.list[i].state &= ~(1<<STATE_TAGGED);
    } else {
      emails.list[i].state |= (1<<STATE_TAGGED);
    }
  }
}

#if defined COMPILE_INTERACTIVE_MODE
/* run once: we don't renew the signal handler on receipt */
static void sigpipe_handler(int sig) {
  pipe_done = 1;
}
#endif

/* pipes a single email body to some file.
   In interactive mode, we catch the PIPE signal
   in case the receiving command closed the connection
   prematurely. In that case, we simply stop sending. */
void pipe_single_email(mbox_item *what, FILE *output) {
  FILE *input;
  char buf[PIPE_BUFLEN];

  if( emails.filename ) {
    input = fopen(emails.filename, "rb");
    if( input ) {
#if defined COMPILE_INTERACTIVE_MODE
      if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
	SLsignal(SIGPIPE, sigpipe_handler);
      }
#endif
      fseek(input, what->seekpos, SEEK_SET);
      pipe_done = 0;
      fputs(what->description[0], output);
      while( !pipe_done ) {
	fgets(buf, PIPE_BUFLEN, input);
	if( feof(input) || (strncmp(buf, "From ", 5) == 0) ) {
	  pipe_done = 1;
	} else {
	  fputs(buf, output);
	}
      }
    }
    fclose(input);
  }

}

/* pipes all emails to an external command.
 Each email is sent to a different instance of the command */
void pipe_all_to_command(char *command) {
  email_count_t i;
  FILE *pipe;

  for(i = 0; i < emails.num_emails; i++) {
    if( (pipe = popen(command, "w")) ) {
      pipe_single_email(&emails.list[i], pipe);
      pclose(pipe);
    }
  }
}


/* sends the sorted list of emails to a file */
void display_results(FILE *output) {
  email_count_t i;

  fprintf(output, "# position|  score | description\n");

  for(i = 0; i < emails.num_emails; i++) {
    fprintf(output, "%-10ld %8.1f %s", 
	    emails.list[i].seekpos, 
	    emails.list[i].score[emails.score_type],
	    emails.list[i].description[emails.index_format]);
  }

}


/***********************************************************
 * EMAIL TAGGING AND PIPING (INTERACTIVE)                  *
 ***********************************************************/
/* tags currently displayed email */
void tag_current(bool_t tag) {
  if( tag ) {
    emails.llist[disp.first_visible + disp.highlighted]->state |= (1<<STATE_TAGGED);
  } else {
    emails.llist[disp.first_visible + disp.highlighted]->state &= ~(1<<STATE_TAGGED);
  }
}

/* tags all visible emails */
void tag_all_limited(bool_t tag) {
  email_count_t i;
  for(i = 0; i < emails.num_limited; i++) {
    if( !tag ) {
      emails.llist[i]->state &= ~(1<<STATE_TAGGED);
    } else {
      emails.llist[i]->state |= (1<<STATE_TAGGED);
    }
  }
}


/***********************************************************
 * S-LANG HELPER FUNCTIONS                                 *
 ***********************************************************/

#if COMPILE_INTERACTIVE_MODE

void ephemeral_message(char *s) {

  SLsmg_gotorc(disp.num_rows - 1, 0);
  SLsmg_set_color(3);
  SLsmg_write_string(s);
  SLsmg_erase_eol();
  SLsmg_refresh();

}

static void sigwinch_handler (int sig) {

  SLsig_block_signals();

  SLtt_get_screen_size();
  SLsmg_reinit_smg();

  if( !disp.delay_sigwinch ) {
    redraw_current_state();
  }

  SLsig_unblock_signals();

  SLsignal(SIGWINCH, sigwinch_handler);

}
 
static void rline_update(unsigned char *buf, int len, int col) {
  SLsmg_gotorc (SLtt_Screen_Rows - 1, 0); 
  SLsmg_set_color(2);
  if( buf ) {
    SLsmg_write_nchars ((char *) buf, len); 
  }
  SLsmg_erase_eol ();      
  SLsmg_gotorc (SLtt_Screen_Rows - 1, col);
  SLsmg_refresh ();     
}

/* make sure to call free on the returned string once no longer needed */
char *get_input_line(char *title, char *prefill) {

#if (SLANG_VERSION >= 20000) /* libslang2 */

  char *s = NULL;
  char *buf;
  unsigned int len = 0;
  SLrline_Type *rli;

  rli = SLrline_open(SLtt_Screen_Cols, SL_RLINE_BLINK_MATCH);
  if( rli ) {
    SLtt_set_cursor_visibility(1);
    SLsmg_normal_video();
    len = 255;
    rline_update(NULL, len, strlen(title));
    buf = SLrline_read_line(rli, title, &len);
    if( buf ) {
      s = strdup(buf);
      add_history(s); 
    }
    SLfree(buf);
    SLtt_set_cursor_visibility(0);
  }

#else /* libslang1 */

  char *s;
  SLang_RLine_Info_Type *rli;

  s = malloc(256 * sizeof(char));
  if( s ) {

    rli = (SLang_RLine_Info_Type *)malloc(sizeof(SLang_RLine_Info_Type));
    if( rli ) {
      memset(rli, 0, sizeof(SLang_RLine_Info_Type));
      rli->buf = (unsigned char *)s; /* buf is unsigned */
      rli->buf_len = 255;
      rli->tab = 8;
      rli->dhscroll = 5;
      rli->getkey = SLang_getkey;
      rli->tt_goto_column = NULL;
      rli->update_hook = rline_update;
      rli->flags |= SL_RLINE_BLINK_MATCH;
      rli->input_pending = SLang_input_pending;
      SLang_init_readline(rli);
      rli->edit_width = SLtt_Screen_Cols - 1;
      rli->prompt = title;

      /* now prefill if necessary */
      if( *prefill ) {
	strncpy(s, prefill, 255); 
	rli->buf[255] = '\0';
	rli->point = strlen((char *)rli->buf);
      } else {
	*rli->buf = 0;
	rli->point = 0;
      }

      SLtt_set_cursor_visibility(1);
      SLsmg_normal_video();
      SLang_read_line(rli);
      add_history(s); /* buf is unsigned */
      SLtt_set_cursor_visibility(0);
      free(rli);
    }
  }

#endif

  return s;
}

#endif

/***********************************************************
 * INTERACTIVE DISPLAY FUNCTIONS                           *
 ***********************************************************/

#if COMPILE_INTERACTIVE_MODE

void interactive_pipe_current(char *prefill) {
  char *s;
  FILE *pipe;
  mbox_item *what;

  if( (s = get_input_line("Send current email to command: ", prefill)) ) { 

    if( s[0] != '\0' ) {
      disp.delay_sigwinch = 1;
      SLsmg_suspend_smg();

      if( (pipe = popen(s, "w")) ) {
	what = emails.llist[disp.first_visible + disp.highlighted];
	pipe_single_email(what, pipe);
	pclose(pipe);
      }

      SLsmg_resume_smg();
      disp.delay_sigwinch = 0;
    }

    free(s);
  }
}

void interactive_pipe_all_tagged(char *prefill) {
  char *s;
  email_count_t i;
  FILE *pipe;

  if( (s = get_input_line("Send TAGGED emails to command: ", prefill)) ) { 

    if( s[0] != '\0' ) {
      disp.delay_sigwinch = 1;
      SLsmg_suspend_smg();
      
      for(i = 0; i < emails.num_emails; i++) {
	if( emails.list[i].state & (1<<STATE_TAGGED) ) {
	  if( (pipe = popen(s, "w")) ) {
	    pipe_single_email(&emails.list[i], pipe);
	    pclose(pipe);
	  }
	}
      }
      
      SLsmg_resume_smg();
      disp.delay_sigwinch = 0;
    }
    
    free(s);
  }
}

void interactive_categorize() {
  char *s;
  FILE *input;

  if( (s = get_input_line("Sort current mailbox by category: ", "")) ) { 

    if( s[0] != '\0' ) {
      cat[1].fullfilename = sanitize_path(s, extn);
      if( load_category(&cat[1]) && 
	  (input = fopen(emails.filename, "rb")) ) {

	sanitize_model_options(&m_options, &m_cp, &cat[1]);
	ephemeral_message("Please wait, recalculating scores");
	/* loaded category successfully, now free old resources */
	free_category(&cat[0]);
	memcpy(&cat[0], &cat[1], sizeof(category_t));
      
	free_emails();
	init_emails();
	
	read_mbox_and_sort_list(input);
      
	if( u_options & (1<<U_OPTION_REVSORT) ) {
	  reverse_sort();
	  recalculate_limited();
	}
      
	fclose(input);
      } else {
	ephemeral_message("Sorry, the category could not be loaded");
      }
    }

    free(s);
  }
}


void interactive_search(bool_t incl, char *prefill) {
  char *s;
  if( incl ) {
    s = get_input_line("Find emails containing: ", prefill);
  } else {
    s = get_input_line("Find emails NOT containing: ", prefill);
  }

  if( s ) {
    ephemeral_message("Searching");
    if( s[0] == '\0' ) {
      limit_all();
    } else {
      limit_on_regex(incl, s);
    }
    
    free(s);
  }
}

void view_current_email() {
  FILE *pipe;
  char *pager;

  disp.delay_sigwinch = 1;
  SLsmg_suspend_smg();

  if( !(pager = getenv("PAGER")) ) {
    pager = "less";
  }

  if( (pipe = popen(pager, "w")) ) {
    pipe_single_email(emails.llist[disp.first_visible + disp.highlighted], pipe);
    pclose(pipe);
  }

  SLsmg_resume_smg();
  disp.delay_sigwinch = 0;
}


/* displays a selection of limited (visible) emails */
void redraw_current_state() {
  int r = 0;
  mbox_item *e;

  SLsig_block_signals();

  disp.num_rows = SLtt_Screen_Rows;
  disp.num_cols = SLtt_Screen_Cols;

  SLsmg_normal_video();

  if( emails.num_limited > 0 ) {
    for(r = 0; r < (disp.num_rows - 2); r++) {
      if( r + disp.first_visible < emails.num_limited ) {
	
	e = emails.llist[r + disp.first_visible];
	
	SLsmg_gotorc(r + 1, 0);
	SLsmg_printf("%c %8.1f %s", 
		     (e->state & (1<<STATE_TAGGED)) ? 'T' : ' ',
		     e->score[emails.score_type],
		     e->description[emails.index_format]);
	SLsmg_erase_eol();
      } else {
	SLsmg_gotorc(r + 1, 0);
	SLsmg_erase_eol();
      }
    }
  } else {
    SLsmg_cls();
    r = disp.num_rows - 2;
  }

  SLsmg_reverse_video();


  e = emails.llist[disp.first_visible + disp.highlighted];

  SLsmg_gotorc(disp.highlighted + 1, 0);
  
  if( emails.num_limited > 0 ) {
    SLsmg_printf("%c %8.1f %s", 
		 (e->state & (1<<STATE_TAGGED)) ? 'T' : ' ',
		 e->score[emails.score_type],
		 e->description[emails.index_format]);
  }
  SLsmg_erase_eol();

  SLsmg_set_color(2);

  SLsmg_gotorc(0,0);
  SLsmg_printf("mailinspect %s %ld msgs sorted(%d) by category %s", 
	       emails.filename, (long)emails.num_limited, emails.score_type, cat[0].filename);
  SLsmg_erase_eol();

  SLsmg_gotorc(r + 1, 0);
  SLsmg_printf("q: quit o: scoring z: rev. sort /: search tT: tag uU: untag sS: send to shell p: summary c: category");
  SLsmg_erase_eol();
  SLsmg_refresh();

  SLsig_unblock_signals();

}

void init_disp_state() {
  int i;

  /* initialize the display state */
  disp.first_visible = 0;
  disp.highlighted = 0;

  SLtt_set_color(0, NULL, "lightgray", "default");
  SLtt_set_color(1, NULL, "black", "lightgray");
  SLtt_set_color(2, NULL, "lightgray", "blue");
  SLtt_set_color(3, NULL, "red", "blue");

  SLtt_set_cursor_visibility(0);

  for(i = 0; i < 10; i++) {
    disp.fkey_cmd[i] = NULL;
  }

  load_rc_file();
}

/* this is the main key processing loop for interactive mode */
void display_results_interactively() {

  bool_t done = 0;
  int c, c1;

  /* set up the terminal etc. */
  SLang_init_tty(-1, 0, 0);
  SLtt_get_terminfo();
  if(-1 == SLsmg_init_smg() ) {
    fprintf (stderr, "Unable to initialize terminal.");
    return;
  }
  SLkp_init(); /* for cursor keys */

  SLsignal(SIGWINCH, sigwinch_handler);

  init_disp_state();

  while(!done) {

    redraw_current_state();

    c = SLang_getkey();

    /* if it's a meta combination, translate */
    if( c == '' ) {
      c1 = SLang_getkey();
      switch(c1) {

      case '<':
	c = SL_KEY_HOME;
	break;

      case '>':
	c = SL_KEY_END;
	break;

      case 'v':
      case 'V':
	c = SL_KEY_PPAGE;
	break;

      case 'r':
      case 'R':
	c = '?';
	break;

      case 's':
      case 'S':
	c = '/';
	break;

      default:
	/* could be special cursor keys */
	SLang_ungetkey(c1);
	SLang_ungetkey(c);
	c = SLkp_getkey();
      }
    }

    /* handle key press */
    switch(c) {

    case 'q':
    case 'Q':
      done = 1;
      break;

    case 'c':
      interactive_categorize();
      disp.highlighted = disp.first_visible = 0;
      break;

    case '\r':
    case 'v':
      if( emails.num_limited > 0 ) {
	view_current_email();
      }
      break;

    case SL_KEY_F(1):
      if( emails.num_limited > 0 && disp.fkey_cmd[0] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[0]);
      }
      break;

    case SL_KEY_F(2):
      if( emails.num_limited > 0 && disp.fkey_cmd[1] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[1]);
      }
      break;

    case SL_KEY_F(3):
      if( emails.num_limited > 0 && disp.fkey_cmd[2] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[2]);
      }
      break;

    case SL_KEY_F(4):
      if( emails.num_limited > 0 && disp.fkey_cmd[3] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[3]);
      }
      break;

    case SL_KEY_F(5):
      if( emails.num_limited > 0 && disp.fkey_cmd[4] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[4]);
      }
      break;

    case SL_KEY_F(6):
      if( emails.num_limited > 0 && disp.fkey_cmd[5] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[5]);
      }
      break;

    case SL_KEY_F(7):
      if( emails.num_limited > 0 && disp.fkey_cmd[6] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[6]);
      }
      break;

    case SL_KEY_F(8):
      if( emails.num_limited > 0 && disp.fkey_cmd[7] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[7]);
      }
      break;

    case SL_KEY_F(9):
      if( emails.num_limited > 0 && disp.fkey_cmd[8] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[8]);
      }
      break;

    case SL_KEY_F(10):
      if( emails.num_limited > 0 && disp.fkey_cmd[9] ) {
	interactive_pipe_all_tagged(disp.fkey_cmd[9]);
      }
      break;

    case 's':
      if( emails.num_limited > 0 ) {
	interactive_pipe_current("");
      }
      break;

    case 'S':
      if( emails.num_limited > 0 ) {
	interactive_pipe_all_tagged("");
      }
      break;

    case 'o':
      if( ++emails.score_type >= MAX_SCORES ) {
	emails.score_type = 0;
      }
      qsort(emails.list, emails.num_emails, sizeof(mbox_item), compare_scores);
      recalculate_limited();
      break;

    case 'p':
      if( ++emails.index_format >= MAX_FORMATS ) {
	emails.index_format = 0;
      }
      break;

    case 't':
      if( emails.num_limited > 0 ) {
	tag_current(1);
      }
      break;

    case 'T':
      if( emails.num_limited > 0 ) {
	tag_all_limited(1);
      }
      break;

    case 'u':
      if( emails.num_limited > 0 ) {
	tag_current(0);
      }
      break;

    case 'U':
      if( emails.num_limited > 0 ) {
	tag_all_limited(0);
      }
      break;

    case 'z':
      reverse_sort();
      recalculate_limited();
      break;

    case 'G':
    case SL_KEY_END:
      if( emails.num_limited > 0 ) {
	disp.first_visible = emails.num_limited - 1;
      } else {
	disp.highlighted = disp.first_visible = 0;
      }
      break;

    case '1':
    case SL_KEY_HOME:
      disp.first_visible = 0;
      break;

    case '':
    case SL_KEY_PPAGE:
      if( disp.first_visible > disp.num_rows )
	{ disp.first_visible -= disp.num_rows; }
      else
	{ disp.first_visible = 0; disp.highlighted = 0; }

      /* assert emails.num_limited >= disp.first_visible */
      if( disp.highlighted > (emails.num_limited - disp.first_visible) ) {
	disp.highlighted = (emails.num_limited - disp.first_visible);
      }
      break;

    case 'k':
    case '':
    case SL_KEY_UP:
      if( disp.highlighted > 0 ) {
	disp.highlighted--;
      } else {
	if( disp.first_visible > 1 ) 
	  { disp.first_visible -= 1; }
	else
	  { disp.first_visible = 0; }
      }
      break;

    case 'j':
    case '':
    case SL_KEY_DOWN:
      if( emails.num_limited > 0 ) {
	if( disp.highlighted < (emails.num_limited - disp.first_visible - 1) ) {
	  if( disp.highlighted < (disp.num_rows - 3) ) {
	    disp.highlighted++;
	  } else {
	    if( (disp.first_visible += 1) >= emails.num_limited ) 
	      { disp.first_visible = emails.num_limited - 1; }
	  }
	}
      } else {
	disp.highlighted = disp.first_visible = 0;
      }
      break;

    case '':
    case '':
    case ' ':
    case SL_KEY_NPAGE:
      if( emails.num_limited > 0 ) {
	if( (disp.first_visible += disp.num_rows) >= emails.num_limited ) 
	  { disp.first_visible = emails.num_limited - 1; }

	if( disp.highlighted > (emails.num_limited - disp.first_visible) ) {
	  disp.highlighted = (emails.num_limited - disp.first_visible) - 1;
	}
      } else {
	disp.highlighted = disp.first_visible = 0;
      }
      break;

    case '?':
      interactive_search(0, "");
      disp.highlighted = disp.first_visible = 0;
      break;
    case '/':
      interactive_search(1, "");
      disp.highlighted = disp.first_visible = 0;
      break;

    default:
      break;
    }

  }

  /* we're done */
  SLsmg_reset_smg();
  SLang_reset_tty();
}

#endif

/***********************************************************
 * MAIN FUNCTIONS                                          *
 ***********************************************************/

int main(int argc, char **argv) {

  regex_count_t k;
  FILE *input;
  signed char op;


  void (*preprocess_fun)(void) = NULL;
  void (*postprocess_fun)(void) = NULL;


  progname = "mailinspect";
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

  /* parse the options */
  while( (op = getopt(argc, argv, "c:g:G:iIjo:p:s:VzZ")) > -1 ) {

    switch(op) {
    case 'j':
      m_options |= (1<<M_OPTION_CASEN);
      break;
    case 'I':
      u_options |= (1<<U_OPTION_INTERACTIVE);
      break;

    case 'V':
      fprintf(stdout, "mailinspect version %s\n", VERSION);
      fprintf(stdout, COPYBLURB, "mailinspect");
      exit(1);
      break;

    case 'z':
      u_options |= (1<<U_OPTION_REVSORT);
      break;

    case 'o':
      if( !*optarg ) {
	errormsg(E_ERROR, "you need a number with the -o switch\n");
	usage(argv);
	exit(0);
      }
      u_options |= (1<<U_OPTION_SCORES);
      if( (emails.score_type = atoi(optarg)) >= MAX_SCORES ) {
	emails.score_type = 0;
      }
      break;

    case 'p':
      if( !*optarg ) {
	errormsg(E_ERROR, "you need a number with the -p switch\n");
	usage(argv);
	exit(0);
      }
      u_options |= (1<<U_OPTION_FORMAT);
      if( (emails.index_format = atoi(optarg)) >= MAX_FORMATS ) {
	emails.index_format = 0;
      }
      break;

    case 'c':
      if( cat_count >= 1 ) {
	errormsg(E_WARNING,
		"maximum reached, category ignored\n");
      } else {
	u_options |= (1<<U_OPTION_CLASSIFY);

	cat[cat_count].fullfilename = sanitize_path(optarg, "");

	if( !*optarg ) {
	  errormsg(E_ERROR, "category needs a name\n");
	  usage(argv);
	  exit(0);
	}
	if( !load_category(&cat[cat_count]) ) {
	  errormsg(E_FATAL,
		  "could not load category %s\n", 
		  cat[cat_count].fullfilename);
	}
	sanitize_model_options(&m_options, &m_cp, &cat[cat_count]);
	cat_count++;
      }
      break;

    case 'G':
    case 'g':
      tagre_inclex[tagre_count] = (op == 'g') ? TAGRE_INCLUDE : TAGRE_EXCLUDE;
      if( regcomp(&tagre[tagre_count].regex, optarg, regcomp_flags) != 0 ) {
	errormsg(E_WARNING,
		 "could not compile regular expression '%s', ignored\n", 
		 optarg);
      } else {
	tagre[tagre_count].string = optarg;
	tagre_count++;
      }
      break;

    case 'i':
      m_options |= (1<<M_OPTION_I18N);
#if defined HAVE_LANGINFO_H
      if( !strcmp(nl_langinfo(CODESET), "UTF-8") ) {
	errormsg(E_WARNING, "you have UTF-8, so -i is not needed.\n");
      }
#endif
      break;


    case 's':
      execute_command = optarg;
      break;

    default:
      break;
    }
  }
  
  /* end option processing */
    
  /* consistency checks */
  if( !(u_options & (1<<U_OPTION_CLASSIFY)) ) {
    errormsg(E_ERROR,
	    "please use -c option\n");
    usage(argv);
    exit(0);
  }

  if( !(u_options & (1<<U_OPTION_FORMAT)) ) {
    if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
      emails.index_format = 1;
    } else {
      emails.index_format = 0;
    }
  }
  if( !(u_options & (1<<U_OPTION_SCORES)) ) {
    emails.score_type = 0;
  }
  if( m_options & (1<<M_OPTION_CASEN) ) {
    regcomp_flags = REG_EXTENDED|REG_NOSUB;
  } else {
    regcomp_flags = REG_EXTENDED|REG_NOSUB|REG_ICASE;
  }

  /* setup */

  if( m_options & (1<<M_OPTION_CALCENTROPY) ) {
    init_empirical(&empirical, 
		   default_max_tokens, 
		   default_max_hash_bits); /* sets cached to zero */
  }

  init_file_handling();
  init_emails();

  /* now do processing */

  if( preprocess_fun ) { (*preprocess_fun)(); }

  /* now process the first file on the command line */
  if( (optind > -1) && *(argv + optind) ) {
    /* if it's a filename, process it */
    if( (input = fopen(argv[optind], "rb")) ) {


      if( cat[0].model_num_docs == 0 ) {
	errormsg(E_WARNING,
		"category %s was not created for emails."
		" Messages may not be sorted optimally.\n", cat[0].filename);
      }
      
      if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
	fprintf(stderr,
		"Reading %s, please wait...\n", argv[optind]);
      }

      emails.filename = argv[optind];
      read_mbox_and_sort_list(input);

      fclose(input);

      if( u_options & (1<<U_OPTION_REVSORT) ) {
	reverse_sort();
      }

      if( u_options & (1<<U_OPTION_INTERACTIVE) ) {
#if defined COMPILE_INTERACTIVE_MODE	
	display_results_interactively();
#else
	errormsg(E_ERROR,
		 "\n"
		 "       sorry, interactive mode not available.\n"
		 "       Make sure you have libslang and libreadline\n"
		 "       on your system and reconfigure/recompile.\n");
#endif
      } else {
	if( execute_command ) {
	  pipe_all_to_command(execute_command);
	} else {
	  display_results(stdout);
	}
      }


    } else {

      errormsg(E_ERROR, "couldn't open %s\n", argv[optind]);
      usage(argv);
      exit(0);

    }
  } else {
    errormsg(E_ERROR, "no mbox file specified.\n");
    usage(argv);
    exit(0);
  }
  
  if( postprocess_fun ) { (*postprocess_fun)(); }

  cleanup_file_handling();

  for(k = 0; k < regex_count; k++) {
    regfree(&re[k].regex);
  }
  for(k = 0; k < antiregex_count; k++) {
    regfree(&re[k+MAX_RE].regex);
  }

  exit(1);
}
