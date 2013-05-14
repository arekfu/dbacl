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

/* don't guard this file, it must be parsed differently several times */

#if defined MBW_WIDE

#define mbw_lit(x) L##x
#define mbw_t wchar_t
#define mbw_prefix(f) w_##f
#define mbw_isalnum(c) iswalnum(c)
#define mbw_isalpha(c) iswalpha(c)
#define mbw_isgraph(c) iswgraph(c)
#define mbw_isspace(c) iswspace(c)
#define mbw_isblank(c) iswblank(c)
#define mbw_iscntrl(c) iswcntrl(c)
#define mbw_ispunct(c) iswpunct(c)
#define mbw_isxdigit(c) iswxdigit(c)
#define mbw_isdigit(c) iswdigit(c)
#define mbw_isprint(c) iswprint(c)
#define mbw_isascii(c) (c <= 127)
#define mbw_strncmp(x,y,z) wcsncmp(x,y,z)
#define mbw_strcmp(x,y) wcscmp(x,y)
#define mbw_strncasecmp(x,y,z) mbw_prefix(mystrncasecmp)(x,y,z) /* wcsncasecmp is broken in glibc */
#define mbw_strtol(x,y,z) wcstol(x,y,z)
#define mbw_tolower(x) towlower(x)
#define mbw_strchr(x,y) wcschr(x,y)
#define mbw_strncpy(x,y,z) wcsncpy(x,y,z)
#define mbw_strlen(x) wcslen(x)

#if defined HAVE_MBRTOWC
static mbstate_t copychar_shiftstate;
static charbuf_len_t copychar_len;
#define mbw_copychar(x,y) { \
            copychar_len = wcrtomb((x), (y), &copychar_shiftstate); \
            if( copychar_len > -1) { \
               x += copychar_len; \
            } \
        }
#endif

/* regexes are disabled in wide character mode for now -
   there are too many issues: how does dbacl decide when 
   to use wide char regexes, and when to use multibyte regexes?
   Thsi must be consistent when -i switch can be anywhere on the 
   command line, when categories are loaded (even if one uses
   wide, another uses multibyte), etc. */
#define mbw_regcomp(x,y,z) (-1)
#define mbw_regexec(x,y,z,t,u) (-1)
#define mbw_regerror(x,y,z,t) (-1)
#define mbw_regfree(x) (-1)

#elif defined MBW_MB

#define mbw_lit(x) x
#define mbw_t char
#define mbw_prefix(f) f
#define mbw_isalnum(c) isalnum((int)(c))
#define mbw_isalpha(c) isalpha((int)(c))
#define mbw_isgraph(c) isgraph((int)(c))
#define mbw_isspace(c) isspace((int)(c))
#define mbw_isblank(c) isblank((int)(c))
#define mbw_iscntrl(c) iscntrl((int)(c))
#define mbw_ispunct(c) ispunct((int)(c))
#define mbw_isxdigit(c) isxdigit((int)(c))
#define mbw_isdigit(c) isdigit((int)(c))
#define mbw_isprint(c) isprint((int)(c))
#define mbw_isascii(c) ((int)c <= 127)
#define mbw_strncmp(x,y,z) strncmp(x,y,z)
#define mbw_strcmp(x,y) strcmp(x,y)
#define mbw_strncasecmp(x,y,z) strncasecmp(x,y,z)
#define mbw_strtol(x,y,z) strtol(x,y,z)
#define mbw_tolower(x) tolower(x)
#define mbw_strchr(x,y) strchr(x,y)
#define mbw_strncpy(x,y,z) strncpy(x,y,z)
#define mbw_strlen(x) strlen(x)
#define mbw_copychar(x,y) { *(x)++ = (y); }

#define mbw_regcomp(x,y,z) regcomp(x,y,z)
#define mbw_regexec(x,y,z,t,u) regexec(x,y,z,t,u) 
#define mbw_regerror(x,y,z,t) regerror(x,y,z,t) 
#define mbw_regfree(x) regfree(x) 

#else

#error "This file must be compiled with -DMBW_WIDE or -DMBW_P"

#endif


#define ISOCT(x) ((x <= mbw_lit('6')) && (x >= mbw_lit('0')))
#define ATTRIBSEP mbw_lit('\n')
