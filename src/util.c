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

#if defined HAVE_UNISTD_H
#include <unistd.h> 
#endif

#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include "util.h"

/*@constant double M_LN2@*/

extern char *progname;
extern char *inputfile;
extern long inputline;
extern options_t u_options;
extern options_t m_options;
extern int cmd;

extern char *textbuf;
extern charbuf_len_t textbuf_len;

#if defined HAVE_MBRTOWC
extern wchar_t *wc_textbuf;
extern charbuf_len_t wc_textbuf_len;
#endif

extern void *in_iobuf;
extern void *out_iobuf;

extern long system_pagesize;

int sa_signal = 0;
signal_cleanup_t cleanup = { NULL };

/***********************************************************
 * GLOBAL BUFFERS                                          *
 ***********************************************************/
void init_buffers() {
  /* preallocate primary text holding buffer */
  textbuf_len = system_pagesize;
  textbuf = (char *)malloc(textbuf_len);

  MADVISE(textbuf, sizeof(char) * textbuf_len, MADV_SEQUENTIAL);

#if defined HAVE_POSIX_MEMALIGN
  /* buffer must exist until after fclose() if used in setvbuf() */
  if( 0 != posix_memalign(&in_iobuf, system_pagesize, 
			  BUFFER_MAG * system_pagesize) ) {
    in_iobuf = NULL; /* just to be sure */
  }
  /* buffer must exist until after fclose() if used in setvbuf() */
  if( 0 != posix_memalign(&out_iobuf, system_pagesize, 
			  BUFFER_MAG * system_pagesize) ) {
    out_iobuf = NULL; /* just to be sure */
  }
#elif defined HAVE_MEMALIGN
  /* memalign()ed memory can't be reclaimed by free() */
  in_iobuf = (void *)memalign(system_pagesize, BUFFER_MAG * system_pagesize);
  out_iobuf = (void *)memalign(system_pagesize, BUFFER_MAG * system_pagesize);
#elif defined HAVE_VALLOC
  /* valloc()ed memory can't be reclaimed by free() */
  in_iobuf = (void *)valloc(BUFFER_MAG * system_pagesize);
  out_iobuf = (void *)valloc(BUFFER_MAG * system_pagesize);
#endif
}

void cleanup_buffers() {
  /* free some global resources */
  free(textbuf);
#if defined HAVE_POSIX_MEMALIGN
  if( in_iobuf ) { free(in_iobuf); }
  if( out_iobuf ) { free(out_iobuf); }
#endif
}

void cleanup_tempfiles() {
  if( cleanup.tempfile ) { 
    unlink(cleanup.tempfile);
    cleanup.tempfile = NULL;
  }
}


void set_iobuf_mode(FILE *input) {
  struct stat statinfo;
  if( in_iobuf ) {
    /* choose appropriate buffering mode */
    if( fstat(fileno(input), &statinfo) == 0 ) {
      switch(statinfo.st_mode & S_IFMT) {
      case S_IFREG:
      case S_IFBLK:
	setvbuf(input, (char *)in_iobuf, _IOFBF, BUFFER_MAG * system_pagesize);
	break;
      case S_IFIFO:
      case S_IFCHR:
	setvbuf(input, (char *)NULL, 
		(u_options & (1<<U_OPTION_FILTER)) ? _IOLBF : _IOFBF, 
		BUFFER_MAG * system_pagesize);
	break;
      case S_IFDIR:
      default:
	/* nothing */
	break;
      }

    }
  }
}
/***********************************************************
 * TOKEN HASHING                                           *
 ***********************************************************/
hash_value_t hash_full_token(const char *tok) {
  const char *q;
  JENKINS_HASH_VALUE h;
  q = strchr(tok,EOTOKEN);
  if( q ) {
    h = hash((unsigned char *)tok, q - tok, 0);
    return (hash_value_t)hash((unsigned char *)q, EXTRA_CLASS_LEN, h);
  } else {
    errormsg(E_FATAL,
	    "hash_full_token called with missing class [%s]\n",
	     tok);
  }
  return (hash_value_t)0;
}

hash_value_t hash_partial_token(const char *tok, int len, const char *extra) {
  JENKINS_HASH_VALUE h;
  h = hash((unsigned char *)tok, len, 0);
  return (hash_value_t)hash((unsigned char *)extra, EXTRA_CLASS_LEN, h);
}

/***********************************************************
 * WEIGHT SIZE REDUCTION                                   *
 ***********************************************************/

digitized_weight_t digitize_a_weight(weight_t w, token_order_t order) {
  if( w < 0.0 ) {
    errormsg(E_FATAL,
	    "digitize_a_weight called with negative argument %f\n",
	    w);
/*     return DIGITIZED_WEIGHT_MIN; */
  } else if( order * w > (1<<(16 - DIG_FACTOR)) ) {
    return DIGITIZED_WEIGHT_MAX;
  } else {
    return (digitized_weight_t)(w * (order<<DIG_FACTOR));
  }
  return DIGITIZED_WEIGHT_MIN;
}

weight_t undigitize_a_weight(digitized_weight_t d, token_order_t order) {
  return ((weight_t)d) / (order<<DIG_FACTOR);
}

double nats2bits(double score) {
  return score/M_LN2;
}



/***********************************************************
 * SIGNAL HANDLING                                         *
 ***********************************************************/
#if defined HAVE_SIGACTION
/* sigaction structure used by several functions */
struct sigaction act;
#endif

void my_sa_handler(int signum) {
#if defined HAVE_SIGACTION
  sa_signal = signum;
#endif
}

void sigsegv(int signum) {
  fprintf(stdout, "%s:error: segmentation fault at input line %ld of %s\n", 
	  progname, inputline, inputfile);
  cleanup_tempfiles();
  exit(1);
}

/* intercepts typical termination signals and tries to do the right thing */
void init_signal_handling() {
#if defined HAVE_SIGACTION

  memset(&act, 0, sizeof(act));

  /* set up global sigaction structure */

  act.sa_handler = my_sa_handler;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask,SIGHUP);
  sigaddset(&act.sa_mask,SIGINT);
  sigaddset(&act.sa_mask,SIGQUIT);
  sigaddset(&act.sa_mask,SIGTERM);
  sigaddset(&act.sa_mask,SIGPIPE);
  sigaddset(&act.sa_mask,SIGUSR1);
  act.sa_flags = 0;

  sigaction(SIGHUP, &act, NULL);
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGQUIT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
  sigaction(SIGPIPE, &act, NULL);
  sigaction(SIGUSR1, &act, NULL);

  act.sa_handler = sigsegv;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask,SIGSEGV);
  act.sa_flags = 0;
  sigaction(SIGSEGV, &act, NULL);

#endif
}

void cleanup_signal_handling() {
  /* nothing - this is just to mess with your head ;-) */
}

void process_pending_signal(FILE *input) {
#if defined HAVE_SIGACTION
  
  if( sa_signal ) {

    sigprocmask(SIG_BLOCK, &act.sa_mask, NULL);
    switch(sa_signal) {
    case SIGINT:
      cleanup_tempfiles();
      fprintf(stderr, 
	      "%s:signal: caught interrupt request, exiting\n", progname);
      exit(1);
      break;
    case SIGPIPE:
      /* should we terminate, or should we ignore? */
      cleanup_tempfiles();
      fprintf(stderr,
	      "%s:error: broken pipe on output, exiting\n", progname);
      exit(1);
      break;
    case SIGHUP:
    case SIGQUIT:
    case SIGTERM:
      fprintf(stderr, 
	      "%s:signal: caught termination request, ignoring further input\n",
	      progname);
      if( input ) { fclose(input); }
      cmd |= (1<<CMD_QUITNOW);
      break;
    case SIGUSR1:
      if( u_options & (1<<U_OPTION_CLASSIFY) ) {
	fprintf(stderr, 
		"%s:signal: caught SIGUSR1 request, reloading categories asap\n",
		progname);
	cmd |= (1<<CMD_RELOAD_CATS);
      } else {
	fprintf(stderr, 
		"%s:signal: caught SIGUSR1 request, ignoring\n", progname);
      }
      break;
    default:
      /* nothing */
      break;
    }
    sa_signal = 0;

    sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL);

  }

#endif
}


/***********************************************************
 * ERROR DISPLAY                                           *
 ***********************************************************/
void errormsg(int etype, const char *fmt, ...) {
  va_list vap;

  switch(etype) {
  case E_WARNING:
    fprintf(stderr, "%s:warning: ", progname);
    break;
  default:
  case E_ERROR:
  case E_FATAL:
    fprintf(stderr, "%s:error: ", progname);
    break;
  }

#if HAVE_VPRINTF
  va_start(vap, fmt);
  vfprintf(stderr, fmt, vap);
  va_end(vap);
#else
  fprintf(stderr, "%s", fmt);
#endif

  if( etype == E_FATAL ) { 
    cleanup_tempfiles();
    exit(1);
  }
}

/***********************************************************
 * MULTIBYTE FILE HANDLING FUNCTIONS                       *
 * this is suitable for any locale whose character set     *
 * encoding doesn't include NUL bytes inside characters    *
 ***********************************************************/

void print_token(FILE *out, const char *tok) {
  while(*tok) {
    switch(*tok) {
    case DIAMOND:
      fprintf(out, "[]");
      break;
    case TOKENSEP:
      fprintf(out, " ");
      break;
    case CLASSEP:
      fprintf(out, "(%d)", tok[1] - AMIN);
      tok++;
      break;
    default:
      fprintf(out, "%c", *tok);
      break;
    }
    tok++;
  }
}

/* even after the EOF is reached, this pretends there are
 * a few more blank lines, to allow filters to process
 * cached input.
 */
bool_t fill_textbuf(FILE *input, int *extra_lines) {
  char *s;
  charbuf_len_t l, k;
  
  if( !(cmd & (1<<CMD_QUITNOW)) && !feof(input) ) {
    process_pending_signal(input);

    /* read in a full line, allocating memory as necessary */
    textbuf[0] = '\0';
    s = textbuf;
    l = textbuf_len;
    k = 1;
    while( fgets(s, l, input) && ((charbuf_len_t)strlen(s) >= (l - 1)) ) {
      textbuf = (char *)realloc(textbuf, 2 * textbuf_len);
      if( !textbuf ) {
	fprintf(stderr, 
		"error: not enough memory for input line (%d bytes)\n",
		textbuf_len);
	cleanup_tempfiles();
	exit(1);
      }

      s = textbuf + textbuf_len - (k++);
      l = textbuf_len;
      textbuf_len *= 2;

      MADVISE(textbuf, sizeof(char) * textbuf_len, MADV_SEQUENTIAL);

    }
    return 1;
  } else if( *extra_lines > 0 ) {
    strcpy(textbuf, "\r\n");
    *extra_lines = (*extra_lines) - 1;
    return 1;
  }
  return 0;
}

/***********************************************************
 * WIDE CHARACTER FILE HANDLING FUNCTIONS                  *
 * this is needed for any locale whose character set       *
 * encoding can include NUL bytes inside characters        *
 ***********************************************************/
#if defined HAVE_MBRTOWC

/* this does the same work as mbstowcs, but unlike the latter,
 * we continue converting even if an error is detected. That
 * is why we can't use the standard function.
 * Returns true if the converted line is nonempty.
 */
bool_t fill_wc_textbuf(char *pptextbuf, mbstate_t *shiftstate) {

  char *s;
  charbuf_len_t k,l;
  charbuf_len_t wclen;
  wchar_t *wp;

  if( !pptextbuf || !*pptextbuf ) { return 0; }

  if( textbuf_len > wc_textbuf_len ) {
    wc_textbuf_len = textbuf_len;
    wc_textbuf = (wchar_t *)realloc(wc_textbuf, wc_textbuf_len * sizeof(wchar_t));
    if( !wc_textbuf ) {
      fprintf(stderr, 
	      "error: not enough memory for wide character conversion "
	      "(%ld bytes)\n",
	      (long int)(wc_textbuf_len * sizeof(wchar_t)));
      cleanup_tempfiles();
      exit(1);
    }

    MADVISE(wc_textbuf, sizeof(wchar_t) * wc_textbuf_len, MADV_SEQUENTIAL);

  }

  /* convert as much as we can of the line into wide characters */
  s = pptextbuf;
  k = textbuf_len;
  wp = wc_textbuf;
  wclen = 0;
  /* since we ensured textbuf_len <= wctextbuf_len
     there will never be overflow of wctextbuf below */
  while( k > 0 ) {
    l = mbrtowc(wp, s, k, shiftstate);
    if( l > 0 ) {
      wp++;
      wclen++;
      k -= l;
      s += l;
    } else if( l == 0 ) {
      break;
    } else if( l == -1 ) {
      /* try to be robust */
      s++; 
      k--;
      memset(shiftstate, 0, sizeof(mbstate_t));
    } else if( l == -2) {
      /* couldn't parse a complete character */
      break;
    }
  }
  *wp = L'\0';

  return (wclen > 0);
}

#endif
