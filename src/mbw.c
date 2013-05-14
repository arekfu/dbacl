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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "dbacl.h"


/* most functions in this file are logically identical in wide char
 * and multibyte versions, except that char is replaced by wchar_t
 * etc. It's become unwieldy to keep two slightly different copies of
 * all functions involved, so we use the preprocessor to build a poor
 * man's template facility.
 * 
 * The "template" macros work as follows:
 * mbw_lit("abc")             -> "abc" or L"abc"
 * mbw_t                      -> char  or wchar_t
 * mbw_prefix(good_char)(x)   -> good_char(x) or w_good_char(x)
 *
 * Once the template macros have done their work, we obtain ordinary
 * functions named in a parallel fashion, where the wide character
 * functions have a w_ prefix, and instances of char are substituted
 * with instances of wchar_t.
 *
 * The code below is split into uncommon code, where the
 * implementations of corresponding functions is different, and common
 * code where the implementation is identical. Only identical code is
 * "templatized".
 */

#include "mbw.h"

extern options_t u_options;
extern charparser_t m_cp;
extern options_t m_options;

extern myregex_t re[MAX_RE];
extern regex_count_t regex_count;

extern long system_pagesize;

/* uncommon code */

/***********************************************************
 * UTILITY FUNCTIONS                                       *
 ***********************************************************/

#if defined HAVE_MBRTOWC && defined MBW_WIDE
/* compiler doesn't seem to know this function is in the 
 * library, so we define our own - bug or just plain weird? */
static __inline__
int mywcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n) {
  register size_t i = 0;
  while( i < n ) {
    if( tolower(*s1) != tolower(*s2) ) {
      return towlower(*s1) - towlower(*s2);
    }
    s1++;
    s2++;
  }
  return 0;
}
#endif


#if defined HAVE_MBRTOWC && defined MBW_WIDE

static __inline__
int w_b64_code(wchar_t c) {
  if( (c >= L'A') && (c <= L'Z') ) {
    return (c - L'A');
  } else if( (c >= L'a') && (c <= L'z') ) {
    return (c - L'a') + 26;
  } else if( (c >= L'0') && (c <= L'9') ) {
    return (c - L'0') + 52;
  } else if( c == L'+' ) { 
    return 62;
  } else if( c == L'/' ) {
    return 63;
  } else if( c == L'=' ) {
    return 64;
  } else {
    return -1;
  }
}

static __inline__
int w_qp_code(wchar_t c) {
  if( (c >= L'0') && (c <= L'9') ) {
    return (c - L'0');
  } else if( (c >= L'A') && (c <= L'F') ) {
    return (c - L'A') + 10;
  } else {
    return -1;
  }
}


#else 

/* warning: only use char here so we never have to bother about endianness */
static const signed char b64_code_table[256] = { 
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,62,-1,-1,-1,63,52,53,
  54,55,56,57,58,59,60,61,-1,-1,
  -1,64,-1,-1,-1, 0, 1, 2, 3, 4,
   5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,
  25,-1,-1,-1,-1,-1,-1,26,27,28,
  29,30,31,32,33,34,35,36,37,38,
  39,40,41,42,43,44,45,46,47,48,
  49,50,51,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1
};

#define b64_code(c) ((int)b64_code_table[(int)c])

static const signed char qp_code_table[256] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1, 0, 1,
   2, 3, 4, 5, 6, 7, 8, 9,-1,-1,
  -1,-1,-1,-1,-1,10,11,12,13,14,
  15,16,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1
};

#define qp_code(c) (qp_code_table[(int)c])

#endif


/* common code */

#if defined MBW_MB || (defined MBW_WIDE && defined HAVE_MBRTOWC)


/* this is dbacl's idea of an empty line. Note that a single \n or
 * a \r\n both constitute an empty line, contrary to RFC 2822, which
 * doesn't allow single \n chars in headers. However, we might be
 * reading the mail from a Unix mbox, where \r\n was replaced with \n.
 * We don't accept single \r however.
 */
#define MBW_EMPTYLINE(line) ((!(line) || \
                              (line[0] == mbw_lit('\0')) || \
                              ((line)[0] == mbw_lit('\n')) || \
			      (((line)[0] == mbw_lit('\r')) &&  \
			       ((line)[1] == mbw_lit('\n')))) ? 1 : 0)

#define MBW_DOUBLEDASH(line) ((line[0] == mbw_lit('-')) && \
                              (line[1] == mbw_lit('-')) && \
                              !mbw_isspace(line[2]))


/***********************************************************
 * TABLES                                                  *
 ***********************************************************/
typedef struct {
  const mbw_t *type_subtype;
  MIME_Content_Type medium;
} mbw_prefix(MIME_Media);

/* Wildcards such as text are represented as "text/" and must
 * be placed after all other text/xxx types. 
 * More generally, comparison uses mystrcasestr, so the smallest strings
 * must come after the more detailed ones.
 *
 * For a description of official mime types, see
 * http://www.iana.org/assignments/
 */
static const mbw_prefix(MIME_Media) mbw_prefix(mime_media)[] = {
  { mbw_lit("text/html"), ctTEXT_HTML },
  { mbw_lit("text/xhtml"), ctTEXT_HTML },
  { mbw_lit("text/plain"), ctTEXT_PLAIN },
  { mbw_lit("text/richtext"), ctTEXT_RICH },
  { mbw_lit("text/enriched"), ctTEXT_RICH },
  { mbw_lit("text/rtf"), ctTEXT_PLAIN },
  { mbw_lit("text/xml"), ctTEXT_XML },
  { mbw_lit("text/sgml"), ctTEXT_SGML },
  { mbw_lit("text/"), ctTEXT_PLAIN }, 

  { mbw_lit("multipart/"), ctTEXT_PLAIN },

  { mbw_lit("message/rfc822"), ctMESSAGE_RFC822 },
  { mbw_lit("message/partial"), ctOTHER },
  { mbw_lit("message/external-body"), ctMESSAGE_RFC822 },
  { mbw_lit("message/news"), ctMESSAGE_RFC822 },
  { mbw_lit("message/"), ctOCTET_STREAM },

  { mbw_lit("application/sgml"), ctTEXT_PLAIN },
  { mbw_lit("application/xml"), ctTEXT_PLAIN },
  { mbw_lit("application/rtf"), ctTEXT_PLAIN },
  { mbw_lit("application/news-transmission"), ctMESSAGE_RFC822 },
  { mbw_lit("application/andrew-inset"), ctTEXT_PLAIN },
  { mbw_lit("application/msword"), ctAPPLICATION_MSWORD },
  { mbw_lit("application/"), ctOCTET_STREAM },

  { mbw_lit("image/"), ctIMAGE },
  { mbw_lit("audio/"), ctAUDIO },
  { mbw_lit("video/"), ctVIDEO },
  { mbw_lit("model/"), ctMODEL },
};
static int num_mime_media = sizeof(mbw_prefix(mime_media))/sizeof(mbw_prefix(MIME_Media));

static const mbw_t *mbw_prefix(armor_start)[] = {
  mbw_lit("-----BEGIN PGP MESSAGE"),
  mbw_lit("-----BEGIN PGP PUBLIC KEY BLOCK"),
  mbw_lit("-----BEGIN PGP PRIVATE KEY BLOCK"),
  mbw_lit("-----BEGIN PGP SIGNATURE"),
};
static int num_armor_start = sizeof(mbw_prefix(armor_start))/sizeof(mbw_t *);

static const mbw_t *mbw_prefix(armor_end)[] = {
  mbw_lit("-----END PGP MESSAGE"),
  mbw_lit("-----END PGP PUBLIC KEY BLOCK"),
  mbw_lit("-----END PGP PRIVATE KEY BLOCK"),
  mbw_lit("-----END PGP SIGNATURE"),
};
static int num_armor_end = sizeof(mbw_prefix(armor_end))/sizeof(mbw_t *);

/***********************************************************
 * UTILITY FUNCTIONS                                       *
 ***********************************************************/

/* checks if the line is "binary", ie contains printable chars
   but not too many extended ascii chars */
static
bool_t mbw_prefix(is_binline)(const mbw_t *line) {
  int numa = 0;
  const mbw_t *p = line;
  while( *p ) {
    if( mbw_isspace(*p) || 
	(mbw_isascii(*p) && !mbw_iscntrl(*p)) ) {
      numa++;
    } else if( !mbw_isprint(*p) ) {
      return 1;
    }
    p++;
  }
  return (numa < (p - line)/2);
}

static
bool_t mbw_prefix(is_emptyspace)(const mbw_t *line) {
  const mbw_t *p = line;
  while( *p ) {
    if( !mbw_isspace(*p) ) {
      return 0;
    }
    p++;
  }
  return 1;
}


static
bool_t mbw_prefix(is_b64line)(const mbw_t *line) {
  const mbw_t *p = line;
  while( *p ) {
    if( (mbw_prefix(b64_code)(*p) == -1) &&
	!mbw_isspace(*p) ) {
      return 0;
    }
    p++;
  }
  return 1;
}

static
int mbw_prefix(is_uuline)(const mbw_t *line) {
  int count = 0;
  const mbw_t *p = line;
  int len = (int)(line[0] - mbw_lit(' '));
  if( (len < 0) || (len > 63) ) {
    return -1;
  }
  while(*p && (*p != mbw_lit('\r')) && (*p != mbw_lit('\n')) ) {
    if( (*p > mbw_lit('`')) || 
	(*p < mbw_lit(' ')) ) {
      return -2;
    } else {
      count++;
    }
    p++;
  }

  return (abs(count - 4*(len/3)) <= 3);
}

/* detecting true yEnc lines is too hard, so we detect
   nonprintable characters instead */
static
bool_t mbw_prefix(is_yencline)(const mbw_t *line) {
  int nonprint = 0;
  const mbw_t *p = line;
  while( *p ) {
    nonprint += !mbw_isprint(*p);
    p++;
  }
  return (nonprint > 5);
}

/* 
 * this code generates mystrcasestr() and w_mystrcasestr() 
 * (similar to strstr, but case insensitive)
 */
static __inline__
const mbw_t *mbw_prefix(mystrcasestr)(const mbw_t *haystack, const mbw_t *needle) {
  const mbw_t *p, *q, *r;

  for(p = haystack; *p; p++) {
    q = needle; r = p;
    while( *q && *r && ((mbw_tolower(*q) - mbw_tolower(*r)) == 0) ) {
      q++; r++;
    }
    if( !*q ) {
      return p;
    }
  }
  return NULL;
}

static __inline__
int mbw_prefix(mystrncasecmp)(const mbw_t *s1, const mbw_t *s2, size_t n) {
  int s = -1;
  if( s1 && s2 ) {
    while(--n > 0) {
      s = (mbw_tolower(*s1++) - mbw_tolower(*s2++));
      if( (s != 0) || (s1 == mbw_lit('\0')) || (s2 == mbw_lit('\0')) ) { 
	break; 
      }
    }
  }
  return s;
}

static __inline__
int mbw_prefix(mystrncmp)(const mbw_t *s1, const mbw_t *s2, size_t n) {
  int s = -1;
  if( s1 && s2 ) {
    while(--n > 0) {
      s = (*s1++ - *s2++);
      if( (s != 0) || (s1 == mbw_lit('\0')) || (s2 == mbw_lit('\0')) ) { 
	break; 
      }
    }
  }
  return s;
}


/***********************************************************
 * DECODING CACHE FUNCTIONS                                *
 ***********************************************************/


static
void mbw_prefix(init_dc)(mbw_prefix(decoding_cache) *dc, size_t len) {
  if( !dc->cache ) {
    dc->cache = (mbw_t *)malloc(len * sizeof(mbw_t));
    dc->data_ptr = dc->cache;
    dc->cache_len = dc->cache ? len : 0;
    dc->max_line_len = len; 
  }
}


static
void mbw_prefix(adjust_cache_size)(mbw_prefix(decoding_cache) *dc, size_t n) {
  mbw_t *p;
  size_t m = (dc->data_ptr - dc->cache);
  while( dc->cache_len < m + n ) {
    p = (mbw_t *)realloc(dc->cache, 2 * dc->cache_len * sizeof(mbw_t));
    if( p ) {
      dc->cache = p;
      dc->data_ptr = p + m;
      dc->cache_len *= 2;
    } else {
      break;
    }
  }
  dc->max_line_len = (dc->max_line_len < n) ? n : dc->max_line_len;
}

static
bool_t mbw_prefix(flush_cache)(mbw_prefix(decoding_cache) *dc, mbw_t *line, bool_t all) {
  mbw_t *q;
  mbw_t *p;
  int i;
  if( dc->cache && (dc->data_ptr > dc->cache) ) {
    /* never output more bytes than will fit on output_line */
    p = (dc->data_ptr > dc->cache + dc->max_line_len) ? 
      (dc->cache + dc->max_line_len): dc->data_ptr;

    if( !all ) {
      /* we break the line at the last space, or ampersand, or seventy
	 chars (> b64/qp limit) from the end - there may well be
	 stretches longer than this, but we try to flush as much as
	 possible, so the limit should be small. Also, we don't want
	 to break entities if possible.
      */	 
/*       for(i = 25; !mbw_isspace(*p) &&  */
/* 	    (p > dc->cache) && i; --p, --i); */
      for(i = 70; !mbw_isspace(*p) && 
	    (*p != mbw_lit('&')) && (p > dc->cache) && i; --p, --i);
    }

    if( p > dc->cache ) {
      for(q = dc->cache; q < p; q++) {
	*line++ = *q;
      }
      *line = mbw_lit('\0');

      dc->data_ptr = dc->cache;
      if( !all ) {
	/* now fold unused part back into cache. Note that
	 * b64_line_cache is always NUL terminated, so we don't
	 * need b64_cache_ptr to mark the end. */
	while( *p ) {
	  *dc->data_ptr++ = *p++;
	}
      }
      *dc->data_ptr = mbw_lit('\0');
      return 1;
    }
  }
  return 0;
}

/***********************************************************
 * DECODING FUNCTIONS                                      *
 ***********************************************************/


#define REPNUL mbw_lit('\t')

/* 
 * this code generates b64_line_filter2() and w_b64_line_filter2() 
 * works ok so long as q <= line, or q >> line 
 * WARNING: it is assumed that the buffer at q can hold (at most) all of line
 *
 * The string which is written is always NUL terminated, but if NULs 
 * were decoded in the middle, those are replaced by tabs (we could
 * also replace them with a more neutral char, but the cache flushing
 * code breaks up lines on spaces, and we want to take advantage of that. See
 * the REPNUL define)
 */
mbw_t *mbw_prefix(b64_line_filter2)(mbw_t *line, mbw_t *q) {
  mbw_t *p = line;
  mbw_t buf[4];
  mbw_t *buf_start = buf;
  mbw_t *buf_end = buf + 4;

  if( q ) {
    while( *p ) {
      if( mbw_prefix(b64_code)(*p) > -1 ) {
	*buf_start++ = *p;
	if( buf_start == buf_end ) {
	  buf_start = buf;
	  *q = (mbw_prefix(b64_code)(buf[0])<<2) + (mbw_prefix(b64_code)(buf[1])>>4);
	  if( !*q ) { *q = REPNUL; }
	  q++;
	  if( buf[2] != mbw_lit('=') ) {
	    *q = (mbw_prefix(b64_code)(buf[1])<<4) + (mbw_prefix(b64_code)(buf[2])>>2);
	    if( !*q ) { *q = REPNUL; }
	    q++;
	    if( buf[3] != mbw_lit('=') ) {
	      *q = (mbw_prefix(b64_code)(buf[2])<<6) + mbw_prefix(b64_code)(buf[3]);	    
	      if( !*q ) { *q = REPNUL; }
	      q++;
	    } else {
	      break;
	    }
	  } else {
	    break; 
	  }
	}
      }
      p++;
    }
    *q = mbw_lit('\0');
  }

  return q;
}

/* 
 * this code generates b64_line_filter() and w_b64_line_filter() 
 * Decodes a base64 encoded line. The input line is overwritten. 
 *
 * The b64 standard arbitrarily truncates lines to 57 characters, so
 * here we place the chunks in a cache and only overwrite line when
 * the cache is full. Unfortunately, malformed email messages may not 
 * follow the standard, so in practice all this means is that we get 
 * arbitrarily truncated input.
 *
 * Note that when we overwrite line with the cached data, we assume
 * the line is big enough to hold all the cached data. This is guaranteed
 * by registering the current line length with the cache.
 */
bool_t mbw_prefix(b64_line_filter)(mbw_prefix(decoding_cache) *b64cache, 
				   mbw_t *line) {
  mbw_prefix(adjust_cache_size)(b64cache, mbw_strlen(line));
  b64cache->data_ptr = 
    mbw_prefix(b64_line_filter2)(line, b64cache->data_ptr);
  return mbw_prefix(flush_cache)(b64cache, line, 0);
}


/* 
 * this code generates qp_line_filter2() and w_qp_line_filter2() 
 * this works ok so long as q <= line, or q >> line 
 * WARNING: it is assumed that the buffer at q can hold (at most) all of line
 */
mbw_t *mbw_prefix(qp_line_filter2)(mbw_t *line, mbw_t *q) {
  mbw_t *p = line;
  if( q ) {
    while( *p ) {
      if( *p != mbw_lit('=') ) {
	*q++ = *p++;
      } else {
	if( !*(++p) || mbw_isspace(*p) ) { 
	  break;
	} else {
 	  /* if the equal sign isn't followed by  */
          /* an upper case hex number, something's wrong */ 
	  *q = mbw_prefix(qp_code)(*p);
	  if( ((signed char)*q < 0) || !p[1] || (mbw_prefix(qp_code)(p[1]) < 0) ) {
	    *q++ = p[-1];
	  } else {
	    *q = (*q << 4) + mbw_prefix(qp_code)(p[1]); 
	    if( *q ) { q++; } 
	    p += 2;
	  }
	}
      }
    }
    *q = mbw_lit('\0');
  }
  return q;
}

/* 
 * this code generates qp_line_filter() and w_qp_line_filter() 
 * Decodes a quoted-printable line. The input line is overwritten. 
 *
 * The QP standard arbitrarily truncates lines to 76 characters, so
 * here we place the chunks in a cache and only overwrite line when
 * the cache is full. Unfortunately, malformed email messages may not 
 * follow the standard, so in practice all this means is that we get 
 * arbitrarily truncated input.
 *
 * Note that when we overwrite line with the cached data, we assume
 * the line is big enough to hold all the cached data. This is guaranteed
 * by registering the current line length with the cache.
 */
bool_t mbw_prefix(qp_line_filter)(mbw_prefix(decoding_cache) *qpcache, 
				  mbw_t *line) {
  mbw_prefix(adjust_cache_size)(qpcache, mbw_strlen(line));
  qpcache->data_ptr = 
    mbw_prefix(qp_line_filter2)(line, qpcache->data_ptr);
  return mbw_prefix(flush_cache)(qpcache, line, 0);
}


/***********************************************************
 * TOKENIZER FUNCTIONS                                     *
 ***********************************************************/
/* the following modules handle state transitions, you can mix and
 * match them, or write new ones. The is_func() is called in the
 * default state, and switches the internal state if necessary. If it
 * can't recognize the current char, it should return gcUNDEF, not
 * gcDISCARD, that way the next is_func() can look at the character. 
 * The handle_func() is similar to the is_func(), but is
 * called when the state is not the default. It should return gcDISCARD 
 * and switch to the default state if it can't recognize the current char,
 * otherwise it can switch states any way it wants. When it detects the
 * end of the current token, it must switch back to the default state.
 */

/* these are macros to save typing, modules below */

#define SET1(c) ( (*(c) == mbw_lit('\'')) || (*(c) == mbw_lit('-')) || (*(c) == mbw_lit('.')) )
/* #define SET1(c) ( (*(c) == mbw_lit('-')) || (*(c) == mbw_lit('+')) || (*(c) == mbw_lit('.')) || (*(c) == mbw_lit('_')) || (*(c) == mbw_lit(',')) || (*(c) >= 0xA0) ) */
/* #define SET1(c) ( (*(c) == mbw_lit('-')) || (*(c) == mbw_lit('+')) || (*(c) == mbw_lit('.')) || (*(c) == mbw_lit('_')) || (*(c) == mbw_lit(',')) || (*(c) == mbw_lit('$')) || (*(c) >= 0xA0) ) */
#define SET2(c) ( (*(c) == mbw_lit(',')) || (*(c) == mbw_lit('.')) )
#define SET3(c) ( (*(c) > mbw_lit(' ')) && (*(c) <= mbw_lit('~')) && (*(c) != mbw_lit('>')) )

#define IO(c) ((*(c) & 0xC0) == 0x80)
#define I2O(c) ((*(c) & 0xE0) == 0xC0)
#define I3O(c) ((*(c) & 0xF0) == 0xE0)
#define I4O(c) ((*(c) & 0xF8) == 0xF0)
#define I5O(c) ((*(c) & 0xFC) == 0xF8)
#define I6O(c) ((*(c) & 0xFE) == 0xFC)

#define RANGE(c,x,y) ((*(c) >= x) && (*(c) <= y))
#define DRANGE(c,x,y,u,v) (RANGE(c,x,y) || RANGE(c,u,v))
#define DTEST(s,t,r) (s && t && (char_filter_state = r))
#define TTEST(s,t,u,r) (s && t && u && (char_filter_state = r))
#define QTEST(s,t,u,v,r) (s && t && u && v && (char_filter_state = r))
#define VTEST(s,t,u,v,w,r) (s && t && u && v && w && (char_filter_state = r))
#define STEST(s,t,u,v,w,x,r) (s && t && u && v && w && x && (char_filter_state = r))

#define Shift_JIS(c) ( DTEST(DRANGE(c,0x81,0x9F,0xE0,0xFC),DRANGE(c+1,0x40,0x7E,0x80,0xFC),fShift_JIS_1) )

#define EUC_Japanese(c) ( DTEST(RANGE(c,0xA1,0xFE),RANGE(c+1,0xA1,0xFE),fEUC_Japanese_1) || DTEST((*c == 0x8E),RANGE(c+1,0xA0,0xDF),fEUC_Japanese_1) || TTEST((*c == 0x8F),RANGE(c+1,0xA1,0xFE),RANGE(c+2,0xA1,0xFE),fEUC_Japanese_2) )

#define BIG5(c) ( DTEST(RANGE(c,0xA1,0xFE),DRANGE(c+1,0x40,0x7E,0xA1,0xFE),fBIG5_1) )

#define BIG5P(c) ( DTEST(RANGE(c,0x81,0xFE),DRANGE(c+1,0x40,0x7E,0x80,0xFE),fBIG5P_1) )

#define EUC_CN(c) ( DTEST(RANGE(c,0xA1,0xFE),RANGE(c+1,0xA1,0xFE),fEUC_CN_1) )

#define EUC_TW(c) ( DTEST(RANGE(c,0xA1,0xFE),RANGE(c+1,0xA1,0xFE),fEUC_TW_1) || QTEST((*c == 0x8E),RANGE(c+1,0xA1,0xB0),RANGE(c+2,0xA1,0xFE),RANGE(c+3,0xA1,0xFE),fEUC_TW_3) )

#define Johab(c) ( DTEST(RANGE(c,0x84,0xD3),DRANGE(c+1,0x41,0x7E,0x81,0xFE),fJohab_1) || DTEST(DRANGE(c,0xD8,0xDE,0xE0,0xF9),DRANGE(c+1,0x31,0x7E,0x91,0xFE),fJohab_1) )

#define UTF8(c) ( DTEST(I2O(c),IO(c+1),fUTF8_1) || TTEST(I3O(c),IO(c+1),IO(c+2),fUTF8_2) ||  QTEST(I4O(c),IO(c+1),IO(c+2),IO(c+3),fUTF8_3) || VTEST(I5O(c),IO(c+1),IO(c+2),IO(c+3),IO(c+4),fUTF8_4) || STEST(I6O(c),IO(c+1),IO(c+2),IO(c+3),IO(c+4),IO(c+5),fUTF8_5) )

#define ISO8859(c) ( RANGE(c,0xA1,0xFE) )

/* atom without slash */
static char rfc2822_atom[256] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0, '!',   0, '#', '$', '%', '&',   0,   0,   0, '*', '+',   0, '-',   0,   0,
/*   0, '!',   0, '#', '$', '%', '&','\'',   0,   0, '*', '+',   0, '-',   0, '/', */
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',   0,   0,   0, '=',   0, '?',
  0, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',   0,   0,   0, '^', '_',
  '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
  'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~',  0,
};

#define ATOM(c) (((unsigned int)(*(c)) < 255) && rfc2822_atom[(unsigned int)(*(c))])
#define DOTTED_ATOM(c) ( ((c[0] == mbw_lit('.')) && ATOM((c+1))) || ATOM(c) )
#define COLON_ATOM(c) ( ((c[0] == mbw_lit(':')) && ATOM((c+1))) || ATOM(c) )
#define DOTTED_DIGITS(c) ( ((c[0] == mbw_lit('.')) && mbw_isdigit(c[1])) || mbw_isdigit(c[0]) )
#define COLON_DIGITS(c) ( ((c[0] == mbw_lit(':')) && mbw_isdigit(c[1])) || mbw_isdigit(c[0]) )
#define DOTTED_ALPHA(c) ( ((c[0] == mbw_lit('.')) && mbw_isalpha(c[1])) || mbw_isalpha(c[0]) )


/* warning: macros and modules work directly with this structure */
static enum {
  fDEF = 1,
  fANX,
  fNAX,
  fMUL,
  fCUR,
  fADD,
  fSEP_1, fSEP_2, fSEP_3,
  fUTF8_1, fUTF8_2, fUTF8_3, fUTF8_4, fUTF8_5, 
  fShift_JIS_1,
  fEUC_Japanese_1, fEUC_Japanese_2,
  fBIG5_1,
  fBIG5P_1,
  fEUC_CN_1,
  fEUC_TW_1,fEUC_TW_2,fEUC_TW_3,
  fJohab_1,
  fALNUM,
  fALPHA,
  fNUMERIC,
  fSYMBOL,
  fANSX_1, fANSX_2, fANSX_3,
  fCEF2_ATOM, fCEF2_DOTTED_ATOM, fCEF2_COLON_ATOM
} char_filter_state = fDEF;

/*
 * asian character tokens
 */

/* macro to be used in case statememt */
#define ASIAN_CASES fShift_JIS_1: case fBIG5_1: case fBIG5P_1: case fEUC_CN_1: case fJohab_1: case fEUC_TW_1: case fEUC_TW_2: case fEUC_TW_3: case fEUC_Japanese_1: case fEUC_Japanese_2

static __inline__
good_char_t mbw_prefix(is_asian_case)(const mbw_t *c) {
  return (Shift_JIS(c) || EUC_Japanese(c) || 
	  BIG5(c) || BIG5P(c) || EUC_CN(c) || EUC_TW(c) || Johab(c)) ? gcTOKEN : gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_asian_case)(const mbw_t *c) {
  switch(char_filter_state) {
  case fShift_JIS_1: 
  case fBIG5_1: 
  case fBIG5P_1: 
  case fEUC_CN_1: 
  case fJohab_1: 
    char_filter_state = fDEF; 
    return gcTOKEN_END; 
  case fEUC_TW_1: 
    char_filter_state = fDEF; 
    return gcTOKEN_END; 
  case fEUC_TW_2: 
    char_filter_state = fEUC_TW_1; 
    return gcTOKEN; 
  case fEUC_TW_3: 
    char_filter_state = fEUC_TW_2; 
    return gcTOKEN; 
  case fEUC_Japanese_1:
    char_filter_state = fDEF; 
    return gcTOKEN_END; 
  case fEUC_Japanese_2: 
    char_filter_state = fEUC_Japanese_1; 
    return gcTOKEN; 
  default:
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * utf8 character tokens - only makes sense if parsing multibyte strings
 */

/* macro to be used in case statememt */
#define UNICODE_CASES fUTF8_1: case fUTF8_2: case fUTF8_3: case fUTF8_4: case fUTF8_5

static __inline__
good_char_t mbw_prefix(is_unicode_case)(const mbw_t *c) {
  return (UTF8(c)) ? gcTOKEN : gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_unicode_case)(const mbw_t *c) {
  switch(char_filter_state) {
  case fUTF8_1: 
    char_filter_state = fDEF; 
    return gcTOKEN_END; 
  case fUTF8_2: 
    char_filter_state = fUTF8_1; 
    return gcTOKEN; 
  case fUTF8_3: 
    char_filter_state = fUTF8_2; 
    return gcTOKEN; 
  case fUTF8_4: 
    char_filter_state = fUTF8_3; 
    return gcTOKEN; 
  case fUTF8_5: 
    char_filter_state = fUTF8_4; 
    return gcTOKEN; 
  default:
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * alpha character tokens
 */

#define ALPHA_CASES fALPHA

/* checks for alpha and switches to alphabetic state, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_alpha_case)(const mbw_t *c) {
  if( mbw_isalpha(*c++) ) {
    if( mbw_isalpha(*c) || (*c == mbw_lit('\0')) ) {
      char_filter_state = fALPHA;
      return gcTOKEN;
    } 
    return gcTOKEN_END;
  }
  return gcUNDEF;
}

/* checks for alpha or discards, may switch back to default state */
static __inline__
good_char_t mbw_prefix(handle_alpha_case)(const mbw_t *c) {
  if( mbw_isalpha(*c++) ) {
    if( mbw_isalpha(*c) || (*c == mbw_lit('\0')) ) {
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * alphanumeric character tokens
 */

#define ALNUM_CASES fALNUM

/* checks for alnum and switches to alphanumeric state, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_alnum_case)(const mbw_t *c) {
  if( mbw_isalnum(*c++) ) {
    if( mbw_isalnum(*c) || (*c == mbw_lit('\0')) ) {
      char_filter_state = fALNUM;
      return gcTOKEN;
    } else {
      return gcTOKEN_END;
    }
  }
  return gcUNDEF;
}

/* checks for alnum or discards, may switch back to default state */
static __inline__
good_char_t mbw_prefix(handle_alnum_case)(const mbw_t *c) {
  if( mbw_isalnum(*c) ) {
    if( mbw_isalnum(*(++c)) || (*c == mbw_lit('\0')) ) {
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * numeric tokens
 */

#define NUMERIC_CASES fNUMERIC

/* checks for digit and switches to numeric state, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_numeric_case)(const mbw_t *c) {
  if( mbw_isdigit(*c++) ) {
    if( mbw_isdigit(*c) || (*c == mbw_lit('\0')) ) {
      char_filter_state = fNUMERIC;
      return gcTOKEN;
    } else {
      return gcTOKEN_END;
    }
  }
  return gcUNDEF;
}

/* checks for numeric or discards, may switch back to default state */
static __inline__
good_char_t mbw_prefix(handle_numeric_case)(const mbw_t *c) {
  if( mbw_isdigit(*c++) ) {
    if( mbw_isdigit(*c) || (*c == mbw_lit('\0')) ) {
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * punctuation tokens
 */

#define SYMBOLIC_CASES fSYMBOL

static __inline__
good_char_t mbw_prefix(is_symbolic_case)(const mbw_t *c) {
  if( mbw_ispunct(*c++) ) {
    if( mbw_ispunct(*c) || (*c == mbw_lit('\0')) ) {
      char_filter_state = fSYMBOL;
      return gcTOKEN;
    } else {
      return gcTOKEN_END;
    }
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_symbolic_case)(const mbw_t *c) {
  if( mbw_ispunct(*c++) ) {
    if( mbw_ispunct(*c) || (*c == mbw_lit('\0')) ) {
      if( (c[-1] == c[0]) && (c[0] == c[1]) ) {
	return gcIGNORE;
      }
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * repeated character tokens, squeezed down to 3.
 */

#define REPEAT_CASES fSEP_1: case fSEP_2: case fSEP_3

/* checks for repeated char, replaces with 3 copies only, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_repeat_case)(const mbw_t *c) {
  if( (c[1] == c[0]) ) {
    if( (c[2] == c[0]) ) {
      char_filter_state = fSEP_3;
      return gcTOKEN;
    } else {
      char_filter_state = fSEP_2;
      return gcTOKEN;
    }
  }
  return gcUNDEF;
}

/* checks for repeated char, replaces with 3 copies only */
static __inline__
good_char_t mbw_prefix(handle_repeat_case)(const mbw_t *c) {
  switch(char_filter_state) {
  case fSEP_1:
    if( c[0] != c[1] ) {
      char_filter_state = fDEF;
    }
    return gcDISCARD;
  case fSEP_2:
    char_filter_state = fSEP_1;
    return gcTOKEN;
  case fSEP_3:
    char_filter_state = fSEP_2;
    return gcTOKEN;
  default:
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * currency tokens, very simple and naive, not localized
 */

#define CURRENCY_CASES fCUR

/* checks for currency, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_currency_case)(const mbw_t *c) {
  /* this should be done properly (locale) sometime ... */
  if( (*c == mbw_lit('$')) || (*c == mbw_lit('\xa3')) ) {
    if( mbw_isdigit(c[1]) && 
	(!mbw_isdigit(c[2]) || !mbw_isdigit(c[3]) || !mbw_isdigit(c[4])) ) {
      char_filter_state = fCUR;
      return gcTOKEN;
    }
  }
  return gcUNDEF;
}

/* checks for currency */
static __inline__
good_char_t mbw_prefix(handle_currency_case)(const mbw_t *c) {
  if( mbw_isdigit(c[1]) ) {
    return gcTOKEN;
  } else if( SET2(c+1) && mbw_isdigit(c[2]) ) {
    char_filter_state = fCUR;
    return gcTOKEN;
  }
  char_filter_state = fDEF;
  return gcTOKEN_END;
}

/*
 * internet embedded address
 */

#define ADDRESS_CASES fADD

static __inline__
good_char_t mbw_prefix(is_address_case)(const mbw_t *c) {
  if( *c == mbw_lit('<') ) {
    for(c++; SET3(c); c++);
    if( *c == mbw_lit('>') ) {
      char_filter_state = fADD;
    }
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_address_case)(const mbw_t *c) {
  switch(*c) {
  case mbw_lit('@'):
    /*     case mbw_lit('#'): */
    /*     case mbw_lit('?'): */
    /*     case mbw_lit('&'): */
    /*     case mbw_lit(':'): */
    /*     case mbw_lit('/'): */
    return gcDISCARD;
  case mbw_lit('>'):
    char_filter_state = fDEF;
    return gcDISCARD;
  default:
    break;
  }
  return gcTOKEN;
}

/*
 * multiple alpha tokens separated by punctuation
 */

#define MULTI_ALPHA_CASES fMUL

/* checks for alpha and switches to alphabetic state, or
   returns gcUNDEF if unrecognized */
static __inline__
good_char_t mbw_prefix(is_multi_alpha_case)(const mbw_t *c) {
  /* don't increment c in SET1 */
  if( mbw_isalpha(*c++) && SET1(c) && mbw_isalpha(*(++c)) ) {
    char_filter_state = fMUL;
    return gcTOKEN;
  }
  return gcUNDEF;
}

/* checks for alpha or discards, may switch back to default state */
static __inline__
good_char_t mbw_prefix(handle_multi_alpha_case)(const mbw_t *c) {
  if( mbw_isalpha(c[1]) ) {
    return gcTOKEN;
  } else if( SET1(c+1) && mbw_isalpha(c[2]) ) {
    return gcTOKEN;
  }
  char_filter_state = fDEF;
  return gcTOKEN_END;
}

/*
 * xxx123 identifiers
 */

#define ALPHA_NUMBER_CASES fANX

static __inline__
good_char_t mbw_prefix(is_alpha_number_case)(const mbw_t *c) {
  if( mbw_isalpha(*c++) && mbw_isdigit(*c) ) {
    char_filter_state = fANX;
    return gcTOKEN;
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_alpha_number_case)(const mbw_t *c) {
  if( mbw_isdigit(*c++) ) {
    if( mbw_isdigit(*c) ) {
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * 123xxx identifiers
 */

#define NUMBER_ALPHA_CASES fNAX

static __inline__
good_char_t mbw_prefix(is_number_alpha_case)(const mbw_t *c) {
  if( mbw_isdigit(*c++) && mbw_isalpha(*c) ) {
    char_filter_state = fNAX;
    return gcTOKEN;
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_number_alpha_case)(const mbw_t *c) {
  if( mbw_isalpha(*c++) ) {
    if( mbw_isalpha(*c) ) {
      return gcTOKEN;
    }
    char_filter_state = fDEF;
    return gcTOKEN_END;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * xxx123% identifiers
 */

#define ALPHA_NUMBER_SYMBOL_CASES fANSX_1: case fANSX_2: case fANSX_3

static __inline__
good_char_t mbw_prefix(is_alpha_number_symbol_case)(const mbw_t *c) {
  if( mbw_isalpha(*c++) ) {
    if( mbw_isdigit(*c) ) {
      char_filter_state = fANSX_2;
      return gcTOKEN;
    } else if( mbw_ispunct(*c) ) {
      char_filter_state = fANSX_1;
      return gcTOKEN;
    } else if( !mbw_isalpha(*c) || (*c == mbw_lit('\0')) ) {
      return gcTOKEN_END;
    }
    char_filter_state = fANSX_3;
    return gcTOKEN;
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(is_number_symbol_case)(const mbw_t *c) {
  if( mbw_isdigit(*c++) ) {
    if( mbw_ispunct(*c) ) {
      char_filter_state = fANSX_1;
      return gcTOKEN;
    } else if( !mbw_isdigit(*c) || (*c == mbw_lit('\0')) ) {
      return gcTOKEN_END;
    }
    char_filter_state = fANSX_2;
    return gcTOKEN;
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_alpha_number_symbol_case)(const mbw_t *c) {
  if( *(c++) == mbw_lit('\0') ) {
    return gcTOKEN;
  }
  switch(char_filter_state) {
  case fANSX_1:
    if( !mbw_ispunct(*c) ) {
      char_filter_state = fDEF;
      return gcTOKEN_END;
    } else if( (c[-1] == c[0]) && (c[0] == c[1]) ) {
      return gcIGNORE;
    }
    return gcTOKEN;
  case fANSX_2:
    if( mbw_ispunct(*c) ) {
      char_filter_state = fANSX_1;
      return gcTOKEN;
    } else if( mbw_isalpha(*c) || ISO8859(c) ) {
      char_filter_state = fANSX_3;
      return gcTOKEN;
    } else if( !mbw_isdigit(*c) ) {
      char_filter_state = fDEF;
      return gcTOKEN_END;
    }
    return gcTOKEN;
  case fANSX_3:
    if( mbw_isdigit(*c) ) {
      char_filter_state = fANSX_2;
      return gcTOKEN;
    } else if( mbw_ispunct(*c) ) {
      char_filter_state = fANSX_1;
      return gcTOKEN;
    } else if( !mbw_isalpha(*c) && !ISO8859(c) ) {
      char_filter_state = fDEF;
      return gcTOKEN_END;
    } else if( (c[-1] == c[0]) && (c[0] == c[1]) ) {
      return gcIGNORE;
    }    
    return gcTOKEN;
  default:
    /* ignore */
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}


/*
 * This is the CEF (common encoding formats) tokenizer. 
 * It was the first attempt at a specialized email tokenizer.
 */

static __inline__
good_char_t mbw_prefix(is_cef_char)(const mbw_t *c) {
  good_char_t retval;
  switch(char_filter_state) {
  case fDEF:
    
#if defined MBW_MB
    /* this doesn't make sense for wide characters */
    if(*c & 0x80) {
      if( mbw_prefix(is_unicode_case)(c) || 
	  mbw_prefix(is_asian_case)(c) ) {
	return gcTOKEN;
      } else if( *c < 0xa0 ) {
	return gcDISCARD;
      }  
    }
#endif

    if( mbw_isalpha(*c) ) {
      if( (retval = mbw_prefix(is_alpha_number_case)(c)) ||
	  (retval = mbw_prefix(is_multi_alpha_case)(c)) ) {
	return retval;
      } 
      return gcTOKEN;
    } else if( mbw_ispunct(*c) ) {
      if( (retval = mbw_prefix(is_repeat_case)(c)) ||
	  (retval = mbw_prefix(is_currency_case)(c)) ||
	  (retval = mbw_prefix(is_address_case)(c)) ) {
	return retval;
      }
    } else if( mbw_isdigit(*c) ) {
      if( (retval = mbw_prefix(is_number_alpha_case)(c)) ) {
	return retval;
      }
    }
    return gcDISCARD;
  case ALPHA_CASES:
    retval = mbw_prefix(handle_alpha_case)(c);
    if( retval == gcTOKEN_END ) {
      if( (retval = mbw_prefix(is_multi_alpha_case)(c)) ||
	  (retval = mbw_prefix(is_alpha_number_case)(c)) ) {
	return retval;
      }
    }
    return retval;
  case ALPHA_NUMBER_CASES:
    return mbw_prefix(handle_alpha_number_case)(c);
  case NUMBER_ALPHA_CASES:
    return mbw_prefix(handle_number_alpha_case)(c);
  case MULTI_ALPHA_CASES:
    return mbw_prefix(handle_multi_alpha_case)(c);
  case ADDRESS_CASES:
    return mbw_prefix(handle_address_case)(c);
  case CURRENCY_CASES:
    return mbw_prefix(handle_currency_case)(c);
  case REPEAT_CASES:
    return mbw_prefix(handle_repeat_case)(c);
  case ASIAN_CASES:
    return mbw_prefix(handle_asian_case)(c);
  case UNICODE_CASES:
    return mbw_prefix(handle_unicode_case)(c);
  default:
    /* nothing */
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD; /* otherwise compiler complains */
}


/*
 * This is the ADP (alpha digit punctuation) tokenizer. 
 * It was the second attempt at a specialized email tokenizer.
 */

static __inline__
good_char_t mbw_prefix(is_adp_char)(const mbw_t *c) {
  good_char_t retval;
  switch(char_filter_state) {
  case fDEF:
#if defined MBW_MB
    /* this doesn't make sense for wide characters */
    if(*c & 0x80) {
      if( (retval = mbw_prefix(is_unicode_case)(c)) || 
	  (retval = mbw_prefix(is_asian_case)(c)) ) {
	return retval;
      } else if( *c < 0xa0 ) {
	return gcDISCARD;
      }  
    }
#endif
    if( (retval = mbw_prefix(is_alpha_number_symbol_case)(c)) || 
	(retval = mbw_prefix(is_number_symbol_case)(c)) || 
	(retval = mbw_prefix(is_symbolic_case)(c)) ) {
      return retval;
    }
    return gcDISCARD;

  case ALPHA_NUMBER_SYMBOL_CASES:
    return mbw_prefix(handle_alpha_number_symbol_case)(c);
  case SYMBOLIC_CASES:
    return mbw_prefix(handle_symbolic_case)(c);
  case ASIAN_CASES:
    return mbw_prefix(handle_asian_case)(c);
  case UNICODE_CASES:
    return mbw_prefix(handle_unicode_case)(c);
  default:
    /* nothing */
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

/*
 * This is the CEF2 (common email format v2) tokenizer. 
 */

static __inline__
good_char_t mbw_prefix(is_cef2_special_case)(const mbw_t *c) {
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(is_cef2_atom_case)(const mbw_t *c) {
  if( ATOM(c) ) {
    char_filter_state = fCEF2_ATOM;
    return gcTOKEN;
  }
  return gcUNDEF;
}

static __inline__
good_char_t mbw_prefix(handle_cef2_atom_case)(const mbw_t *c) {
  if( ATOM(c) ) {
    return gcTOKEN;
  } else if( DOTTED_ATOM(c) ) {
    char_filter_state = fCEF2_DOTTED_ATOM;
    return gcTOKEN;
  } else if( COLON_ATOM(c) ) {
    char_filter_state = fCEF2_COLON_ATOM;
    return gcTOKEN;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

static __inline__
good_char_t mbw_prefix(handle_cef2_dotted_atom_case)(const mbw_t *c) {
  if( DOTTED_ATOM(c) ) {
    return gcTOKEN;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

static __inline__
good_char_t mbw_prefix(handle_cef2_colon_atom_case)(const mbw_t *c) {
  if( COLON_ATOM(c) ) {
    return gcTOKEN;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}

static __inline__
good_char_t mbw_prefix(is_cef2_char)(const mbw_t *c) {
  good_char_t retval;
  switch(char_filter_state) {
  case fDEF:
#if defined MBW_MB
    /* this doesn't make sense for wide characters */
    if(*c & 0x80) {
      if( (retval = mbw_prefix(is_unicode_case)(c)) || 
	  (retval = mbw_prefix(is_asian_case)(c)) ) {
	return retval;
      } else if( *c < 0xa0 ) {
	return gcDISCARD;
      }  
    }
#endif
    if( (retval = mbw_prefix(is_cef2_special_case)(c)) ||
	(retval = mbw_prefix(is_cef2_atom_case)(c)) ) {
      return retval;
    }
    return gcDISCARD;
  case fCEF2_ATOM:
    return mbw_prefix(handle_cef2_atom_case)(c);
  case fCEF2_DOTTED_ATOM:
    return mbw_prefix(handle_cef2_dotted_atom_case)(c);
  case fCEF2_COLON_ATOM:
    return mbw_prefix(handle_cef2_colon_atom_case)(c);
  case ASIAN_CASES:
    return mbw_prefix(handle_asian_case)(c);
  case UNICODE_CASES:
    return mbw_prefix(handle_unicode_case)(c);
  default:
    /* nothing */
    break;
  }
  char_filter_state = fDEF;
  return gcDISCARD;
}



static __inline__
good_char_t mbw_prefix(is_char_char)(const mbw_t *c) {
/*   return (mbw_isgraph(*c) ? gcTOKEN_END :  */
/* 	  (mbw_isspace(*c) ? (mbw_isspace(c[1]) ? gcDISCARD : gcTOKEN_END) :  */
/* 	   gcDISCARD)); */
  return mbw_isgraph(*c) ? gcTOKEN_END : gcDISCARD;
}

/* 
 * this code generates good_char() and w_good_char() 
 * returns true if the character is part of a token 
 * 
 * gcTOKEN: character should be part of a token
 * gcTOKEN_END: like gcTOKEN, but token must end immediately
 * gcDISCARD: character is not part of a token
 * gcIGNORE: pretend there is no character here
 *
 * gcDISCARD is also returned if the line is empty
 */
good_char_t mbw_prefix(good_char)(mbw_t *c) {
  if( c && (*c != mbw_lit('\0')) ) {
    if( !(m_options & (1<<M_OPTION_CASEN)) ) {
      *c = mbw_tolower(*c);
    }
    switch(m_cp) {
    case CP_ADP:
      return mbw_prefix(is_adp_char)(c);
    case CP_CEF2:
      return mbw_prefix(is_cef2_char)(c);
    case CP_CHAR:
      return mbw_prefix(is_char_char)(c);
    case CP_ALPHA:
      return mbw_isalpha(*c) ? gcTOKEN : gcDISCARD;
    case CP_CEF:
      return mbw_prefix(is_cef_char)(c);
    case CP_ALNUM:
      return mbw_isalnum(*c) ? gcTOKEN : gcDISCARD;
    case CP_GRAPH:
      return mbw_isgraph(*c) ? gcTOKEN : gcDISCARD;
    case CP_DEFAULT:
      break;
    }
  }
  return gcDISCARD;
}

/*
 * The regex tokenizer operates on single lines only, ie regexes cannot
 * straddle lines. This makes the code much simpler.
 */
void mbw_prefix(regex_tokenizer)(mbw_t *p, int i,
				 void (*word_fun)(char *, token_type_t, regex_count_t),
				 token_type_t (*get_tt)(token_order_t)) {
  char *q, *cq;
  charbuf_len_t k,l, j;
  int eflag = 0;
  token_type_t tt;
  token_order_t z, order;
  char tok[(MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN];
  regmatch_t pmatch[MAX_SUBMATCH];

  k = 0;
  l = mbw_strlen(p);
  /* see if a match */
  while( (k < l) && (mbw_regexec(&re[i].regex, p + k, 
				 MAX_SUBMATCH, pmatch, eflag) == 0) ) {
    /* all the submatches (delimited by brackets in the regex) 
       get concatenated and the result gets word_fun'd */
    q = tok;
    *q++ = DIAMOND;
    for(order = 0, z = 1; 
	(z < MAX_SUBMATCH) && (pmatch[z].rm_so > -1); z++) {
      if( !(re[i].submatches & (1<<z)) ) 
	{ continue; } else { order++; }
      /* transcribe the submatch into tok */
      for(j = pmatch[z].rm_so; 
	  ((j < (charbuf_len_t)pmatch[z].rm_eo) && 
	   (j < (charbuf_len_t)pmatch[z].rm_so + MAX_TOKEN_LEN)); j++) {
	if( m_options & (1<<M_OPTION_CASEN) ) {
	  mbw_copychar(q,p[k + j]);
	} else {
	  mbw_copychar(q,mbw_tolower(p[k+j]));
	}
      }
      *q++ = DIAMOND;
    }

    tt = (*get_tt)(order);

    cq = q;
    *cq++ = CLASSEP;
    *cq++ = (char)(AMIN + tt.cls);
    *cq = '\0'; 

    /* now let each category process the token */
    (*word_fun)(tok, tt, i + 1); /* +1 because i = 0 means INVALID_RE */

    k += pmatch[0].rm_so + 1; /* advance string and repeat */
    eflag = REG_NOTBOL;
  }	      
}

/*
 * The standard tokenizer converts each acceptable token into a char
 * string, and passes it to word_fun().  To construct a token, the
 * good_char() function is called on each successive input character,
 * obtaining a code as follows: 
 *
 * gcTOKEN: The character belongs to an acceptable token, and is
 * copied to the holding buffer at the position pointed by nq, unless
 * the token length would exceed MAX_TOKEN_LEN, in which case we
 * pretend we have a gcTOKEN_END code.  
 *
 * gcTOKEN_END: The character belongs to an acceptable token, but the
 * token must be terminated immediately. In this case, we fall through
 * to the gcDISCARD case.  
 *
 * gcDISCARD: The character does not belong to a token. In this case,
 * we check that the holding buffer contains usable data, to which we
 * apply the word_fun(). The holding buffer is then reset.
 *
 * If p is NULL, we simply flush the token. Tokens normally straddle
 * newlines, but if M_OPTION_NGRAM_STRADDLE_NL is set, then each 
 * newline is flushes the current token also.
 */
void mbw_prefix(std_tokenizer)(mbw_t *p, char **pq, char *hbuf, 
			       token_order_t *hbuf_order, token_order_t max_order,
			       void (*word_fun)(char *, token_type_t, regex_count_t),
			       token_type_t (*get_tt)(token_order_t)) {
  token_type_t tt;
  token_order_t n, o;
  char *q;
  char *tstart, *qq, *cq;
  bool_t reset;

  if( p && (p[0] == mbw_lit('\0')) ) { 
    /* waste of time */
    return; 
  }

  q = *pq;
  o = *hbuf_order;

  if( !q || 
      (q < hbuf) || 
      (q > hbuf + (MAX_TOKEN_LEN+1)*MAX_SUBMATCH+EXTRA_TOKEN_LEN) ) {
    q = hbuf;
    *q++ = DIAMOND;
#if defined MBW_WIDE
    memset(&copychar_shiftstate, 0, sizeof(mbstate_t));
#endif
  }
  for(tstart = q - 1; *tstart != DIAMOND; --tstart);

  /* p[0] at least is nonzero */
  do {
    switch( mbw_prefix(good_char)(p) ) {
    case gcIGNORE:
      /* pretend there is no character here */
      break;
    case gcTOKEN:
      if( p && q < tstart + MAX_TOKEN_LEN ) {
	mbw_copychar(q,*p);
	break;
      }
      /* if we're here, fall through */
    case gcTOKEN_END:
      if( p && q < tstart + MAX_TOKEN_LEN + 1) {
	mbw_copychar(q,*p);
      }
      /* don't break, always fall through */
    case gcUNDEF:
    case gcDISCARD:
      reset = ( !(m_options & (1<<M_OPTION_NGRAM_STRADDLE_NL)) &&
		p && ( (p[0] == mbw_lit('\0')) || 
		       (p[0] == mbw_lit('\n')) ) ); 

      if( (p == NULL) || reset || (q[-1] != DIAMOND) ) {
	tstart = q;
	*q++ = DIAMOND;
	*q = '\0';

	if( max_order == 1 ) {
	  tt = (*get_tt)(1);
	  cq = q;
	  *cq++ = CLASSEP;
	  *cq++ = (char)(AMIN + tt.cls);
	  *cq = '\0';
	  /* let each category process the token */
	  (*word_fun)(hbuf, tt, INVALID_RE); 
	  tstart = q = hbuf;
	  *q++ = DIAMOND;
	} else if( p ) {
	  /* do this only if we have a line to work with */
	  if( ++o > max_order ) {
	    o--;
	    /* move all tokens down by one */
	    for(q = hbuf + 1; *q != DIAMOND; q++) {};
	    for(q++, qq = hbuf + 1; *q; *qq++ = *q++) {};
	    *qq = '\0';
	    tstart = q = qq;
	  }

	  tt = (*get_tt)(o);

	  cq = q;
	  *cq++ = CLASSEP;
	  *cq++ = (char)(AMIN + tt.cls);
	  *cq = '\0';
		
	  qq = hbuf;
	  for(n = o; n > 0; n--) {
	    /* let each category process the token */
	    tt.order = n;
	    (*word_fun)(qq, tt, INVALID_RE); 
	    qq++;
	    /* skip to next token and repeat */
	    while(*qq != DIAMOND ) { qq++; }
	  }
	}
	if( reset ) {
	  /* reset the current ngrams to zero */
	  tstart = hbuf;
	  q = hbuf + 1;
	  o = 0;
	}
      }

    }
  } while( p && (*(p++) != mbw_lit('\0')) ); 

  *pq = q;
  *hbuf_order = o;
}



/***********************************************************
 * FILTERING FUNCTIONS                                     *
 ***********************************************************/

/* 
 * this code generates mhe_line_filter() and w_mhe_line_filter() 
 * translates a MIME message header extension encoded
 * token into its equivalent byte sequence. 
 */
bool_t mbw_prefix(mhe_line_filter)(mbw_t *line) {
  mbw_t *p = line;
  mbw_t *q = line;
  mbw_t *r;

  while( *p ) {
    if( (p[0] == mbw_lit('=')) && (p[1] == mbw_lit('?')) ) {
      r = p + 2;
      while( *r && (*r != mbw_lit('?'))) { r++; }
      r++;
      /* I think lower case is illegal */
      if( (*r == mbw_lit('Q')) || (*r == mbw_lit('q')) ) { 
	if( *(++r) == mbw_lit('?') ) { 
	  r++;
	  /* we are now committed. find end marker and replace with NUL */
	  for(p = r; *p; p++) { 
	    if( *p == mbw_lit('_') ) {
	      *p = ' ';
	    } else if( (p[0] == mbw_lit('?')) && (p[1] == mbw_lit('=')) ) {
	      *p = mbw_lit('\0');
	      break;
	    }
	  }
	  q = mbw_prefix(qp_line_filter2)(r, q);
	  p += 2;
	} else {
	  /* malformed encoding */
	  *q++ = *p++;
	}
      } else if( (*r == mbw_lit('B')) || (*r == mbw_lit('b')) ) { 
	/* I think lower case is illegal, but we're lenient */
	if( *(++r) == mbw_lit('?') ) { 
	  r++;
	  /* we are now committed. find end marker and replace with NUL */
	  for(p = r; *p; p++) { 
	    if( (p[0] == mbw_lit('?')) && (p[1] == mbw_lit('=')) ) {
	      *p = mbw_lit('\0');
	      break;
	    }
	  }
	  q = mbw_prefix(b64_line_filter2)(r, q);
	  p += 2;
	} else {
	  /* malformed encoding */
	  *q++ = *p++;
	}
      } else {
	/* malformed encoding */
	*q++ = *p++;
      }
    } else {
      *q++ = *p++;      
    }
  }
  *q = '\0';
  return 1;
}

int mbw_prefix(extract_header_label)(MBOX_State *mbox, mbw_t *line) {
  mbw_t *p = line;

  if( m_options & (1<<M_OPTION_XHEADERS) ) {
    if( (mbw_strncasecmp(p, mbw_lit("X-DBACL"),7) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Date:"),4) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Path:"),4) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Posted:"),6) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Expires:"),7) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Received:"),8) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Resent-Date:"),11) == 0) ||
	(mbw_strncasecmp(p, mbw_lit("Delivery-Date:"),13) == 0) ||
	(mbw_isspace(line[0]) && mbox->skip_header) ) {
      mbox->skip_header = 1;
      return 0;
    } else {
      mbox->skip_header = 0;
      return mbw_prefix(mhe_line_filter)(line);
    }
  }

  return 0;
}

/***********************************************************
 * MBOX PARSING FUNCTIONS                                  *
 ***********************************************************/

/* 
 * this code generates extract_mime_boundary() and w_extract_mime_boundary() 
 * retrieves the MIME boundary if one is found. Doesn't cope with rfc2184 
 */
bool_t mbw_prefix(extract_mime_boundary)(MBOX_State *mbox,mbw_t *line) {
  const mbw_t *q;
  mbw_t *r;
  int size;
  bool_t quoted = 0; /* used both in calsulation and for return value */

  q = mbw_prefix(mystrcasestr)(line, mbw_lit("boundary="));

  if( q ) {
    /* we skip white space after = sign, even though it is not allowed */
    for(q += 9; mbw_isspace(*q); q++);
    if( *q ) {
      quoted = (*q == mbw_lit('"'));      
      r = mbox->boundary.mbw_prefix(identifier)[mbox->boundary.index];
      size = 0;
      if( quoted ) { 
	for(q++; *q && (*q != mbw_lit('"')) && 
	      (size < MAX_BOUNDARY_BUFSIZE); q++, size++) {
	  *r++ = *q;
	}
      } else {
	for(; *q && !mbw_isspace(*q) && 
	      (size < MAX_BOUNDARY_BUFSIZE); q++, size++) {
	  *r++ = *q;
	}
      }
      mbox->boundary.size[mbox->boundary.index] = size;
      if( ++mbox->boundary.index >= MAX_BOUNDARIES ) { mbox->boundary.index = 0; }
      quoted = (size > 0) ? 1 : 0;
    } else {
      /* this is bad */
      quoted = 0;
    }
  }

  /* MIME messages look like this: head-preamble-sec1-...-secN-postamble,
   * and the RFCs recommend that preambles/postambles be ignored. 
   * However, this introduces a loophole for spammers, who can define a 
   * boundary, but then never cite it. "Robust" MUAs will show the 
   * contents of the preamble, but we would not see it. To ignore preambles,
   * define the symbol IGNORE_MIME_PREAMBLE below.
   */
#undef IGNORE_MIME_PREAMBLE

#if defined(IGNORE_MIME_PREAMBLE)
  return quoted;
#else
  return 0;
#endif
}


static
bool_t mbw_prefix(check_old_style_digest)(const mbw_t *line) {

#define THIRTYDASHES mbw_lit("------------------------------")
#define SEVENTYDASHES mbw_lit("----------------------------------------------------------------------")

  /* messages are separated by either exactly 30 or exactly 70 dashes */
  return ( ((mbw_strncmp(line, THIRTYDASHES, 30) == 0) && 
	    (line[30] == mbw_lit('\r') || line[30] == mbw_lit('\n'))) ||
	   ((mbw_strncmp(line, SEVENTYDASHES, 70) == 0) &&
	    (line[70] == mbw_lit('\r') || line[70] == mbw_lit('\n'))) );
}

/* static */
/* bool_t mbw_prefix(outlook_message_announce)(const mbw_t *line) { */
/* #define OLDASHES mbw_lit("-----Original Message-----") */
/* #define OEDASHES mbw_lit("----- Original Message -----") */
/*   return ( ((mbw_strncmp(line, OLDASHES, 26) == 0) && */
/* 	    (line[26] == mbw_lit('\r') || line[26] == mbw_lit('\n'))) || */
/* 	   ((mbw_strncmp(line, OEDASHES, 28) == 0) && */
/* 	    (line[28] == mbw_lit('\r') || line[28] == mbw_lit('\n'))) ); */
/* } */

/* 
 * this code generates check_mime_boundary() and w_check_mime_boundary() 
 * The check is only approximate.
 */
bool_t mbw_prefix(check_mime_boundary)(MBOX_State *mbox, const mbw_t *line) {
  int c = (mbox->boundary.index > 0) ? 
    (mbox->boundary.index - 1) : (MAX_BOUNDARIES - 1);
  int k = 0;
  const mbw_t *p = mbox->boundary.mbw_prefix(identifier)[c];
  const mbw_t *q = line + 2; 
  while(*q) {
    if( (k >= mbox->boundary.size[c]) || (*q != *p) ) {
      c--;
      if( c < 0 ) { c = MAX_BOUNDARIES - 1; }
      p = &mbox->boundary.mbw_prefix(identifier)[c][k];
      if( c == mbox->boundary.index ) {
	if( (k >= mbox->boundary.size[c]) || (*q != *p) ) {
	  mbox->boundary.was_end = 0;
	  return 0;
	}
      }
    } else if( k == mbox->boundary.size[c] - 1) {
      if((q[1] == mbw_lit('-')) && (q[2] == mbw_lit('-'))) {
	mbox->boundary.was_end = 1;
      } else {
	mbox->boundary.was_end = 0;
      }
      return 1;
    } else {
      /* normally, a space isn't allowed in the boundary, but we're lenient */ 
      q++; 
      p++;
      k++;
    }
  }
  mbox->boundary.was_end = 0;
  return 0;
}

static
bool_t mbw_prefix(check_armor_start)(const mbw_t *line) {
  int i;
  for(i = 0; i < num_armor_start; i++) {
    if( mbw_strncmp(line, mbw_prefix(armor_start)[i],
		    mbw_strlen(mbw_prefix(armor_start)[i])) == 0 ) {
      return 1;
    }
  }
  if( (mbw_strncmp(line, mbw_lit("begin "), 6) == 0) &&
      ISOCT(line[6]) && ISOCT(line[7]) && ISOCT(line[8]) ) {
    /* uuencoded */
    return 1;
  } else if( (mbw_strncmp(line, mbw_lit("=ybegin"), 7) == 0) &&
	     ((line[7] == mbw_lit(' ')) || (line[7] == mbw_lit('2'))) &&
	     mbw_prefix(mystrcasestr)(line + 8, mbw_lit("line=")) &&
	     mbw_prefix(mystrcasestr)(line + 8, mbw_lit("size=")) &&
	     mbw_prefix(mystrcasestr)(line + 8, mbw_lit("name=")) ) {
    /* yEnc */
    return 1;
  }
  return 0;
}

static
bool_t mbw_prefix(check_armor_end)(const mbw_t *line) {
  int i;
  for(i = 0; i < num_armor_end; i++) {
    if( mbw_strncmp(line, mbw_prefix(armor_end)[i],
		    mbw_strlen(mbw_prefix(armor_end)[i])) == 0 ) {
      return 1;
    }
  }
  if( (mbw_strncmp(line, mbw_lit("end"),3) == 0) &&
      (!line[3] || (line[3] == mbw_lit('\n')) || (line[3] == mbw_lit('\r'))) ) {
    /* uuencoded */
    return 1;
  } else if( (mbw_strncmp(line, mbw_lit("=yend "), 6) == 0) &&
	     mbw_prefix(mystrcasestr)(line + 8, mbw_lit("size=")) ) {
    /* yEnc */
    return 1;
  }
  return 0;
}

/* return true if line should be shown, false otherwise */
static
bool_t mbw_prefix(armor_filter)(const mbw_t *line) {
  if( (mbw_prefix(is_b64line)(line) == 1) ||
      (mbw_prefix(is_uuline)(line) == 1) ||
      (mbw_prefix(is_yencline)(line) == 1) ) {
    return 0;
  }
  return 1;
}

/* 
 * this code generates extract_mime_types() and w_extract_mime_types() 
 */
static
void mbw_prefix(extract_mime_types)(mbw_t *line, MIME_Struct *ms) {
  int i;
  if( !mbw_strncasecmp(line, mbw_lit("Content-Type:"), 13) ) {
    line += 13;
    for(i = 0; i < num_mime_media; i++) {
      if( mbw_prefix(mystrcasestr)(line, 
				   mbw_prefix(mime_media)[i].type_subtype) ) {
	ms->type = mbw_prefix(mime_media)[i].medium;
	return;
      }
    }

    ms->type = ctOTHER;

  } else if( !mbw_strncasecmp(line, mbw_lit("Content-Transfer-Encoding:"), 
			      26) ) {
    line += 26;
    if( mbw_prefix(mystrcasestr)(line, mbw_lit("base64")) ) {
      ms->encoding = ceB64;
    } else if( mbw_prefix(mystrcasestr)(line, mbw_lit("quoted-printable")) ) {
      ms->encoding = ceQP;
    } else if( mbw_prefix(mystrcasestr)(line, mbw_lit("binary")) ) {
      ms->encoding = ceBIN;
    } else if( mbw_prefix(mystrcasestr)(line, mbw_lit("7bit")) ) {
      ms->encoding = ceSEVEN;
    } else {
      ms->encoding = ceID;
    }

  }
}

/* scans the line for the character strip_header_char, and truncates
 * from that point on, and switches strip_header_char to the special value 1.
 * If special value 1, truncates line from second char onwards.
 * If special value 0, does nothing.
 */
static __inline__
void mbw_prefix(strip_from_char)(MBOX_State *mbox, mbw_t *q) {
  if( mbox->mbw_prefix(strip_header_char) == mbw_lit('\x01') ) {
    if( *q++ ) {
      *q = mbw_lit('\0');
    }
  } else if( mbox->mbw_prefix(strip_header_char) ) {
    while(*q++) {
      if( *q == mbox->mbw_prefix(strip_header_char) ) {
	*q++ = mbw_lit('\n');
	*q = mbw_lit('\0');
	mbox->mbw_prefix(strip_header_char) = mbw_lit('\x01');
	break;
      }
    }
  }
}

static
Mheaderid mbw_prefix(identify_header)(mbw_t *line) {
#define HDRID(s,l,h) !mbw_strncasecmp(line, s,l) ? h
  if( mbw_isspace(*line) ) { 
    return hidCONTINUATION; 
  }
  switch(mbw_tolower(line[0])) {
  case mbw_lit('b'):
    return
      HDRID(mbw_lit("BCC:"),4,hidBCC) :
      hidUNDEF;
  case mbw_lit('c'):
    return
      HDRID(mbw_lit("Content-"),8,hidCONTENT_) :
      HDRID(mbw_lit("CC:"),3,hidCC) :
      HDRID(mbw_lit("Categor"),7,hidCATEGORY) :
      HDRID(mbw_lit("Comments:"),9,hidCOMMENTS) :
      hidUNDEF;
  case mbw_lit('f'):
    return
      HDRID(mbw_lit("From:"),5,hidFROM) :
      hidUNDEF;
  case mbw_lit('i'):
    return
      HDRID(mbw_lit("In-Reply-To:"),12,hidIN_REPLY_TO) :
      HDRID(mbw_lit("Importance:"),11,hidIMPORTANCE) :
      hidUNDEF;
  case mbw_lit('k'):
    return
      HDRID(mbw_lit("Keywords:"),9,hidKEYWORDS) :
      hidUNDEF;
  case mbw_lit('l'):
    return
      HDRID(mbw_lit("List-"),5,hidLIST_) :
      hidUNDEF;
  case mbw_lit('m'):
    return
      HDRID(mbw_lit("Message-ID:"),11,hidMESSAGE_ID) :
      HDRID(mbw_lit("MIME-Version:"),13,hidMIME_VERSION) :
      hidUNDEF;
  case mbw_lit('n'):
    return
      HDRID(mbw_lit("Notes:"),6,hidNOTE) :
      hidUNDEF;
  case mbw_lit('o'):
    return
      HDRID(mbw_lit("Original-"),8,hidORIGINAL_) :
      hidUNDEF;
  case mbw_lit('p'):
    return 
      HDRID(mbw_lit("Priority:"),9,hidPRIORITY) :
      hidUNDEF;
  case mbw_lit('r'):
    return 
      HDRID(mbw_lit("Received:"),9,hidRECEIVED) : 
      HDRID(mbw_lit("Return-Path:"),12,hidRETURN_PATH) :
      HDRID(mbw_lit("References:"),11,hidREFERENCES) :
      HDRID(mbw_lit("Return-Receipt-To:"),18,hidRETURN_RECEIPT_TO) :
      HDRID(mbw_lit("Reply-To:"),9,hidREPLY_TO) :
      HDRID(mbw_lit("Resent-"),7,hidRESENT_) :
      hidUNDEF;
  case mbw_lit('s'):
    return
      HDRID(mbw_lit("Subject:"),8,hidSUBJECT) :
      HDRID(mbw_lit("Sent:"),5,hidSENT) :
      HDRID(mbw_lit("Sender:"),7,hidSENDER) :
      hidUNDEF;
  case mbw_lit('t'):
    return
      HDRID(mbw_lit("To:"),3,hidTO) :
      HDRID(mbw_lit("Thread-"),7,hidTHREAD_) :
      hidUNDEF;
  case mbw_lit('x'):
    return
      HDRID(mbw_lit("X-MS"),4,hidX_MS) :
      HDRID(mbw_lit("X-"),2,hidX_) :
      hidUNDEF;
  case mbw_lit('u'):
    return
      HDRID(mbw_lit("User-Agent:"),11,hidUSER_AGENT) :
      hidUNDEF;
  }
  return hidUNDEF;
}


static
HEADER_Type mbw_prefix(scan_header_type)(MBOX_State *mbox, mbw_t *line) {

#define STRIP(q) {while(*q++) { if( *q == mbw_lit(';') ) { *q++ = mbw_lit('\n'); *q = mbw_lit('\0'); break; }}}
#define HDRIDCHK(x,y) ((mbox->hid == x) && (mbox->hstate = y))
  Mheaderid hid = mbw_prefix(identify_header)(line);

  if( hid == hidCONTINUATION ) {
    /* we don't update mbox->hid */
    mbw_prefix(strip_from_char)(mbox, line);
    return htCONT;
  } 

  mbox->hid = hid;
  if( HDRIDCHK(hidFROM,mhsFROM) ||
      HDRIDCHK(hidTO,mhsTO) ||
      HDRIDCHK(hidMESSAGE_ID,mhsUNDEF) ||
      HDRIDCHK(hidIN_REPLY_TO,mhsUNDEF) ||
      HDRIDCHK(hidSUBJECT,mhsSUBJECT) ) {
    mbox->mbw_prefix(strip_header_char) = mbw_lit('\0');
    mbw_prefix(strip_from_char)(mbox, line);
    return htSTANDARD;
  } else if( HDRIDCHK(hidRETURN_PATH,mhsTRACE) || 
	     HDRIDCHK(hidRECEIVED,mhsTRACE) ) {
    mbox->mbw_prefix(strip_header_char) = mbw_lit(';');
    mbw_prefix(strip_from_char)(mbox, line);
    return htTRACE;
  } else if( (mbox->hid == hidCONTENT_) &&
	     mbw_strchr(line + 8, mbw_lit(':')) ) {
    mbox->hstate = mhsMIME;
    mbox->mbw_prefix(strip_header_char) = mbw_lit('\0');
    return htMIME;
  } else if( HDRIDCHK(hidSENDER,mhsUNDEF) ||
	     HDRIDCHK(hidREPLY_TO,mhsUNDEF) ||
	     HDRIDCHK(hidBCC,mhsUNDEF) ||
	     HDRIDCHK(hidCC,mhsUNDEF) ||
	     HDRIDCHK(hidREFERENCES,mhsUNDEF) ) {
    mbox->mbw_prefix(strip_header_char) = mbw_lit('\0');
    mbw_prefix(strip_from_char)(mbox, line);
    return htEXTENDED;
  } else {
    /* if the line starts with a word missing a :, then 
       it could be a malformed continuation line */
    while( *line && !mbw_isspace(*line) && (*line != mbw_lit(':')) ) { line++; }
    if( *line == mbw_lit(':') ) {
      mbox->hstate = mhsUNDEF;
      mbox->mbw_prefix(strip_header_char) = mbw_lit('\0');
      return htUNDEF;
    } else {
      return htCONT;
    }
  }
}

/* static */
/* HEADER_Type mbw_prefix(scan_header_type)(MBOX_State *mbox, mbw_t *line) { */

/* #define STRIP(q) {while(*q++) { if( *q == mbw_lit(';') ) { *q++ = mbw_lit('\n'); *q = mbw_lit('\0'); break; }}} */

/* #define HDRCHK(s,l,h) (!mbw_strncasecmp(line, s,l) && (mbox->hstate = h)) */

/*   mbox->mm = mbw_prefix(identify_header)(line); */

/*   if( mbw_isspace(*line) ) { */
/*     mbw_prefix(strip_from_char)(mbox, line); */
/*     return htCONT; */
/*   } else if( HDRCHK(mbw_lit("From:"),5,mhsFROM) ||  */
/* 	     HDRCHK(mbw_lit("To:"),3,mhsTO) || */
/* 	     HDRCHK(mbw_lit("Message-ID:"),11,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("In-Reply-To:"),12,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("Subject:"),8,mhsSUBJECT) ) { */
/*     mbox->mbw_prefix(strip_header_char) = mbw_lit('\0'); */
/*     mbw_prefix(strip_from_char)(mbox, line); */
/*     return htSTANDARD; */
/*   } else if( HDRCHK(mbw_lit("Return-Path:"),12,mhsTRACE) ||  */
/* 	     HDRCHK(mbw_lit("Received:"),9,mhsTRACE) ) { */
/*     mbox->mbw_prefix(strip_header_char) = mbw_lit(';'); */
/*     mbw_prefix(strip_from_char)(mbox, line); */
/*     return htTRACE; */
/*   } else if( !mbw_strncasecmp(line, mbw_lit("Content-"),8) && */
/* 	     mbw_strchr(line + 8, mbw_lit(':')) ) { */
/*     mbox->hstate = mhsMIME; */
/*     mbox->mbw_prefix(strip_header_char) = mbw_lit('\0'); */
/*     return htMIME; */
/*   } else if( HDRCHK(mbw_lit("Sender:"),7,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("Reply-To:"),9,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("Bcc:"),4,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("Cc:"),3,mhsUNDEF) || */
/* 	     HDRCHK(mbw_lit("References:"),11,mhsUNDEF) ) { */
/*     mbox->mbw_prefix(strip_header_char) = mbw_lit('\0'); */
/*     mbw_prefix(strip_from_char)(mbox, line); */
/*     return htEXTENDED; */
/*   } else { */
/*     /\* if the line starts with a word missing a :, then  */
/*        it could be a malformed continuation line *\/ */
/*     while( *line && !mbw_isspace(*line) && (*line != mbw_lit(':')) ) { line++; } */
/*     if( *line == mbw_lit(':') ) { */
/*       mbox->hstate = mhsUNDEF; */
/*       mbox->mbw_prefix(strip_header_char) = mbw_lit('\0'); */
/*       return htUNDEF; */
/*     } else { */
/*       return htCONT; */
/*     } */
/*   } */
/* } */

static
int mbw_prefix(extract_mime_label)(mbw_t *line) {
  mbw_t *q;
  if( m_options & (1<<M_OPTION_HEADERS) ) {
    if( !mbw_strncasecmp(line, mbw_lit("Content-"),8) ) {
      line += 8;
      if( !mbw_strncasecmp(line, mbw_lit("Type:"),5) ) {
	/* we want both the mime type and the file name */
	q = (mbw_t *)mbw_prefix(mystrcasestr)(line, mbw_lit("name="));
	if( q ) { STRIP(q); } else { STRIP(line); }
	return 1;
      } else if( !mbw_strncasecmp(line, mbw_lit("Disposition:"),12) ) {
	STRIP(line);
	return 1;
      } else if( !mbw_strncasecmp(line, mbw_lit("ID:"),3) ||
		 !mbw_strncasecmp(line, mbw_lit("Description:"),12) ) {
	/* note: we only get first line of description */
	return 1;
      }
    } else if( mbw_isspace(*line) ) {
      q = (mbw_t *)mbw_prefix(mystrcasestr)(line, mbw_lit("name="));
      if( q ) { 
	STRIP(q); 
	return 1;
      }
    }
  }
  return 0;
}

/* 
 * this code generates mbox_line_filter() and w_mbox_line_filter() 
 *
 * returns true if the line should be processed further
 * depends on global mbox state
 */
bool_t mbw_prefix(mbox_line_filter)(MBOX_State *mbox, mbw_t *line, 
				    XML_State *xml) {
  bool_t line_empty = 0;
  bool_t doubledash = 0;
  bool_t process_line = 0; /* by default we skip the line */
  XML_Reset force_filter = xmlUNDEF;
  bool_t octet_stream = 0;

  line_empty = MBW_EMPTYLINE(line);
  doubledash = MBW_DOUBLEDASH(line);

  /* STEP 1: first perform state transitions */
  switch(mbox->state) {
  case msUNDEF:
    /* wait until we see the first nonempty line */
    if( !line_empty ) {
      mbox->state = msHEADER;
      mbox->substate = msuUNDEF;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      mbox->skip_until_boundary = 0;
    }
    break;
  case msHEADER:
    if( line_empty ) {
      mbox->state = msBODY;
      mbox->substate = msuUNDEF;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      /* don't reset skip_until_boundary */
      mbox->corruption_check = 5;
    } 
    break; 
  case msBODY:
    if( doubledash && mbw_prefix(check_mime_boundary)(mbox, line) ) {
      mbox->state = msATTACH;
      mbox->substate = msuUNDEF;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      mbox->skip_until_boundary = mbox->boundary.was_end;
      mbox->corruption_check = 0;
    } else if( doubledash && 
/* 	       mbw_prefix(outlook_message_announce)(line) || */
	       mbw_prefix(check_old_style_digest)(line) ) {
      mbox->state = msATTACH;
      mbox->substate = msuTRACK;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      mbox->skip_until_boundary = mbox->boundary.was_end;
      mbox->corruption_check = 0;
      /* since there are no mime headers, we impose a content type */
      /* note: we only try to detect digests because we 
	 want to remove the Date: headers */
      mbox->body.type = ctMESSAGE_RFC822;
    } else if( doubledash && 
	       (mbox->substate == msuARMOR) &&
	       mbw_prefix(check_armor_end)(line) ) {
      mbox->substate = msuTRACK;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      mbox->skip_until_boundary = mbox->boundary.was_end;
      mbox->corruption_check = 0;
    } else if( doubledash && 
	       (mbox->substate != msuARMOR) &&
	       mbw_prefix(check_armor_start)(line) ) {
      mbox->substate = msuARMOR;
      mbox->armor = maENABLED;
    } else if( mbox->prev_line_empty ) {
      if( doubledash && !(m_options & (1<<M_OPTION_PLAIN)) ) {
	/* could be a corrupted boundary - note 
	 * previous empty line is not required, but often true
	 */
	mbox->corruption_check = 5;
      } else if( !mbw_strncasecmp(line, mbw_lit("Content-"), 8) ) {
	mbw_prefix(mhe_line_filter)(line); 
	switch(mbw_prefix(scan_header_type)(mbox, line)) {
	case htMIME:
	  mbox->state = msATTACH;
	  mbox->substate = msuMIME;
	  mbox->hstate = mhsUNDEF;
	  mbox->armor = maUNDEF;
	  mbox->skip_until_boundary = 0;
	  mbox->corruption_check = 0;
	  break;
	default:
	  /* do nothing - so far so good */
	  break;
	}
      } else if( !mbw_strncmp(line, mbw_lit("From "), 5) ) {
	mbox->state = msHEADER;
	mbox->hid = hidUNDEF;
	mbox->substate = msuUNDEF;
	mbox->hstate = mhsUNDEF;
	mbox->armor = maUNDEF;
	mbox->skip_until_boundary = 0;
	mbox->corruption_check = 0;
      }
    } else if( mbox->corruption_check > 0 ) {
      mbox->corruption_check--;
      /* we filter out mail header extension codings - shouldn't do any harm */
      mbw_prefix(mhe_line_filter)(line); 
      switch(mbw_prefix(scan_header_type)(mbox, line)) {
      case htMIME:
	mbox->state = msATTACH;
	mbox->substate = msuMIME;
	mbox->hstate = mhsUNDEF;
	mbox->armor = maUNDEF;
	mbox->skip_until_boundary = 0;
	mbox->corruption_check = 0;
	break;
      default:
	/* do nothing - so far so good */
	break;
      }
    }
    break;
  case msATTACH:
    if( line_empty ) {
      switch(mbox->body.type) {
      case ctMESSAGE_RFC822:
	/* our mime parse isn't recursive - instead we start a 
	   new message and associate with it all later attachments */
	mbox->state = msHEADER;
	break;
      case ctAPPLICATION_MSWORD:
	mbox->state = msBODY;
	/* override encoding if undefined */
	if( mbox->body.encoding == ceUNDEF ) {
	  mbox->body.encoding = ceB64;
	}
	break;
      default:
	mbox->state = msBODY;
	break;
      }
      mbox->substate = msuUNDEF;
      mbox->hid = hidUNDEF;
      mbox->hstate = mhsUNDEF;
      mbox->armor = maUNDEF;
      mbox->skip_until_boundary = 0;
      mbox->corruption_check = 0;
    } 
    break;
  }

  mbox->prev_line_empty = line_empty; /* for next time */

  /* STEP 2: now clean up and prepare the line according to current state
   * and substate. 
   * After cleanup, the variable process_line indicates if the line
   * should be ignored. 
   * The substate can evolve while the current state is unchanging.
   */
  switch(mbox->state) {
  case msUNDEF:
    /* line is not processed */
    break;
  case msHEADER:
    switch(mbox->substate) {
    case msuUNDEF:
      /* flush caches */
      process_line = 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(b64_dc)), line, 1) || 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(qp_dc)), line, 1);
      if( process_line ) {
	/* we still remember previous type/encoding, decide if we need filter */
	force_filter = select_xml_defaults(&mbox->body);
	octet_stream = (mbox->body.type == ctOCTET_STREAM) ||
	  (mbox->body.type == ctAPPLICATION_MSWORD);
      }
      /* there are no default mime types for headers */
      mbox->header.type = mbox->body.type = ctUNDEF;
      mbox->header.encoding = mbox->body.encoding = ceUNDEF;
      /* switch to normal state next time */
      mbox->substate = msuOTHER;
      mbox->corruption_check = 0;
      mbox->skip_header = 0;
      mbox->plainstate = psPLAIN;
      /* don't break, as the current line could contain 
	 interesting headers already */

    default:
      /* switch substate if necessary */
      switch(mbw_prefix(scan_header_type)(mbox, line)) {
      case htSTANDARD:
	if( m_options & (1<<M_OPTION_NOHEADERS) ) {
	  mbox->substate = (mbox->hstate == mhsSUBJECT) ? msuTRACK : msuOTHER;
	} else {
	  mbox->substate = msuTRACK;
	}
	break;
      case htEXTENDED:
	mbox->substate = (m_options & (1<<M_OPTION_HEADERS)) ? msuTRACK : msuOTHER;
	break;
      case htTRACE:
	mbox->substate = (m_options & (1<<M_OPTION_THEADERS)) ? msuTRACK : msuOTHER;
	break;
      case htMIME:
	mbox->substate = msuMIME;
	break;
      case htCONT:
	/* nothing */
	break;	
      case htUNDEF:
	mbox->substate = msuOTHER;
	break;
      }

      /* process substate */
      
      switch(mbox->substate) {
      case msuTRACK:
	process_line = mbw_prefix(mhe_line_filter)(line);
	break;
      case msuMIME:
	mbw_prefix(mhe_line_filter)(line);
	mbw_prefix(extract_mime_types)(line, &mbox->header);
	mbox->skip_until_boundary = 
	  mbw_prefix(extract_mime_boundary)(mbox, line) || mbox->skip_until_boundary;
	/* this comes last, modifies line */
	process_line = mbw_prefix(extract_mime_label)(line); 
	break;
      case msuUNDEF:
      case msuOTHER:
	mbox->hstate = mhsXHEADER;
      case msuARMOR:
	process_line = mbw_prefix(extract_header_label)(mbox, line);
	break;
      }
    }
    break;
  case msBODY:
    switch(mbox->substate) {
    case msuUNDEF:
      /* flush caches */
      process_line = 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(b64_dc)), line, 1) || 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(qp_dc)), line, 1);
      if( process_line ) {
	/* we still remember previous type/encoding, decide if we need filter */
	force_filter = select_xml_defaults(&mbox->body);
	octet_stream = (mbox->body.type == ctOCTET_STREAM) ||
	  (mbox->body.type == ctAPPLICATION_MSWORD);
      }
      /* bodies by default inherit the header mime types */
      if( mbox->body.type == ctUNDEF ) 
	{ mbox->body.type = mbox->header.type; }
      if( mbox->body.encoding == ceUNDEF ) 
	{ mbox->body.encoding = mbox->header.encoding; }

      /* switch to normal state next time */
      mbox->substate = msuTRACK;
      mbox->plainstate = psPLAIN;
      break;
    case msuARMOR:
      switch(mbox->armor) {
      case maUNDEF:
	process_line = 1;
	break;
      case maENABLED:
	process_line = mbw_prefix(armor_filter)(line);
	break;
      }

      break;
    default:
      if( mbox->skip_until_boundary ) {
	process_line = 0;
      } else {
	switch(mbox->body.type) {
	case ctOCTET_STREAM:
	case ctAPPLICATION_MSWORD:
	  if( !(m_options & (1<<M_OPTION_ATTACHMENTS)) ) {
	    process_line = 0;
	    break;
	  } else {
	    /* otherwise fall through */
	    octet_stream = 1;
	  }
	case ctUNDEF:  /* the header didn't say, so we must assume text */
	case ctMESSAGE_RFC822:
	case ctTEXT_PLAIN:
	  switch(mbox->body.encoding) {
	  case ceBIN:
	    process_line = 1;
	    break;
	  case ceUNDEF:
	  case ceSEVEN:
	  case ceID:
	    process_line = ((m_options & (1<<M_OPTION_PLAIN)) ?
			    1 : mbw_prefix(plain_text_filter)(mbox, line));
	    break;
	  case ceQP:
 	    process_line = 
	      mbw_prefix(qp_line_filter)(&(mbox->mbw_prefix(qp_dc)), line);
	    break;
	  case ceB64:
	    process_line = 
	      mbw_prefix(b64_line_filter)(&(mbox->mbw_prefix(b64_dc)), line);
	    break;
	  }
	  break;
	case ctTEXT_RICH:
	case ctTEXT_HTML:
	case ctTEXT_XML:
	case ctTEXT_SGML:
	case ctTEXT_UNKNOWN:
	  switch(mbox->body.encoding) {
	  case ceBIN:
	  case ceUNDEF:
	  case ceSEVEN:
	  case ceID:
	    process_line = 1;
	    break;
	  case ceQP:
 	    process_line = 
	      mbw_prefix(qp_line_filter)(&(mbox->mbw_prefix(qp_dc)), line);
	    break;
	  case ceB64:
	    process_line = 
	      mbw_prefix(b64_line_filter)(&(mbox->mbw_prefix(b64_dc)), line);
	    break;
	  }
	  break;
	case ctIMAGE:
	case ctAUDIO:
	case ctVIDEO:
	case ctMODEL:
	case ctOTHER:
	  process_line = 0;
	  break;
	}
      }
      break;
    }
    break;
  case msATTACH:
    switch(mbox->substate) {
    case msuUNDEF:
      /* flush caches */
      process_line = 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(b64_dc)), line, 1) || 
	mbw_prefix(flush_cache)(&(mbox->mbw_prefix(qp_dc)), line, 1);
      if( process_line ) {
	/* we still remember previous type/encoding, decide if we need filter */
	force_filter = select_xml_defaults(&mbox->body);
	octet_stream = (mbox->body.type == ctOCTET_STREAM) ||
	  (mbox->body.type == ctAPPLICATION_MSWORD);
      }
      /* attachments by default inherit the header mime types */
      mbox->body.type = mbox->header.type;
      mbox->body.encoding = mbox->header.encoding;
      /* switch to normal state next time */
      /* this has a nice side-effect: if the first line is a htCONT,
	 then it gets displayed and that's the right thing to do, 
	 because if the first line is a htCONT, then the ATTACH header
	 is not a header at all, ie the paragraph was misidentified. */
      mbox->substate = msuTRACK;
      mbox->plainstate = psPLAIN;
      break;
    default:
      /* switch substate if necessary */
      switch(mbw_prefix(scan_header_type)(mbox, line)) {
      case htSTANDARD:
	mbox->substate = msuTRACK;
	break;
      case htEXTENDED:
	mbox->substate = (m_options & (1<<M_OPTION_HEADERS)) ? msuTRACK : msuOTHER;
	break;
      case htTRACE:
	mbox->substate = (m_options & (1<<M_OPTION_THEADERS)) ? msuTRACK : msuOTHER;
	break;
      case htMIME:
	mbox->substate = msuMIME;
	break;
      case htCONT:
	/* nothing */
	break;	
      case htUNDEF:
	mbox->substate = msuOTHER;
	break;
      }
      /* process substate */
      switch(mbox->substate) {
      case msuTRACK:
	process_line = 1;
	break;
      case msuUNDEF:
      case msuOTHER:
      case msuARMOR:
	process_line = 0;
	break;
      case msuMIME:
	mbw_prefix(mhe_line_filter)(line);
	mbw_prefix(extract_mime_types)(line, &mbox->body);
	mbox->skip_until_boundary = 
	  mbw_prefix(extract_mime_boundary)(mbox, line) || mbox->skip_until_boundary;
	/* this comes last, modifies line */
	process_line = mbw_prefix(extract_mime_label)(line);
	break;
      }
      break;
    }
    break;
  }

  /* STEP 3: activate filters */

  if( octet_stream ) {
    process_line = mbw_prefix(strings1_filter)(line);
  } 

  if( !process_line && line_empty ) {
    /* don't touch this: the end of file is artificially marked by an
       empty line */
    process_line = 
      mbw_prefix(flush_cache)(&(mbox->mbw_prefix(b64_dc)), line, 1) || 
      mbw_prefix(flush_cache)(&(mbox->mbw_prefix(qp_dc)), line, 1);
  }

  if( force_filter != xmlUNDEF ) {
    reset_xml_character_filter(xml, force_filter);
  } else {
    if( mbox->state == msBODY ) {
      if( mbox->skip_until_boundary ) {
	process_line = 0;
      } else {
	reset_xml_character_filter(xml, select_xml_defaults(&(mbox->body)));
      }
    } else {
      reset_xml_character_filter(xml, xmlDISABLE);
    }
  }

  /* we also process empty lines, as they can be helpful for n-gram boundaries */
  return process_line || line_empty;
}



/***********************************************************
 * HTML PARSING FUNCTIONS                                  *
 ***********************************************************/

/* 
 * this code generates decode_html_entity() and w_decode_html_entity() 
 * (this is ugly, but I am _not_ building a string hash, sheesh).
 *
 * note: the conversion from unicode to multibyte depends on the current
 * locale, but also assumes that wchar_t *is* unicode internally. Both assumptions
 * can be false on weird compilers. In case the locale is incapable of doing the job, 
 * we convert based on the hex code.
 * 
 * note2: the conversion is not always faithful, even so. We try not to convert 
 * characters which could look like control codes to the html parser later.
 *
 * note3: upon successful conversion, *qq is incremented by the
 * character, and *lline is incremented by the entity length - 1, so
 * that you still need to increment *lline by one to obtain the next
 * parseable input. If conversion is unsuccessful, the pointers are not
 * modified. Check the return value for success or failure.
 *
 * note4: this function really needs to be reworked a bit. It ought to be possible
 * to do the right thing even for machines with missing wchar_t.
 */
static
bool_t mbw_prefix(decode_html_entity)(mbw_t **lline, mbw_t **qq) {
  bool_t retval = 0;
  mbw_t *line = *lline;
  mbw_t *q = *qq;
/*   printf("\nline = %p q = %p (line - q) = %d\n", line, q, line - q); */
/*   printf("[[[%s]]]\n", line); */
#if defined HAVE_MBRTOWC 

  mbw_t *r = NULL;
#if defined MBW_MB
  int s,t;
  mbw_t scratch[16]; /* C compiler complains  about MB_CUR_MAX */
#endif
  wchar_t c = 0; /* this must always be wchar_t */

  switch(line[1]) {
  case mbw_lit('#'):
    if( (line[2] == mbw_lit('x')) || (line[2] == mbw_lit('X')) ) {
#if defined MBW_MB || (defined MBW_WIDE && defined HAVE_WCSTOL)
      c = (wchar_t)mbw_strtol(line + 3, &r, 16);
#else
      /* can't convert, but skip the payload anyway */
      for(r = line + 3; isxdigit(*r); r++);
#endif
    } else {
#if defined MBW_MB || (defined MBW_WIDE && defined HAVE_WCSTOL)
      c = (wchar_t)mbw_strtol(line + 2, &r, 10);
#else
      /* can't convert, but skip the payload anyway */
      for(r = line + 2; isdigit(*r); r++);
#endif
    }
    break;

#define ENTITY(x,y,z) if( !mbw_strncmp((line + 3), (x + 2), (y - 2)) ) \
                           { c = (z); r = line + (y) + 1; }

  case mbw_lit('a'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("aacute"),6,0xe1);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("acute"),5,0xb4) else ENTITY(mbw_lit("acirc"),4,0xe2);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("aelig"),5,0xe6);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("agrave"),6,0xe0);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("alpha"),5,0x03b1) else ENTITY(mbw_lit("alefsym"),7,0x2135);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("amp"),3,0x26);
      break;
    case mbw_lit('n'):
      ENTITY(mbw_lit("ang"),3,0x2220) else ENTITY(mbw_lit("and"),3,0x2227);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("aring"),5,0xe5);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("asymp"),5,0x2248);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("atilde"),6,0xe3);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("auml"),4,0xe4);
      break;
    }
    break;

  case mbw_lit('A'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Aacute"),6,0xc1);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("Acirc"),5,0xc2);
      break;
    case mbw_lit('E'):
      ENTITY(mbw_lit("AElig"),5,0xc6);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("Agrave"),6,0xc0);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("Alpha"),5,0x0391);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("Aring"),5,0xc5);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("Atilde"),6,0xc3);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Auml"),4,0xc4);
      break;
    }
    break;

  case mbw_lit('b'):
    switch(line[2]) {
    case mbw_lit('d'):
      ENTITY(mbw_lit("bdquo"),5,0x201e);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("beta"),4,0x03b2);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("bull"),4,0x2022);
      break;
    }
    break;

  case mbw_lit('B'):
    switch(line[2]) {
    case mbw_lit('e'):
      ENTITY(mbw_lit("Beta"),4,0x0392);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("Brvbar"),6,0xa6);
      break;
    }
    break;

  case mbw_lit('c'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("cap"),3,0x2229);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("ccedil"),6,0xe7);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("cent"),4,0xa2) else ENTITY(mbw_lit("cedil"),5,0xb8);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("chi"),3,0x03c7);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("circ"),4,0x02c6);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("clubs"),5,0x2663);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("copy"),4,0xa9) else ENTITY(mbw_lit("cong"),4,0x2245);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("crarr"),5,0x21b5);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("curren"),6,0xa4) else ENTITY(mbw_lit("cup"),3,0x222a);
      break;
    }
    break;

  case mbw_lit('C'):
    switch(line[2]) {
    case mbw_lit('c'):
      ENTITY(mbw_lit("Ccedil"),6,0xc7);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("Chi"),3,0x03a7);
      break;
    }
    break;

  case mbw_lit('d'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("darr"),4,0x2193) else ENTITY(mbw_lit("dagger"),6,0x2020);
      break;
    case mbw_lit('A'):
      ENTITY(mbw_lit("dArr"),4,0x21d3);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("delta"),5,0x03b4);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("divide"),6,0xf7) else ENTITY(mbw_lit("diams"),5,0x2666);
      break;
    }
    break;

  case mbw_lit('D'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Dagger"),6,0x2021);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("Deg"),3,0xb0) else ENTITY(mbw_lit("Delta"),5,0x0394);
      break;
    }
    break;

  case mbw_lit('e'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("eacute"),6,0xe9);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("ecirc"),5,0xea);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("egrave"),6,0xe8);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("empty"),5,0x2205) else ENTITY(mbw_lit("emsp"),4,0x2003);
      break;
    case mbw_lit('n'):
      ENTITY(mbw_lit("ensp"),4,0x2002);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("epsilon"),7,0x03b5);
      break;
    case mbw_lit('q'):
      ENTITY(mbw_lit("equiv"),5,0x2261);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("eth"),3,0xf0) else ENTITY(mbw_lit("eta"),3,0x03b7);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("euml"),4,0xeb) else ENTITY(mbw_lit("euro"),4,0x20ac);
      break;
    case mbw_lit('x'):
      ENTITY(mbw_lit("exist"),5,0x2203);
      break;
    }
    break;

  case mbw_lit('E'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Eacute"),6,0xc9);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("Ecirc"),5,0xca);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("Egrave"),6,0xc8);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("Epsilon"),7,0x0395);
      break;
    case mbw_lit('T'):
      ENTITY(mbw_lit("ETH"),3,0xd0);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("Eta"),3,0x0397);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Euml"),4,0xcb);
      break;
    }
    break;

  case mbw_lit('f'):
    switch(line[2]) {
    case mbw_lit('n'):
      ENTITY(mbw_lit("fnof"),4,0x0192);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("forall"),6,0x2200);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("frac14"),6,0xbc) else ENTITY(mbw_lit("frac12"),6,0xbd) else
	ENTITY(mbw_lit("frac34"),6,0xbe) else ENTITY(mbw_lit("frasl"),5,0x2044);
      break;
    }
    break;

  case mbw_lit('F'):
    /* nothing */
    break;

  case mbw_lit('g'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("gamma"),5,0x3b3);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("ge"),2,0x2265);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("gt"),2,0x3e);
      break;
    }
    break;

  case mbw_lit('G'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Gamma"),5,0x0393);
      break;
    }
    break;

  case mbw_lit('h'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("harr"),4,0x2194);
      break;
    case mbw_lit('A'):
      ENTITY(mbw_lit("hArr"),4,0x21d4);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("hearts"),6,0x2665) else ENTITY(mbw_lit("hellip"),6,0x2026);
      break;
    }
    break;

  case mbw_lit('H'):
    /* nothing */
    break;

  case mbw_lit('i'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("iacute"),6,0xed);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("icirc"),5,0xee);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("iexcl"),5,0xa1);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("igrave"),6,0xec);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("image"),5,0x2111);
      break;
    case mbw_lit('n'):
      ENTITY(mbw_lit("infin"),5,0x221e) else ENTITY(mbw_lit("int"),3,0x222b);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("iota"),4,0x03b9);
      break;
    case mbw_lit('q'):
      ENTITY(mbw_lit("iquest"),6,0xbf);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("isin"),4,0x2208);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("iuml"),4,0xef);
      break;
    }
    break;

  case mbw_lit('I'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Iacute"),6,0xcd);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("Icirc"),5,0xce);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("Igrave"),6,0xcc);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("Iota"),4,0x0399);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Iuml"),4,0xcf);
      break;
    }
    break;

  case mbw_lit('j'):
    /* nothing */
    break;

  case mbw_lit('J'):
    /* nothing */
    break;

  case mbw_lit('k'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("kappa"),5,0x03ba);
      break;
    }
    break;

  case mbw_lit('K'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Kappa"),5,0x039a);
      break;
    }
    break;

  case mbw_lit('l'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("lambda"),6,0x03bb) else ENTITY(mbw_lit("lang"),4,0x2329);
      break;
    case mbw_lit('A'):
      ENTITY(mbw_lit("lArr"),4,0x21d0);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("lceil"),5,0x2308);
      break;
    case mbw_lit('d'):
      ENTITY(mbw_lit("ldquo"),5,0x201c);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("le"),2,0x2264);
      break;
    case mbw_lit('f'):
      ENTITY(mbw_lit("lfloor"),6,0x2309);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("lowast"),6,0x2217) else ENTITY(mbw_lit("loz"),3,0x25ca);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("lrm"),3,0x200e);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("lsquo"),5,0x2018) else ENTITY(mbw_lit("lsaquo"),6,0x2039);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("lt"),2,0x3c);
      break;
    }
    break;

  case mbw_lit('L'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Laquo"),5,0xab) else ENTITY(mbw_lit("Lambda"),6,0x039b) else
	ENTITY(mbw_lit("Larr"),4,0x2190);
      break;
    }
    break;

  case mbw_lit('m'):
    switch(line[2]) {
    case mbw_lit('d'):
      ENTITY(mbw_lit("mdash"),5,0x2014);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("minus"),5,0x2212);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("mu"),2,0x03bc);
      break;
    }
    break;

  case mbw_lit('M'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Macr"),4,0xaf);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("Micro"),5,0xb5) else ENTITY(mbw_lit("Middot"),6,0xb7);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Mu"),2,0x039c);
      break;
    }
    break;

  case mbw_lit('n'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("nabla"),5,0x2207);
      break;
    case mbw_lit('b'):
      ENTITY(mbw_lit("nbsp"),4,0xa0);
      break;
    case mbw_lit('d'):
      ENTITY(mbw_lit("ndash"),5,0x2013);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("ne"),2,0x2260);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("ni"),2,0x220b);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("not"),3,0xac) else ENTITY(mbw_lit("notin"),5,0x2209);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("nsub"),4,0x2284);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("ntilde"),6,0xf1);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("nu"),2,0x03bd);
      break;
    }
    break;

  case mbw_lit('N'):
    switch(line[2]) {
    case mbw_lit('t'):
      ENTITY(mbw_lit("Ntilde"),6,0xd1);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Nu"),2,0x039d);
      break;
    }
    break;

  case mbw_lit('o'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("oacute"),6,0xf3);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("ocirc"),5,0xf4);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("oelig"),5,0x0153);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("ograve"),6,0xf2);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("oline"),5,0x203e);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("omicron"),7,0x03bf) else ENTITY(mbw_lit("omega"),5,0x03c9);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("oplus"),5,0x2295);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("ordf"),4,0xaa) else ENTITY(mbw_lit("ordm"),4,0xba) else
	ENTITY(mbw_lit("or"),2,0x2228);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("oslash"),6,0xf8);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("otilde"),6,0xf5) else ENTITY(mbw_lit("otimes"),6,0x2297);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("ouml"),4,0xf6);
      break;
    }
    break;

  case mbw_lit('O'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Oacute"),6,0xd3);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("Ocirc"),5,0xd4);
      break;
    case mbw_lit('E'):
      ENTITY(mbw_lit("OElig"),5,0x0152);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("Omicron"),7,0x039f) else ENTITY(mbw_lit("Omega"),5,0x03a9);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("Ograve"),6,0xd2);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("Oslash"),6,0xd8);
      break;
    case mbw_lit('t'):
      ENTITY(mbw_lit("Otilde"),6,0xd5);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Ouml"),4,0xd6);
      break;
    } 
    break;

  case mbw_lit('p'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("part"),4,0x2202);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("perp"),4,0x22a5) else ENTITY(mbw_lit("permil"),6,0x2030);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("phi"),3,0x03c6);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("pi"),2,0x03c0) else ENTITY(mbw_lit("piv"),3,0x03d6);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("prime"),5,0x2032) else	ENTITY(mbw_lit("prod"),4,0x220f);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("psi"),3,0x03c8);
      break;
    }
    break;

  case mbw_lit('P'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Para"),4,0xb6);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("Phi"),3,0x03a6);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("Pi"),2,0x03a0);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("Plusmn"),6,0xb1);
      break;
    case mbw_lit('o'):
      ENTITY(mbw_lit("Pound"),5,0xa3);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("Prime"),5,0x2033) else	ENTITY(mbw_lit("Prop"),4,0x221d);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("Psi"),3,0x03a8);
      break;
    }
    break;

  case mbw_lit('q'):
    switch(line[2]) {
    case mbw_lit('u'):
      ENTITY(mbw_lit("quot"),4,0x22);
      break;
    }
    break;

  case mbw_lit('Q'):
    /* nothing */
    break;

  case mbw_lit('r'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("rarr"),4,0x2192) else ENTITY(mbw_lit("radic"),5,0x221a) else
	ENTITY(mbw_lit("rang"),4,0x232a);
      break;
    case mbw_lit('A'):
      ENTITY(mbw_lit("rArr"),4,0x21d2);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("rceil"),5,0x2309);
      break;
    case mbw_lit('d'):
      ENTITY(mbw_lit("rdquo"),5,0x201d);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("real"),4,0x211C) else ENTITY(mbw_lit("reg"),3,0xae);
      break;
    case mbw_lit('f'):
      ENTITY(mbw_lit("rfloor"),6,0x230a);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("rho"),3,0x03c1);
      break;
    case mbw_lit('l'):
      ENTITY(mbw_lit("rlm"),3,0x200f);
      break;
    case mbw_lit('s'):
      ENTITY(mbw_lit("rsquo"),5,0x2019) else	ENTITY(mbw_lit("rsaquo"),6,0x203a);
      break;
    }
    break;

  case mbw_lit('R'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Raquo"),5,0xbb);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("Reg"),3,0xae);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("Rho"),3,0x03a1);
      break;
    }
    break;

  case mbw_lit('s'):
    switch(line[2]) {
    case mbw_lit('b'):
      ENTITY(mbw_lit("sbquo"),5,0x201a);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("scaron"),6,0x0161);
      break;
    case mbw_lit('d'):
      ENTITY(mbw_lit("sdot"),4,0x22c5);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("sect"),4,0xa7);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("shy"),3,0xad);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("sigmaf"),6,0x03c2) else ENTITY(mbw_lit("sigma"),5,0x03c3) else
	ENTITY(mbw_lit("sim"),3,0x223c);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("spades"),6,0x2660);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("sup2"),4,0xb2) else ENTITY(mbw_lit("sup3"),4,0xb3) else
	ENTITY(mbw_lit("sup1"),4,0xb9) else ENTITY(mbw_lit("sum"),3,0x2211) else
	  ENTITY(mbw_lit("sub"),3,0x2282) else ENTITY(mbw_lit("sup"),3,0x2283) else
	    ENTITY(mbw_lit("sube"),4,0x2286) else ENTITY(mbw_lit("supe"),4,0x2287);
      break;
    case mbw_lit('z'):
      ENTITY(mbw_lit("szlig"),5,0xdf);
      break;
    }
    break;

  case mbw_lit('S'):
    switch(line[2]) {
    case mbw_lit('c'):
      ENTITY(mbw_lit("Scaron"),6,0x0160);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("Sigma"),5,0x03a3);
      break;
    }
    break;

  case mbw_lit('t'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("tau"),3,0x03c4);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("thorn"),5,0xfe) else ENTITY(mbw_lit("theta"),5,0x03b8) else
	ENTITY(mbw_lit("thetasym"),8,0x03d1) else ENTITY(mbw_lit("there4"),6,0x2234) else
	  ENTITY(mbw_lit("thinsp"),6,0x2009);
      break;
    case mbw_lit('i'):
      ENTITY(mbw_lit("times"),5,0xd7) else ENTITY(mbw_lit("tilde"),5,0x02dc);
      break;
    case mbw_lit('r'):
      ENTITY(mbw_lit("trade"),5,0x2122);
      break;
    }
    break;

  case mbw_lit('T'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Tau"),3,0x03a4);
      break;
    case mbw_lit('h'):
      ENTITY(mbw_lit("Theta"),5,0x0398);
      break;
    case mbw_lit('H'):
      ENTITY(mbw_lit("THORN"),5,0xde);
      break;
    }
    break;

  case mbw_lit('u'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("uacute"),6,0xfa) else ENTITY(mbw_lit("uarr"),4,0x2191);
      break;
    case mbw_lit('A'):
      ENTITY(mbw_lit("uArr"),4,0x21d1);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("ucirc"),5,0xfb);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("ugrave"),6,0xf9);
      break;
    case mbw_lit('m'):
      ENTITY(mbw_lit("uml"),3,0xa8);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("upsilon"),7,0xc5) else ENTITY(mbw_lit("upsih"),5,0x03d2);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("uuml"),4,0xfc);
      break;
    }
    break;

  case mbw_lit('U'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Uacute"),6,0xda);
      break;
    case mbw_lit('c'):
      ENTITY(mbw_lit("Ucirc"),5,0xdb);
      break;
    case mbw_lit('g'):
      ENTITY(mbw_lit("Ugrave"),6,0xd9);
      break;
    case mbw_lit('p'):
      ENTITY(mbw_lit("Upsilon"),7,0xa5);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Uuml"),4,0xdc);
      break;
    }
    break;

  case mbw_lit('v'):
    /* nothing */
    break;

  case mbw_lit('V'):
    /* nothing */
    break;

  case mbw_lit('w'):
    switch(line[2]) {
    case mbw_lit('e'):
      ENTITY(mbw_lit("weierp"),6,0x2118);
      break;
    }
    break;

  case mbw_lit('W'):
    /* nothing */
    break;

  case mbw_lit('x'):
    switch(line[2]) {
    case mbw_lit('i'):
      ENTITY(mbw_lit("xi"),2,0x03be);
      break;
    }
    break;

  case mbw_lit('X'):
    switch(line[2]) {
    case mbw_lit('i'):
      ENTITY(mbw_lit("Xi"),2,0x039e);
      break;
    }
    break;

  case mbw_lit('y'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("yacute"),6,0xfd);
      break;
    case mbw_lit('e'):
      ENTITY(mbw_lit("yen"),3,0xa5);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("yuml"),4,0xff);
      break;
    }
    break;

  case mbw_lit('Y'):
    switch(line[2]) {
    case mbw_lit('a'):
      ENTITY(mbw_lit("Yacute"),6,0xdd);
      break;
    case mbw_lit('u'):
      ENTITY(mbw_lit("Yuml"),4,0x0178);
      break;
    }
    break;

  case mbw_lit('z'):
    switch(line[2]) {
    case mbw_lit('e'):
      ENTITY(mbw_lit("zeta"),4,0x03b6);
      break;
    case mbw_lit('w'):
      ENTITY(mbw_lit("zwnj"),4,0x200c) else ENTITY(mbw_lit("zwj"),3,0x200d);
      break;
    }
    break;

  case mbw_lit('Z'):
    switch(line[2]) {
    case mbw_lit('e'):
      ENTITY(mbw_lit("Zeta"),4,0x0396);
      break;
    }
    break;

  default:
    break;
  }

  /* some values of c are not allowed, because they interfere with the
     html parser */
  switch(c) {
  case L'\0':
  case L'\001': /* TOKENSEP */
  case L'\002': /* CLASSEP */
  case L'\003': /* DIAMOND */
    /* reserved control codes */
    c = L' '; break;
  case L'<': c = L'('; break;
  case L'>': c = L')'; break;
  default: break;
  }

  /* normally, entities end with ';', which will be skipped
     after we exit this function. However, we're lenient: if
     we don't point to ';', then we back up by one so that later
     we don't skip this character. Note this is safe, because
     ENTITY makes r point to at least line +1, and otherwise r is NULL */
  if( r && (*r != mbw_lit(';')) ) { r--; }

#if defined MBW_WIDE

  if( c && r ) {
    *q++ = c;
    line = r;
    retval = 1;
  } else {
    /* do nothing */
  }
#else

  /* now if c is nonzero, then we found the entity */
  if( c && r ) {
    if( c == 0xa0 ) { 
      /* shortcut for &nbsp; */
      *q++ = mbw_lit(' ');
    } else {
      s = wcrtomb(scratch,c,NULL);
      if( (s > -1) && (q + s <= r) ) {
	for(t = 0; t < s; t++) { *q++ = scratch[t]; }
      } else {
	/* locale doesn't recognize this char */
	s = c;
	if( s < 0xFF ) { *q++ = (mbw_t)s; }
      }
    }
    line = r;
    retval = 1;
  } else {
    /* do nothing */
  }
#endif

#else /* HAVE_MBRTOWC is not defined */

  /* do nothing */

#endif

  /* reminder: if no conversion is possible, we do nothing - 
     I always forget this and try to update q and line, thereby
     introducing bugs */
  *lline = line;
  *qq = q;
  return retval;
}

/* 
 * this code generates decode_escaped_uri_character() and 
 * w_decode_escaped_uri_character().
 *
 * NOTE: this doesn't cope correctly with the case that the
 * URI encoded character is itself encoded as html entities.
 * For example, %20 can itself be encoded as &#37;&#50;&#48;
 * Since the first char is decoded as '%' (otherwise we wouldn't
 * be inside decode_uri_character(), the function effectively
 * tries to decode %&#50;&#48; and fails. This is harmless, as
 * the ultimately decode line will be %20 instead of ' '. 
 */
void mbw_prefix(decode_uri_character)(mbw_t **lline, mbw_t **qq) {
  mbw_t *line = *lline;
  mbw_t *q = *qq;
  mbw_t scratch[3];
  mbw_t c = 0;
  mbw_t *r;

  if( *line == mbw_lit('%') ) {
#if defined MBW_MB || (defined MBW_WIDE && defined HAVE_WCSTOL)
    /* check that the next two chars are hex */
    for(r = line + 1; mbw_isspace(*r); r++);
    if( mbw_isxdigit(*r) ) {
      scratch[0] = *r;
      for(r++; mbw_isspace(*r); r++);
      if( mbw_isxdigit(*r) ) {
	scratch[1] = *r;
	scratch[2] = mbw_lit('\0');
	c = (mbw_t)mbw_strtol(scratch, NULL, 16);
      }
    }
#endif
    if( c ) {
      *q++ = c;
      line = r;
    } else {
      *q++ = *line;
    }

  } else if( mbw_isspace(*line) ) {
    /* ignore */
  } else {
    /* not an escaped character */
    *q++ = *line;
  }

  *lline = line;
  *qq = q;
}

/* this reads one or more characters from line and outputs 
 * zero or one character at q. 
 * The characters output depend on the xml.attribute state, but
 * the function never outputs more than it reads. Thus if q <= line
 * to begin with, this is preserved. This is necessary as the function
 * is normally used to modify line in-place.
 *
 * For URL type attributes, this function assumes the standard URI
 * form scheme:netloc/extra, and only prints the netloc part.
 * 
 * A special case is the URI javascript:xxxxx, which is treated differently.
 *
 * CAUTION: if you increment qq, don't also call decode_uri_character().
 *          Just do one or the other.
 */
static
void mbw_prefix(xml_attribute_filter)(XML_State *xml, mbw_t **lline, mbw_t **qq) {

  if( (xml->attribute != UNDEF) && (*(*lline) == mbw_lit('&')) ) {
    if( mbw_prefix(decode_html_entity)(lline, qq) ) {
      (*lline)++;
    }
  }

  switch(xml->attribute) {
  case SRC:
    if( *(*lline) == mbw_lit(':') ) { 
      xml->attribute = SRC_NETLOC_PREFIX; 
      *(*qq)++ = *(*lline);
    } else if( mbw_strncasecmp(*lline, mbw_lit("javascript:"), 11) == 0 ) {
      /* scripts are dealt differently */
      xml->attribute = JSCRIPT;
      if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
	*(*qq)++ = *(*lline);
      }
    } else {
      *(*qq)++ = *(*lline);
    }
    break;
  case SRC_NETLOC_PREFIX:
    switch(*(*lline)) {
    case mbw_lit('/'):
      *(*qq)++ = *(*lline);
      break;
    case mbw_lit('?'):
    case mbw_lit(';'):
    case mbw_lit('#'):
    case mbw_lit('&'):
      xml->attribute = SRC_NETLOC_SUFFIX;
      *(*qq)++ = *(*lline);
      break;
    default:
      mbw_prefix(decode_uri_character)(lline,qq);
      xml->attribute = SRC_NETLOC;
      break;
    }
    break;
  case SRC_NETLOC:
    switch(*(*lline)) {
    case mbw_lit('/'):
      *(*qq)++ = *(*lline);
      xml->attribute = SRC_NETLOC_PATH;
      break;
    case mbw_lit('?'):
    case mbw_lit(';'):
    case mbw_lit('#'):
    case mbw_lit('&'):
      xml->attribute = SRC_NETLOC_SUFFIX;
      break;
    default:
      mbw_prefix(decode_uri_character)(lline,qq);
      break;
    }
    break;
  case SRC_NETLOC_PATH:
    switch(*(*lline)) {
    case mbw_lit('.'):
      *(*qq)++ = *(*lline);
      break;
    case mbw_lit('?'):
    case mbw_lit(';'):
    case mbw_lit('#'):
    case mbw_lit('&'):
      xml->attribute = SRC_NETLOC_SUFFIX;
      *(*qq)++ = *(*lline);
      break;
    default:
      mbw_prefix(decode_uri_character)(lline,qq);
      break;
    }
    break;
  case SRC_NETLOC_SUFFIX:
/*     *(*qq)++ = mbw_lit(' '); */
    *(*qq)++ = *(*lline);
    break;
  case ALT:
    *(*qq)++ = *(*lline);
    break;
  case UNDEF:
    /* nothing */
    break;
  case JSCRIPT:
    if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
      *(*qq)++ = *(*lline);
    }
    break;
  case ASTYLE:
    if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
      *(*qq)++ = *(*lline);
    }
    break;
  }
}

/* Removes tags in the string - modifies in place 
 * the name of this function is a misnomer, since it doesn't
 * parse xml properly. 
 *
 * The filter can be called in several "modes" selected by the 
 * xml.parser variable. 
 *
 * The simplest parsing is xpDUMB mode, which simply skips XML like
 * tags without looking inside them.
 *
 * For HTML parsing, there is xpHTML mode, and its counterpart xpSMART
 * mode. xpHTML looks inside common HTML tags and can print the contents
 * of attributes. To explain xpSMART mode, remember that HTML documents
 * should normally be written inside <html> and </html> tags. If these 
 * tags are found, then everything outside them is handled by xpSMART mode.
 *
 * Thus, in particular, a new document should be started in xpSMART mode.
 * For text documents, this ensures that any preambles are not rendered,
 * until true HTML is encountered.
 *
 * However, there is a small problem, namely the <html> tags are
 * optional.  For text documents, missing <html> tags are rare, but
 * email often contains only fragments with <html> missing.  To cope
 * with this, in mail mode, xpSMART scans the current line and
 * switches immediately to xpHTML mode if the line is printable.
 *
 * This turns out to be an extremely important function, because
 * spammers don't always label attachments correctly. So it's possible
 * to get a binary stream labeled as text/html, and of course lots of
 * junk tokens. If xpSMART mode detects binary, then it does NOT
 * switch to xpHTML mode immediately, and nothing gets printed.  If
 * and when a valid <html> tag is found later, HTML will be enabled as
 * necessary. I think this is a robust partial solution to an
 * intractable problem.
 */
void mbw_prefix(xml_character_filter)(XML_State *xml, mbw_t *line) {
  mbw_t *q;
  q = line;
/*   int k; */

  /* don't call this with y < 1 */
#define TAGMATCH(x,y) (!mbw_strncasecmp(line + 1, x + 1, y - 1) && (mbw_isspace(line[y]) || (line[y] == mbw_lit('>')) || (line[y] == mbw_lit('\0'))) && (line += (y - 1)))

#define ATTRMATCH(x,y) (!mbw_strncasecmp(line + 1, x + 1, y - 1) && (mbw_isspace(line[y]) || (line[y] == mbw_lit('=')) || (line[y] == mbw_lit('\0'))) && (line += (y - 1)))

  /* this is convenient for debugging */
#define PDEBUG(x) printf(#x"{%c%c%c%c%c%c%c%c%c%c}\n", line[0], line[1], line[2], line[3], line[4], line[5], line[6], line[7], line[8], line[9])
#define PDEBUG2(x,y) {printf(#x"2{"); for(k = 0; k < y; k++) printf("%c",line[k]); printf("}\n"); }

  /* this is important - read comments above */
  if( (m_options & (1<<M_OPTION_MBOX_FORMAT)) && 
      (xml->parser == xpSMART) && 
      !mbw_prefix(is_binline)(line) &&
      !mbw_prefix(is_emptyspace)(line) ) {
    xml->parser = xpHTML;
  }

  while( *line ) {
/*     printf("%d %d ->", xml->state, xml->attribute); */
/*     PDEBUG(LINE); */
    switch(xml->state) {
    case TEXT:
      switch(line[0]) {
      case mbw_lit('<'):
	/* does it look like <x where x is either alpha or punctuation? */
	if( mbw_isalpha(line[1]) ) {
	  line++;
	  /* tags aren't mined, xtags are */
	  xml->state = TAG; 
	  xml->attribute = UNDEF;
	  switch(mbw_tolower(line[0])) {
	  case mbw_lit('a'):
	    if( (line[1] == mbw_lit('\0')) || mbw_isspace(line[1]) || 
		TAGMATCH(mbw_lit("area"),4) ||
		TAGMATCH(mbw_lit("applet"),6) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('b'):
	    if( TAGMATCH(mbw_lit("base"),4) ||
		TAGMATCH(mbw_lit("bgsound"),7) ) {
	      xml->state = XTAG;
	    } else if( TAGMATCH(mbw_lit("br"),2) ) {
	      *q++ = mbw_lit('\n');
	      xml->state = TAG;
	    } else if( TAGMATCH(mbw_lit("body"),4) ) {
	      xml->hide = VISIBLE;
	      if( xml->parser == xpSMART ) {
		xml->parser = xpHTML;
	      }
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('c'):
	    if( TAGMATCH(mbw_lit("comment"),7) ) {
	      if( xml->parser == xpHTML ) {
		xml->hide = COMMENT;
	      }
	    }
	    break;
	  case mbw_lit('d'):
	    if( TAGMATCH(mbw_lit("div"),3) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('e'):
	    if( TAGMATCH(mbw_lit("embed"),5) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('f'):
	    if( TAGMATCH(mbw_lit("frame"),5) ||
		TAGMATCH(mbw_lit("form"),4) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('h'):
	    if( TAGMATCH(mbw_lit("html"),4) || TAGMATCH(mbw_lit("head"),4) ) {
	      xml->hide = VISIBLE;
	      if( xml->parser == xpSMART ) {
		xml->parser = xpHTML;
	      }
	    } else if( TAGMATCH(mbw_lit("hr"),2) ) {
	      *q++ = mbw_lit('\n');
	      xml->state = TAG;
	    }
	    break;
	  case mbw_lit('i'):
	    if( TAGMATCH(mbw_lit("img"),3) ||
		TAGMATCH(mbw_lit("iframe"),6) ||
		TAGMATCH(mbw_lit("ilayer"),6) ||
		TAGMATCH(mbw_lit("input"),5) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('l'):
	    if( TAGMATCH(mbw_lit("layer"),5) ||
		TAGMATCH(mbw_lit("link"),4) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('n'):
	    if( (TAGMATCH(mbw_lit("noframes"),8) && (xml->hide = NOFRAMES)) ||
		(TAGMATCH(mbw_lit("nolayer"),7) && (xml->hide = NOLAYER)) ||
		(TAGMATCH(mbw_lit("noscript"),8) && (xml->hide = NOSCRIPT)) ||
		(TAGMATCH(mbw_lit("noembed"),7) && (xml->hide = NOEMBED)) ) {
	      if( xml->parser == xpHTML ) {
		if( (m_options & (1<<M_OPTION_SHOW_ALT)) ) {		  
		  xml->hide = VISIBLE; 
		}
	      }
	    }
	    break;
	  case mbw_lit('o'):
	    if( TAGMATCH(mbw_lit("object"),6) ) {
	      xml->state = XTAG;
	    }
	    break;
	  case mbw_lit('s'):
	    if( TAGMATCH(mbw_lit("span"),4) ) {
	      xml->state = XTAG;
	    } else if( TAGMATCH(mbw_lit("script"),6) ) {
	      xml->hide = SCRIPT;
	    } else if( TAGMATCH(mbw_lit("style"),5) ) {
	      xml->hide = STYLE;
	    }
	    break;
	  case mbw_lit('t'):
	    if( TAGMATCH(mbw_lit("title"),5) ) {
		xml->hide = TITLE;
	    }
	    break;
	  default:
	    /* ignore, ie it's a TAG */
	    break;
	  }
	} else if( line[1] == mbw_lit('/') ) {
	  line++;
	  /* tags aren't mined, xtags are */
	  xml->state = TAG; 
	  xml->attribute = UNDEF;
	  switch(mbw_tolower(line[1])) {
	  case mbw_lit('b'):
	    if( TAGMATCH(mbw_lit("/body"),5) ) {
	      if( xml->parser == xpHTML ) {
		xml->parser = xpSMART; 
	      }
	      xml->hide = VISIBLE;
	    }
	    break;
	  case mbw_lit('c'):
	    if( (xml->hide == COMMENT) && TAGMATCH(mbw_lit("/comment"),8) ) {
	      /* nothing */
	    }
	    break;
	  case mbw_lit('h'):
	    if( TAGMATCH(mbw_lit("/html"),5) || TAGMATCH(mbw_lit("/head"),5) ) {
	      if( xml->parser == xpHTML ) {
		xml->parser = xpSMART; 
	      }
	      xml->hide = VISIBLE;
	    }
	    break;
	  case mbw_lit('n'):
	    if( ((xml->hide == NOFRAMES) && TAGMATCH(mbw_lit("/noframes"),9)) ||
		((xml->hide == NOSCRIPT) && TAGMATCH(mbw_lit("/noscript"),9)) ||
		((xml->hide == NOLAYER) && TAGMATCH(mbw_lit("/nolayer"),8)) ||
		((xml->hide == NOEMBED) && TAGMATCH(mbw_lit("/noembed"),8)) ) {
	      xml->hide = VISIBLE;
	    }
	    break;
	  case mbw_lit('s'):
	    if( ((xml->hide == SCRIPT) && TAGMATCH(mbw_lit("/script"),7)) ||
		((xml->hide == STYLE) && TAGMATCH(mbw_lit("/style"),6)) ) {
	      xml->hide = VISIBLE;
	    }
	    break;
	  case mbw_lit('t'):
	    if( TAGMATCH(mbw_lit("/title"),6) ) {
	      xml->hide = VISIBLE;
	    }
	    break;
	  default:
	    /* ignore, ie it's a TAG */
	    break;
	  }
	} else { /* second char is not alpha or slash */
	  if( mbw_strncmp(line + 1, mbw_lit("!--"), 3) == 0 ) {
	    if( line[4] == mbw_lit('>') ) {
	      /* buggy MSHTML accepts <!--> as a comment, so ignore this */
	      line += 4;
	    } else { /* real comment */
	      xml->state = CMNT;
	      line += 3;
	    }
	  } else if( line[1] == mbw_lit('<') ) {
	    /* stay in TEXT state */
	    if( xml->parser == xpDUMB ) {
	      line++;
	    } else {
	      switch(xml->hide) {
	      case VISIBLE:
	      case TITLE:
		*q++ = *line;
		break;
	      case SCRIPT:
		if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
		  *q++ = *line;
		}
		break;
	      case STYLE:
		if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
		  *q++ = *line;
		}
		break;
	      default:
		break;
	      }
	    }
	  } else {
	    /* bogus tag? */
	    xml->state = TAG;
	    line++;
	  }
	}
	break;
      case mbw_lit('&'):
	if( (xml->parser == xpHTML) || (xml->parser == xpDUMB) ) {
	  switch(xml->hide) {
	  case VISIBLE:
	  case TITLE:
	    mbw_prefix(decode_html_entity)(&line, &q); 
	    break;
	  case SCRIPT:
	    if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
	      mbw_prefix(decode_html_entity)(&line, &q); 
	    }
	    break;
	  case STYLE:
	    if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
	      mbw_prefix(decode_html_entity)(&line, &q); 
	    }
	    break;
	  default:
	    break;
	  }
	}
	break;
      default:
	if( (xml->parser == xpHTML) || (xml->parser == xpDUMB) ) {
	  switch(xml->hide) {
	  case VISIBLE:
	  case TITLE:
	    *q++ = *line;
	    break;
	  case SCRIPT:
	    if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
	      *q++ = *line;
	    }
	    break;
	  case STYLE:
	    if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
	      *q++ = *line;
	    }
	    break;
	  default:
	    break;
	  }
	}
	break;
      }
      break;
    case TAG:
      if( line[0] == mbw_lit('>') ) {
	xml->state = TEXT;
      } else if( line[0] == mbw_lit('=') ) {
	xml->state = TAGPREQ;
      }
      break;
    case TAGPREQ:
      if( line[0] == mbw_lit('\'') ) {
	xml->state = TAGQUOTE;
      } else if( line[0] == mbw_lit('"') ) {
	xml->state = TAGDQUOTE;
      } else if( !mbw_isspace(line[0]) ) {
	xml->state = TAG;
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
      }
      break;
    case TAGQUOTE:
      if( (line[0] == '\\') && (line[1] == mbw_lit('\'')) ) {
	line++;
      } else if( line[0] == mbw_lit('\'') ) {
	xml->state = TAG;
      }
      break;
    case TAGDQUOTE:
      if( (line[0] == '\\') && (line[1] == mbw_lit('"')) ) {
	line++;
      } else if( line[0] == mbw_lit('"') ) {
	xml->state = TAG;
      }
      break;
    case XTAG:
      if( xml->parser == xpSMART ) { 
	/* we've recognized an HTML tag */
	xml->parser = xpHTML; 
      }
      if( (xml->attribute != UNDEF) && 
	  mbw_isspace(line[0]) &&
	  !MBW_EMPTYLINE(line) ) {
	xml->attribute = UNDEF;
	*q++ = ATTRIBSEP;
      } else
      switch(mbw_tolower(line[0])) {
      case mbw_lit('>'):
	xml->state = TEXT;
	if( xml->attribute != UNDEF ) {
	  xml->attribute = UNDEF;
	  *q++ = ATTRIBSEP;
	}
	break;
      case mbw_lit('='):
	xml->state = XTAGPREQ;
	break;
      case mbw_lit('a'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_ALT) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("alt"), 3) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = ALT;
	    line += 2;
	    *q++ = ATTRIBSEP;
	  } 
	}
	if( m_options & (1<<M_OPTION_SHOW_FORMS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("action"), 6) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 5;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('c'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("class"), 5) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = ASTYLE;
	    line += 4;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('d'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_LINKS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("data"), 4) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 3;
	    *q++ = ATTRIBSEP;
	  }
	}
	break;
      case mbw_lit('h'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_LINKS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("href"), 4) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 3;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('o'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( mbw_tolower(line[1]) != mbw_lit('n') ) {
	  /* false alarm */
	} else if( ATTRMATCH(mbw_lit("onmousedown"), 11) ||
		   ATTRMATCH(mbw_lit("onmousemove"), 11) ||
		   ATTRMATCH(mbw_lit("onmouseout"), 10) ||
		   ATTRMATCH(mbw_lit("onmouseover"), 11) ||
		   ATTRMATCH(mbw_lit("onmouseup"), 9) ||

		   ATTRMATCH(mbw_lit("onclick"), 7) ||
		   ATTRMATCH(mbw_lit("ondblclick"), 10) ||
		   ATTRMATCH(mbw_lit("onfocus"), 7) ||

		   ATTRMATCH(mbw_lit("onkeydown"), 9) ||
		   ATTRMATCH(mbw_lit("onkeypress"), 10) ||
		   ATTRMATCH(mbw_lit("onkeyup"), 7) ||

		   ATTRMATCH(mbw_lit("ondataavailable"), 15) ||
		   ATTRMATCH(mbw_lit("ondatasetchanged"), 16) ||
		   ATTRMATCH(mbw_lit("ondatasetcomplete"), 17) ||

		   ATTRMATCH(mbw_lit("onabort"), 7) ||
		   ATTRMATCH(mbw_lit("onload"), 6) ||
		   ATTRMATCH(mbw_lit("onunload"), 8) ||
		   ATTRMATCH(mbw_lit("onmove"), 6) ||
		   ATTRMATCH(mbw_lit("onresize"), 8) ||
		   ATTRMATCH(mbw_lit("onsubmit"), 8) ) {
	  if( xml->attribute != UNDEF ) {
	    *q++ = ATTRIBSEP;
	  }

	  xml->attribute = JSCRIPT;
	  *q++ = ATTRIBSEP;
	  /* line is already updated by ATTRMATCH */
	}
	break;
      case mbw_lit('s'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_LINKS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("src"), 3) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 2;
	    *q++ = ATTRIBSEP;
	  } 
	}
	if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("style"), 5) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = ASTYLE;
	    line += 4;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('t'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_ALT) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("title"), 5) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = ALT;
	    line += 4;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('u'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_LINKS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("urn"), 3) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 2;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      case mbw_lit('v'):
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( m_options & (1<<M_OPTION_SHOW_FORMS) ) {
	  if( !mbw_strncasecmp(line, mbw_lit("value"), 5) ) {
	    if( xml->attribute != UNDEF ) {
	      *q++ = ATTRIBSEP;
	    }
	    xml->attribute = SRC;
	    line += 4;
	    *q++ = ATTRIBSEP;
	  } 
	}
	break;
      default:
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	break;
      }
      break;
    case XTAGPREQ:
      if( line[0] == mbw_lit('\'') ) {
	xml->state = XTAGQUOTE;
      } else if( line[0] == mbw_lit('"') ) {
	xml->state = XTAGDQUOTE;
      } else if( !mbw_isspace(line[0]) ) {
	xml->state = XTAG;
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
      }
      break;
    case XTAGQUOTE:
      if( line[0] == mbw_lit('\'') ) {
	xml->state = XTAG;
	if( xml->attribute != UNDEF ) {
	  *q++ = ATTRIBSEP;
	}
	xml->attribute = UNDEF;
      } else {
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( (line[0] == mbw_lit('\\')) && (line[1] == mbw_lit('\'')) ) {
	  line++;
	  mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	}
      }
      break;
    case XTAGDQUOTE:
      if( line[0] == mbw_lit('"') ) {
	xml->state = XTAG;
	if( xml->attribute != UNDEF ) {
	  *q++ = ATTRIBSEP;
	}
	xml->attribute = UNDEF;
      } else {
	mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	if( (line[0] == mbw_lit('\\')) && (line[1] == mbw_lit('"')) ) {
	  line++;
	  mbw_prefix(xml_attribute_filter)(xml, &line,&q);
	}
      }
      break;
    case CMNT:
      if( (line[0] == mbw_lit('-')) && (line[1] == mbw_lit('-')) ) {
	xml->state = TAG;
      } else {
	/* ignore comments in some circumstances */
	if( m_options & (1<<M_OPTION_SHOW_HTML_COMMENTS) ) {
	  *q++ = *line;
	} else switch(xml->hide) {
	case SCRIPT:
	  if( m_options & (1<<M_OPTION_SHOW_SCRIPT) ) {
	    *q++ = *line;
	  }
	  break;
	case STYLE:
	  if( m_options & (1<<M_OPTION_SHOW_STYLE) ) {
	    *q++ = *line;
	  }
	  break;
	default:
	  break;
	}
      }
      break;
    case DISABLED:
      /* don't modify the line at all */
      return;
    }
    line++;
  }
  *q = mbw_lit('\0'); /* mark the end of the clean text string */

}


/***********************************************************
 * TEXT PARSING FUNCTIONS                                  *
 ***********************************************************/

bool_t mbw_prefix(plain_text_filter)(MBOX_State *mbox, mbw_t *line) {
  mbw_t *q;
  bool_t url = 0;
  bool_t censor = 0;


  switch(mbox->plainstate) {
  case psPLAIN:
    if( (line[0] == mbw_lit('b')) && 
	!mbw_strncmp(line, mbw_lit("begin "),6) && 
	ISOCT(line[6]) && ISOCT(line[7]) && ISOCT(line[8]) ) {
      mbox->plainstate = psUUENCODE;
      return 1;
    }
    break;
  case psUUENCODE:
    switch(mbw_prefix(is_uuline)(line)) {
    case -1:
      return 1;
    case -2:
      mbox->plainstate = psPLAIN;
      break;
    default:
      return 0;
    }
    break;
  }

  /* now assume psPLAIN */

  q = line;
  while(*line) {
    switch(*line) {
    case mbw_lit('%'):
      if( !censor ) {
	mbw_prefix(decode_uri_character)(&line, &q);
      }
      break;
    case mbw_lit('&'):
      if( !censor ) {
	if( !url ) {
	  mbw_prefix(decode_html_entity)(&line, &q);
	} else {
	  censor = 1;
	}
      }
      break;
    case mbw_lit('H'):
    case mbw_lit('h'):
      if( !mbw_strncasecmp(line, mbw_lit("http://"), 7) ) {
	censor = 0;
	url = 1;
      }
      if( !censor ) { *q++ = *line; }
      break;
    case mbw_lit('?'):
    case mbw_lit(';'):
    case mbw_lit('#'):
      if( url ) { 
	censor = 1; 
      } else {
	*q++ = *line;
      }
      break;
    case mbw_lit(' '):
    case mbw_lit('\t'):
    case mbw_lit('>'):
    case mbw_lit('\''):
    case mbw_lit('"'):
      if( censor && url ) { censor = 0; url = 0; }
      *q++ = *line;
      break;
    default:
      if( !censor ) { *q++ = *line; }
      break;
    }
    line++;
  }
  *q = mbw_lit('\0');

  return 1;
}


/* assume string is binary with embedded NULs replaced by FFs, 
 the strings that are found are separated by spaces */
/* note: this doesn't work like strings(1) yet, but eventually it will ;-) */
bool_t mbw_prefix(strings1_filter)(mbw_t *line) {
  size_t c;
  mbw_t *q;

#define MIN_STRING_SIZE 4
  for(q = line, c = 0; *line; line++) {
/*     if( mbw_isalnum(*line) ||  */
/* 	mbw_ispunct(*line) ||  */
/* 	(*line == mbw_lit(' ')) ) { */
    if( mbw_isprint(*line) && (*line != mbw_lit('\t')) ) {
      *q++ = *line;
      c++;
    } else if( c >= MIN_STRING_SIZE ) {
      *q++ = mbw_lit(' ');
      c = 0;
    } else if( c > 0 ) {
      q -= c;
      c = 0;
    }
  }
  *q = mbw_lit('\0');

  return 1;
}

/***********************************************************
 * CALLED OUTSIDE THIS SOURCE FILE                         *
 ***********************************************************/

void mbw_prefix(init_decoding_caches)(MBOX_State *mbox) {
  mbw_prefix(init_dc)(&(mbox->mbw_prefix(b64_dc)),system_pagesize);
  mbw_prefix(init_dc)(&(mbox->mbw_prefix(qp_dc)),system_pagesize);
}

void mbw_prefix(free_decoding_caches)(MBOX_State *mbox) {
  if( mbox->mbw_prefix(b64_dc).cache ) { 
    free(mbox->mbw_prefix(b64_dc).cache); 
    mbox->mbw_prefix(b64_dc).cache = NULL;
  }
  if( mbox->mbw_prefix(qp_dc).cache ) { 
    free(mbox->mbw_prefix(qp_dc).cache); 
    mbox->mbw_prefix(qp_dc).cache = NULL;
  }
}

#endif


