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

#ifndef DBACL_H
#define DBACL_H

#ifdef HAVE_CONFIG_H
#undef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef VERSION
#ifdef PACKAGE_VERSION
#define VERSION PACKAGE_VERSION
#endif
#endif

#define COPYBLURB "Copyright (c) 2002-2013 L.A. Breyer. All rights reserved.\n" \
                  "%s comes with ABSOLUTELY NO WARRANTY, and is licensed\n" \
	          "to you under the terms of the GNU General Public License 3 or later.\n\n"

#define DEFAULT_CATPATH "DBACL_PATH"
/* define this to save category files with a temporary name, then atomically
 * rename them. This makes corrupt category files nearly impossible, and 
 * obviates the need for file locking in case another instance of dbacl is
 * trying to read the category while it is being written.
 */
#define ATOMIC_CATSAVE
/* we give our files the 640 permissions - I've added "write"
   permission because sometimes we want to mmap/readwrite such files
   so we need those permissions. Also, O_BINARY is not portable, but
   a good idea for some platforms
 */
#ifndef O_BINARY
#define O_BINARY 0
#endif
#define ATOMIC_CREATE(x) open(x, O_CREAT|O_EXCL|O_RDWR|O_BINARY, 0640)

/* we define several memory models, which differ basically 
   in the number of bytes used for the hash tables. Adjust to taste */

/* use this for 64-bit hashes */
#undef HUGE_MEMORY_MODEL
/* use this for 32-bit hashes */
#define NORMAL_MEMORY_MODEL 
/* use this for 16-bit hashes */
#undef SMALL_MEMORY_MODEL
/* use this for 8-bit hashes */
#undef TINY_MEMORY_MODEL

/* the following defines set up a tradeoff between
   modelling accuracy and memory requirements - season to taste 
   (if you often get digitization errors, undef the appropriate macro) */

/* digram digitization: avg loss of precision = 0.01 * token size */
#define DIGITIZE_DIGRAMS
/* lambda digitization: avg loss of precision = 0.01 */
#define DIGITIZE_LAMBDA
/* learner.hash digitization: avg loss of precision = 0.01 */
#define DIGITIZE_LWEIGHTS
#if defined HAVE_MBRTOWC

#include <wctype.h>
#include <wchar.h>

#endif

#include <limits.h>
#include <stdio.h>


#if !defined LOADED_REGEX

#include <sys/types.h>
#include <regex.h>

#endif

#if defined HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifndef htonl
#define htonl(x) (x)
#define ntohl(x) (x)
#define htons(x) (x)
#define ntohs(x) (x)
#endif

#if defined OS_SUN
#include <ieeefp.h>
#endif

/* some systems seem to have broken sys/types */
#if defined OS_SUN || defined OS_HPUX
#include <inttypes.h>

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#endif

#ifdef HAVE_MMAP
#ifdef HAVE_MADVISE
#ifdef HAVE_SYS_MMAN_H

#include <sys/types.h>
#include <sys/mman.h>

#ifdef OS_SUN
#define MADVISE(x,y,z) madvise((caddr_t)(x),y,z)
#define MLOCK(x,y) mlock((caddr_t)(x),y)
#define MUNLOCK(x,y) munlock((caddr_t)(x),y)
#define MUNMAP(x,y) munmap((void *)(x),y)
#define MMAP(x,y,z,t,u,v) mmap((void *)(x),y,z,t,u,v)
#else
#define MADVISE(x,y,z) madvise(x,y,z)
#define MLOCK(x,y) mlock(x,y)
#define MUNLOCK(x,y) munlock(x,y)
#define MUNMAP(x,y) munmap((void *)(x), y)
#define MMAP(x,y,z,t,u,v) mmap((void *)(x),y,z,t,u,v)
#endif

#endif
#endif
#endif

#ifndef MADVISE
#define MAP_FAILED ((void *)-1)
#define MADVISE(x,y,z)
#define MLOCK(x,y)
#define MUNLOCK(x,y)
#define MUNMAP(x,y)
#define MMAP(x,y,z,t,u,v) NULL
#endif

/* constants used by mmap */
#ifndef PROT_READ
#define PROT_READ  0
#define PROT_WRITE 0
#define PROT_EXEC  0
#define PROT_NONE  0
#endif

#define PAGEALIGN(x) ((x) / system_pagesize) * system_pagesize

/* below, FMT_* macros are used in printf/scanf format strings */
#if defined HUGE_MEMORY_MODEL

typedef u_int64_t token_count_t;
typedef unsigned int token_order_t; /* used in bit-field, therefore uint */
typedef unsigned int token_class_t; /* used in bit-field, therefore uint */
typedef u_int8_t hash_bit_count_t;
typedef u_int64_t hash_count_t;
typedef unsigned int hash_percentage_t;
typedef u_int16_t category_count_t;
typedef u_int16_t regex_count_t;
typedef u_int64_t document_count_t;
typedef u_int16_t confidence_t;

typedef float weight_t;
typedef double score_t;
#define FMT_printf_score_t "f"
#define FMT_scanf_score_t "lf"
#define FMT_printf_integer_t "ld"

typedef u_int16_t token_stack_t;
typedef int charbuf_len_t;
typedef u_int16_t alphabet_size_t;
typedef u_int16_t smbitmap_t;
typedef u_int8_t regex_flags_t;

typedef int error_code_t;
typedef int bool_t;
typedef u_int8_t byte_t;

#if defined DIGITIZE_DIGRAMS && defined DIGITIZE_LAMBDA
/* cats not portable because hash value is too big */
#undef PORTABLE_CATS
#endif
/* keep typedefs and macros togegher */
typedef u_int64_t hash_value_t;
#define hton_hash_value_t(x) (x)
#define ntoh_hash_value_t(x) (x)
typedef u_int16_t digitized_weight_t;
#define hton_digitized_weight_t(x) (x)
#define ntoh_digitized_weight_t(x) (x)

/* where token counts wrap around */
#define K_TOKEN_COUNT_MAX ((token_count_t)18446744073709551615U)
/* where digrams wrap around */
#define K_DIGRAM_COUNT_MAX ((weight_t)1.0e+9)
/* size of hash in bits */
#define MAX_HASH_BITS ((hash_bit_count_t)64)
/* for line filtering: maximum number of tokens allowed on a single line */
#define MAX_TOKEN_LINE_STACK ((token_stack_t)16384) 
/* number of pages we want to use for I/O buffering */
#define BUFFER_MAG 64
/* we need 8 byte hash values */
#define JENKINS8
#undef  JENKINS4

#elif defined NORMAL_MEMORY_MODEL

typedef u_int32_t token_count_t;
typedef unsigned int token_order_t;/* used in bit-field, therefore uint */
typedef unsigned int token_class_t;/* used in bit-field, therefore uint */
typedef u_int8_t hash_bit_count_t;
typedef u_int32_t hash_count_t;
typedef unsigned int hash_percentage_t;
typedef u_int8_t category_count_t;
typedef u_int8_t regex_count_t;
typedef u_int32_t document_count_t;
typedef u_int16_t confidence_t;

typedef float weight_t;
typedef double score_t;
#define FMT_printf_score_t "f"
#define FMT_scanf_score_t "lf"
#define FMT_printf_integer_t "d"

typedef u_int8_t token_stack_t;
typedef int charbuf_len_t;
typedef u_int16_t alphabet_size_t;
typedef u_int16_t smbitmap_t;
typedef u_int8_t regex_flags_t;

typedef int error_code_t;
typedef int bool_t;
typedef u_int8_t byte_t;

#if defined DIGITIZE_DIGRAMS && defined DIGITIZE_LAMBDA && defined HAVE_NETINET_IN_H
#define PORTABLE_CATS
#endif
/* keep typedefs and macros togegher */
typedef u_int32_t hash_value_t;
#define hton_hash_value_t(x) htonl(x)
#define ntoh_hash_value_t(x) ntohl(x)
typedef u_int16_t digitized_weight_t;
#define hton_digitized_weight_t(x) htons(x)
#define ntoh_digitized_weight_t(x) ntohs(x)

/* where token counts wrap around */
#define K_TOKEN_COUNT_MAX ((token_count_t)4294967295U)
/* where digrams wrap around */
#define K_DIGRAM_COUNT_MAX ((weight_t)1.0e+9)
/* size of hash in bits */
#define MAX_HASH_BITS ((hash_bit_count_t)30)
/* for line filtering: maximum number of tokens allowed on a single line */
#define MAX_TOKEN_LINE_STACK ((token_stack_t)255) 
/* number of pages we want to use for I/O buffering */
#define BUFFER_MAG 32
/* we need 4 byte hash values */
#define  JENKINS8
#undef JENKINS4

#elif defined SMALL_MEMORY_MODEL

typedef u_int32_t token_count_t;
typedef unsigned int token_order_t;/* used in bit-field, therefore uint */
typedef unsigned int token_class_t;/* used in bit-field, therefore uint */
typedef u_int8_t hash_bit_count_t;
typedef u_int16_t hash_count_t;
typedef unsigned int hash_percentage_t;
typedef u_int8_t category_count_t;
typedef u_int8_t regex_count_t;
typedef u_int16_t document_count_t;
typedef u_int16_t confidence_t;

typedef float weight_t;
typedef double score_t;
#define FMT_printf_score_t "f"
#define FMT_scanf_score_t "lf"
#define FMT_printf_integer_t "d"

typedef u_int8_t token_stack_t;
typedef int charbuf_len_t;
typedef u_int16_t alphabet_size_t;
typedef u_int16_t smbitmap_t;
typedef u_int8_t regex_flags_t;

typedef int error_code_t;
typedef int bool_t;
typedef u_int8_t byte_t;

#if defined DIGITIZE_DIGRAMS && defined DIGITIZE_LAMBDA && defined HAVE_NETINET_IN_H
#define PORTABLE_CATS
#endif
/* keep typedefs and macros togegher */
typedef u_int16_t hash_value_t;
#define hton_hash_value_t(x) htons(x)
#define ntoh_hash_value_t(x) ntohs(x)
typedef u_int16_t digitized_weight_t;
#define hton_digitized_weight_t(x) htons(x)
#define ntoh_digitized_weight_t(x) ntohs(x)

/* where token counts wrap around */
#define K_TOKEN_COUNT_MAX ((token_count_t)4294967295U)
/* where digrams wrap around */
#define K_DIGRAM_COUNT_MAX ((weight_t)1.0e+9)
/* size of hash in bits */
#define MAX_HASH_BITS ((hash_bit_count_t)15)
/* for line filtering: maximum number of tokens allowed on a single line */
#define MAX_TOKEN_LINE_STACK ((token_stack_t)128) 
/* number of pages we want to use for I/O buffering */
#define BUFFER_MAG 16
/* we need 4 byte hash values */
#undef  JENKINS8
#define JENKINS4

#elif defined TINY_MEMORY_MODEL
/* not tested, this model probably doesn't work ;-) */
#undef DIGITIZE_DIGRAMS

typedef u_int32_t token_count_t;
typedef unsigned int token_order_t;/* used in bit-field, therefore uint */
typedef unsigned int token_class_t;/* used in bit-field, therefore uint */
typedef u_int8_t hash_bit_count_t;
typedef u_int8_t hash_count_t;
typedef unsigned int hash_percentage_t;
typedef u_int8_t category_count_t;
typedef u_int8_t regex_count_t;
typedef u_int8_t document_count_t;
typedef u_int16_t confidence_t;

typedef float weight_t;
typedef double score_t;
#define FMT_printf_score_t "f"
#define FMT_scanf_score_t "lf"
#define FMT_printf_integer_t "d"

typedef u_int8_t token_stack_t;
typedef int charbuf_len_t;
typedef u_int16_t alphabet_size_t;
typedef u_int16_t smbitmap_t;
typedef u_int8_t regex_flags_t;

typedef int error_code_t;
typedef int bool_t;
typedef u_int8_t byte_t;

#if defined DIGITIZE_DIGRAMS && defined DIGITIZE_LAMBDA
#undef PORTABLE_CATS
#endif
/* keep typedefs and macros togegher */
typedef u_int8_t hash_value_t;
#define hton_hash_value_t(x) (x)
#define ntoh_hash_value_t(x) (x)
typedef u_int16_t digitized_weight_t;
#define hton_digitized_weight_t(x) htons(x)
#define ntoh_digitized_weight_t(x) ntohs(x)

#define K_TOKEN_COUNT_MAX ((token_count_t)4294967295U)
/* where digrams wrap around */
#define K_DIGRAM_COUNT_MAX ((weight_t)1.0e+9)
/* size of hash in bits */
#define MAX_HASH_BITS ((hash_bit_count_t)8)
/* for line filtering: maximum number of tokens allowed on a single line */
#define MAX_TOKEN_LINE_STACK ((token_stack_t)128) 
/* number of pages we want to use for I/O buffering */
#define BUFFER_MAG 2
/* we need 4 byte hash values */
#undef  JENKINS8
#define JENKINS4

#endif

/* this is common to all memory models */

#if defined OS_DARWIN
/* the system I tested this on didn't seem to like packed structures */
#define PACK_STRUCTS

#else

/* disable this if speed is paramount  */
#if defined __GNUC__
#define PACK_STRUCTS __attribute__ ((packed))
#else
#define PACK_STRUCTS
#endif

#endif

/* when digitizing transitions, this stands for -infinity */
#define DIGITIZED_WEIGHT_MIN ((digitized_weight_t)0)
#define DIGITIZED_WEIGHT_MAX ((digitized_weight_t)USHRT_MAX)
#define DIG_FACTOR           5
/* maximum number of categories we can handle simultaneously */
#define MAX_CAT ((category_count_t)64)
/* percentage of hash we use */
#define HASH_FULL ((hash_percentage_t)95)
/* alphabet size */
#define ASIZE ((alphabet_size_t)256)
/* we need three special markers, which cannot be part 
 * of the alphabet. Fortunately, we can use ASCII control
 * characters. Hopefully, these won't be used for anything important. 
 * Make sure AMIN equals DIAMOND is the last reserved char.
 */
#define TOKENSEP '\001'
#define CLASSEP '\002'
#define DIAMOND '\003'
#define AMIN DIAMOND
#define EOTOKEN CLASSEP
/* enough room to pad token with NULL, DIAMOND, CLASSEP and class */
#define EXTRA_CLASS_LEN 2
#define EXTRA_TOKEN_LEN (EXTRA_CLASS_LEN + 2)
#define MULTIBYTE_EPSILON 10 /* enough for a multibyte char and a null char */

/* make sure a character is in the alphabet range */
#define CLIP_ALPHABET(x) x = (((unsigned char)x) < AMIN) ? AMIN : (x)
/* the space outside of AMIN-ASIZE is used for auxiliary RESERVED_* data */
#define RESERVED_UNUSED0  0 /* dig[0][0-255] */
#define RESERVED_MARGINAL 2 /* dig[2][0-255] counts single char marginal freqs */
#define RESERVED_TOKLEN   1 /* dig[1][0-MAX_TOKEN_LEN] counts token lengths */

/* decides how we compute the shannon entropy */
#undef SHANNON_STIRLING

/* maximum size of a token, beyond that rest is ignored (ie put into
 * another token) The value should not be too big, because it protects
 * against extreme probabilities failing to digitize properly. 
 *
 * Here's the back-of-the-envelope calculation: In the model, for each
 * token, we save the reference weight, and the lambda weight. It
 * seems that the lambdas are of the same order as the corresponding
 * n-gram's reference weight,
 *
 * The reference weights are most extreme for the uniform, equal to
 * about -5 per token character. Thus, for an n-gram lambda weight,
 * the most extreme values are about (-5) * total number of
 * characters. Our calculations blow up for n >= 6 anyway, so the most
 * extreme value in the worst case (=7) should be about (-5) *
 * MAX_TOKEN_LEN * 7.
 *
 * The other constraint is that we must be able to digitize the
 * weights, and with DIG_FACTOR = 5, the extreme weight values can be
 * up to 2048. Giving us a margin of error, we assume that 35 *
 * MAX_TOKEN_LEN < 1024, which gives MAX_TOKEN_LEN = 30.
 */
#define MAX_TOKEN_LEN ((charbuf_len_t)30) 
#define TOKEN_LIST_GROW 1048576L

/* user options */
#define U_OPTION_CLASSIFY               1
#define U_OPTION_LEARN                  2
#define U_OPTION_FASTEMP                3
#define U_OPTION_CUTOFF                 4
#define U_OPTION_VERBOSE                5
#define U_OPTION_STDIN                  6
#define U_OPTION_SCORES                 7
#define U_OPTION_POSTERIOR              8
#define U_OPTION_FILTER                 9
#define U_OPTION_DEBUG                  10
#define U_OPTION_DUMP                   12
#define U_OPTION_APPEND                 13
#define U_OPTION_DECIMATE               14
#define U_OPTION_GROWHASH               15
#define U_OPTION_INDENTED               16
#define U_OPTION_NOZEROLEARN            17
#define U_OPTION_MMAP                   21
#define U_OPTION_CONFIDENCE             22
#define U_OPTION_VAR                    23
#define U_OPTION_HM_ADDRESSES           24
#define U_OPTION_CLASSIFY_MULTIFILE     25
#define U_OPTION_PRIOR_CORRECTION       26
#define U_OPTION_MEDIACOUNTS            27

/* model options */
#define M_OPTION_REFMODEL               1
#define M_OPTION_TEXT_FORMAT            2
#define M_OPTION_MBOX_FORMAT            3
#define M_OPTION_XML                    4
#define M_OPTION_I18N                   5
#define M_OPTION_CASEN                  6
#define M_OPTION_CALCENTROPY            7
#define M_OPTION_MULTINOMIAL            8
#define M_OPTION_HEADERS                13
#define M_OPTION_PLAIN                  14
#define M_OPTION_NOPLAIN                15
#define M_OPTION_SHOW_LINKS             16
#define M_OPTION_SHOW_ALT               17
#define M_OPTION_HTML                   18
#define M_OPTION_XHEADERS               19
#define M_OPTION_SHOW_SCRIPT            21
#define M_OPTION_SHOW_HTML_COMMENTS     22
#define M_OPTION_USE_STDTOK             23
#define M_OPTION_ATTACHMENTS            24
#define M_OPTION_WARNING_BAD            25
#define M_OPTION_SHOW_STYLE             26
#define M_OPTION_SHOW_FORMS             28
#define M_OPTION_NOHEADERS              29
#define M_OPTION_NGRAM_STRADDLE_NL      30
#define M_OPTION_THEADERS               31

/* category options */
#define C_OPTION_MMAPPED_HASH            1


typedef u_int32_t options_t; /* make sure big enough for all options */
typedef enum {
  DT_DEFAULT=0,
  DT_UNIFORM, DT_DIRICHLET, DT_MAXENT, DT_MLE, DT_IID
} digtype_t;
typedef enum {
  CP_DEFAULT=0, 
  CP_CHAR, CP_ALPHA, CP_ALNUM, CP_GRAPH, 
  CP_CEF, CP_ADP, CP_CEF2
} charparser_t;
#define FMT_printf_options_t "d"
#define FMT_scanf_options_t "ld"

typedef long int re_bitfield;
#define MAX_RE ((regex_count_t)(8 * sizeof(re_bitfield)))
#define INVALID_RE 0
/* maximum number of tagged subexpressions we can handle for each regex */
#define MAX_SUBMATCH ((token_order_t)9)

typedef enum {gcUNDEF = 0, gcDISCARD, gcTOKEN, gcTOKEN_END, gcIGNORE} good_char_t;


/* macros */

/* used for digitizing */
#if defined DIGITIZE_LWEIGHTS

/* use this when digitizing positive weights */
#define PACK_LWEIGHTS(a) ((digitized_weight_t)digitize_a_weight(a,1))
#define UNPACK_LWEIGHTS(a) ((weight_t)undigitize_a_weight(a,1))

/* use this when digitizing negative weights */
#define PACK_RWEIGHTS(a) ((digitized_weight_t)digitize_a_weight(-(a),1))
#define UNPACK_RWEIGHTS(a) (-(weight_t)undigitize_a_weight(a,1))

#define DW "w"

#else

#define PACK_LWEIGHTS(a) ((weight_t)(a))
#define UNPACK_LWEIGHTS(a) ((weight_t)(a))

#define PACK_RWEIGHTS(a) ((weight_t)(a))
#define UNPACK_RWEIGHTS(a) ((weight_t)(a))

#define DW ":"

#endif

#if defined DIGITIZE_LAMBDA

#define PACK_LAMBDA(a) ((digitized_weight_t)digitize_a_weight(a,1))
#define UNPACK_LAMBDA(a) ((weight_t)undigitize_a_weight(a,1))
#define DL "l"

#else

#define PACK_LAMBDA(a) ((weight_t)(a))
#define UNPACK_LAMBDA(a) ((weight_t)(a))
#define DL ":"

#endif

#if defined DIGITIZE_DIGRAMS

#define PACK_DIGRAMS(a) ((digitized_weight_t)digitize_a_weight(-(a),1))
#define UNPACK_DIGRAMS(a) (-(weight_t)undigitize_a_weight(a,1))
#define SIZEOF_DIGRAMS (sizeof(digitized_weight_t))
#define DD "d"

#else

#define PACK_DIGRAMS(a) ((weight_t)(a))
#define UNPACK_DIGRAMS(a) ((weight_t)(a))
#define SIZEOF_DIGRAMS (sizeof(weight_t))
#define DD ":"

#endif

#define CLIP_LAMBDA_TOL(x) (x < 1.0/(1<<DIG_FACTOR) ? 1.0/(1<<DIG_FACTOR) : x)

/* used in hash code */

#define FILLEDP(a) ((a)->id)
#define EQUALP(a,b) ((a)==(b))
#define SET(a,b) (a = (b))

#define SETMARK(a) ((a)->typ.mark = (unsigned int)1)
#define UNSETMARK(a) ((a)->typ.mark = (unsigned int)0)
#define MARKEDP(a) ((a)->typ.mark == (unsigned int)1)

#define NOTNULL(x) ((x) > 0)

#define MAXIMUM(x,y) (((x)<(y))?(y):(x))
#define INCREMENT(x,y,z) if( (x) < (y) ) { (x)++; } else { z = 1; }
#define INCREASE(x,d,y,z) if( (x) < ((y)-(d)) ) { (x) += (d); } else { z = 1; }

#if defined PORTABLE_CATS
#define SIGNATURE VERSION " " DD DL DW " " "portable"

#define NTOH_ID(x)      ntoh_hash_value_t(x)
#define HTON_ID(x)      hton_hash_value_t(x)

#define NTOH_DIGRAM(x)  ntoh_digitized_weight_t(x)
#define HTON_DIGRAM(x)  hton_digitized_weight_t(x)

#define NTOH_LAMBDA(x)  ntoh_digitized_weight_t(x)
#define HTON_LAMBDA(x)  hton_digitized_weight_t(x)

#else
#define SIGNATURE VERSION " " DD DL DW " " TARGETCPU

#define NTOH_ID(x)      (x)
#define HTON_ID(x)      (x)

#define NTOH_DIGRAM(x)  (x)
#define HTON_DIGRAM(x)  (x)

#define NTOH_LAMBDA(x)  (x)
#define HTON_LAMBDA(x)  (x)

#endif

/* used by both category load and learner save functions */
#define MAGIC_BUFSIZE 512
#define MAGIC1    "# dbacl " SIGNATURE " category %s %s\n"
#define MAGIC1_LEN (17 + strlen(SIGNATURE))
#define MAGIC2_i  "# entropy %" FMT_scanf_score_t \
                  " logZ %" FMT_scanf_score_t " max_order %hd" \
                  " type %s\n"
#define MAGIC2_o  "# entropy %" FMT_printf_score_t \
                  " logZ %" FMT_printf_score_t " max_order %hd" \
                  " type %s\n"
#define MAGIC3    "# hash_size %hd" \
                  " features %ld unique_features %ld" \
                  " documents %ld\n"
#define MAGIC4_i  "# options %" FMT_scanf_options_t " %hd %hd (%s)\n"
#define MAGIC4_o  "# options %" FMT_printf_options_t " %hd %hd (%s)\n"
#define MAGIC5_i  "# regex %s\n"
#define MAGIC5_o  "# regex %s||%s\n"
#define MAGIC5_wo "# regex %ls||%s\n"
#define MAGIC7_i  "# antiregex %s\n"
#define MAGIC7_o  "# antiregex %s||%s\n"
#define MAGIC7_wo "# antiregex %ls||%s\n"
#define MAGIC9    "# min_feature_count %ld max_feature_count %ld\n" 
#define RESTARTPOS 8
#define MAGIC6    "#\n"
#define MAGIC8_i  "# shannon %" FMT_scanf_score_t \
                  " shannon_s2 %" FMT_scanf_score_t "\n"
#define MAGIC8_o  "# shannon %" FMT_printf_score_t \
                  " shannon_s2 %" FMT_printf_score_t "\n"
#define MAGIC10_i "# alpha %" FMT_scanf_score_t \
                  " beta %" FMT_scanf_score_t \
                  " mu %" FMT_scanf_score_t \
                  " s2 %" FMT_scanf_score_t "\n"
#define MAGIC10_o "# alpha %" FMT_printf_score_t \
                  " beta %" FMT_printf_score_t \
                  " mu %" FMT_printf_score_t \
                  " s2 %" FMT_printf_score_t "\n"
#define MAGIC11   "# medialp "

#define MAGIC_ONLINE "# dbacl " SIGNATURE " online memory dump\n"

#define MAGIC_DUMP "# lambda | dig_ref | count | id     | token\n"
#define MAGIC_DUMPTBL_o "%9.3f %9.3f %7" FMT_printf_integer_t " %8lx "
#define MAGIC_DUMPTBL_i "%f %f %d %lx "

/* data structures */
#define TOKEN_CLASS_MAX 16
#define TOKEN_ORDER_MAX 8
typedef struct {
  token_class_t cls: 4;
  token_order_t order: 3;
  unsigned int mark: 1;
} PACK_STRUCTS token_type_t;


typedef struct {
  hash_value_t id;
  token_count_t count;
} h_item_t;

typedef struct {
  hash_count_t max_tokens;
  hash_bit_count_t max_hash_bits;
  token_count_t full_token_count;
  token_count_t unique_token_count;
  h_item_t *hash;
  bool_t track_features;
  h_item_t *feature_stack[MAX_TOKEN_LINE_STACK];
  token_stack_t feature_stack_top;
  int hashfull_warning;
} empirical_t;

typedef struct {
  hash_value_t id;
#if defined DIGITIZE_LAMBDA
  digitized_weight_t lam;
#else
  weight_t lam;
#endif
} PACK_STRUCTS c_item_t;

typedef enum {simple, sequential} mtype;

typedef struct {
  char *filename;
  char *fullfilename;
  token_order_t max_order;
  token_count_t fcomplexity;
  token_count_t model_unique_token_count;
  token_count_t model_full_token_count;
  document_count_t model_num_docs;
  hash_count_t max_tokens;
  hash_bit_count_t max_hash_bits;
  re_bitfield retype;
  score_t logZ;
  score_t divergence;
  score_t renorm;
  score_t delta;
  score_t complexity;
  score_t score;
  score_t score_div;
  score_t score_s2;
  score_t score_shannon;
  score_t shannon;
  score_t shannon_s2;
  score_t alpha;
  score_t beta;
  score_t mu;
  score_t s2;
  score_t prior;
  token_count_t fmiss;
  token_count_t mediacounts[TOKEN_CLASS_MAX];
  struct {
    mtype type;
    options_t options;
    charparser_t cp;
    digtype_t dt;
  } model;
  options_t c_options;
  c_item_t *hash;
  byte_t *mmap_start;
  long mmap_offset;
#if defined DIGITIZE_DIGRAMS
  digitized_weight_t dig[ASIZE][ASIZE];
#else
  weight_t dig[ASIZE][ASIZE];
#endif
} category_t;

typedef struct {
  token_count_t count;
  weight_t B; /* mustn't digitize this :-( */
#if defined DIGITIZE_LAMBDA
  digitized_weight_t lam; 
#else
  weight_t lam;
#endif
  union {
    struct {
#if defined DIGITIZE_LWEIGHTS
      digitized_weight_t ltrms;
      digitized_weight_t dref;
#else
      weight_t ltrms;
      weight_t dref;
#endif
    } min;
    struct {
      token_count_t eff;
    } read;
  } tmp;
  hash_value_t id;
  token_type_t typ;
} PACK_STRUCTS l_item_t;

typedef struct {
  hash_value_t *stack;
  hash_count_t top;
  hash_count_t max;
  score_t shannon;
} emplist_t;

typedef struct {
  char *filename;
  struct {
    FILE *file;
    char *filename;
    void *iobuf;    
    long offset;
    long used;
    off_t avail;
    byte_t *mmap_start;
    long mmap_offset;
    size_t mmap_length;
    long mmap_cursor;
  } tmp;
  re_bitfield retype;
  token_order_t max_order;
  token_count_t fixed_order_token_count[MAX_SUBMATCH];
  token_count_t fixed_order_unique_token_count[MAX_SUBMATCH];
  hash_bit_count_t max_hash_bits;
  hash_count_t max_tokens;
  token_count_t full_token_count;
  token_count_t unique_token_count;
  token_count_t tmax;
  score_t logZ;
  score_t divergence;
  score_t shannon;
  score_t shannon2;
  score_t alpha;
  score_t beta;
  score_t mu;
  score_t s2;
  score_t mediaprobs[TOKEN_CLASS_MAX];
  struct {
    options_t options;
    charparser_t cp;    
    digtype_t dt;
    int tmin;
  } model;
  options_t u_options;
  byte_t *mmap_start;
  long mmap_learner_offset;
  long mmap_hash_offset;
  l_item_t *hash;
  weight_t dig[ASIZE][ASIZE];
  long int regex_token_count[MAX_RE + 1];
  struct {
    score_t A;
    score_t S;
    document_count_t count;
    document_count_t nullcount;
    bool_t skip;
#define RESERVOIR_SIZE 25
/*       #define RESERVOIR_SIZE 12 */
    /* the reservoir size constrains the accuracy of the variance
     * estimate. Since this is a heavy computation, we want 
     * to choose the lowest value we can get away with. Here 12
     * gives an estimate for the error term to within sigma/3, which
     * hopefully is godd enough for most cases.
     */
    emplist_t emp;
    emplist_t reservoir[RESERVOIR_SIZE];
  } doc;
} learner_t;
/* this is used when minimizing learner divergence */
#define MAX_LAMBDA_JUMP 100

typedef struct {
  double alpha;
  double u[ASIZE];
} dirichlet_t;

typedef struct {
  regex_t regex;
  char *string;
  smbitmap_t submatches;
  regex_flags_t flags;
} myregex_t;

#define MAX_BOUNDARIES 8

#define MAX_BOUNDARY_BUFSIZE 70

typedef enum { ceUNDEF, ceID, ceB64, ceQP, ceBIN, ceSEVEN} MIME_Content_Encoding;
typedef enum { 
  ctUNDEF, 
  ctTEXT_PLAIN, ctTEXT_RICH, ctTEXT_HTML, ctTEXT_XML, ctTEXT_SGML, ctTEXT_UNKNOWN, 
  ctIMAGE, 
  ctAUDIO, 
  ctVIDEO,
  ctMODEL,
  ctMESSAGE_RFC822, 
  ctOTHER, 
  ctOCTET_STREAM,
  ctAPPLICATION_MSWORD
} MIME_Content_Type;

typedef struct {
  MIME_Content_Type type;
  MIME_Content_Encoding encoding;
} MIME_Struct;

typedef enum { htSTANDARD, htEXTENDED, htTRACE, htMIME, htCONT, htUNDEF } HEADER_Type;

typedef enum { msUNDEF=1, msHEADER, msBODY, msATTACH} Mstate;
typedef enum { msuUNDEF=1, msuTRACK, msuMIME, msuARMOR, msuOTHER } Msubstate;
typedef enum { mhsUNDEF=1, mhsSUBJECT, mhsFROM, mhsTO, mhsMIME, mhsXHEADER, mhsTRACE} Mhstate;
typedef enum { maUNDEF=1, maENABLED} Marmor;
typedef enum { psPLAIN, psUUENCODE } Mplainstate;
typedef enum { hidUNDEF=1, hidCONTINUATION,
	       hidRECEIVED, hidRETURN_PATH, hidRETURN_RECEIPT_TO, hidREPLY_TO,
	       hidMESSAGE_ID, hidREFERENCES, hidIN_REPLY_TO,
	       hidRESENT_, hidORIGINAL_,
	       hidFROM, hidCC, hidBCC, hidSENT, hidSENDER,
	       hidTO,
	       hidSUBJECT,
	       hidCONTENT_, hidMIME_VERSION,
	       hidLIST_,
	       hidX_,
	       hidUSER_AGENT,
	       hidX_MS, hidCATEGORY, hidPRIORITY, hidIMPORTANCE, hidTHREAD_,
	       hidCOMMENTS, hidKEYWORDS, hidNOTE
} Mheaderid;

typedef struct {
  char *cache;
  char *data_ptr;
  size_t cache_len;
  size_t max_line_len;
} decoding_cache;

#if defined HAVE_MBRTOWC

typedef struct {
  wchar_t *cache;
  wchar_t *data_ptr;
  size_t cache_len;
  size_t max_line_len;
} w_decoding_cache;

#endif

typedef struct {
  Mstate state;
  Msubstate substate;
  Mhstate hstate;
  Mheaderid hid;
  Marmor armor;
  MIME_Struct header, body;
  bool_t prev_line_empty;
  bool_t skip_until_boundary;
  bool_t corruption_check;
  bool_t skip_header;
  char strip_header_char;
#if defined HAVE_MBRTOWC
  wchar_t w_strip_header_char;
#endif
  Mplainstate plainstate;
  struct {
    int size[MAX_BOUNDARIES];
    char identifier[MAX_BOUNDARIES][MAX_BOUNDARY_BUFSIZE];
#if defined HAVE_MBRTOWC
    wchar_t w_identifier[MAX_BOUNDARIES][MAX_BOUNDARY_BUFSIZE];
#endif
    int index;
    bool_t was_end;
  } boundary;
  decoding_cache b64_dc;
  decoding_cache qp_dc;
#if defined HAVE_MBRTOWC
  w_decoding_cache w_b64_dc;
  w_decoding_cache w_qp_dc;
#endif
} MBOX_State;

typedef enum {TEXT=1, XTAG, XTAGQUOTE, XTAGDQUOTE, XTAGPREQ, TAG, TAGQUOTE, TAGDQUOTE, TAGPREQ, CMNT, DISABLED} Xstate;
typedef enum {ALT=1, SRC, SRC_NETLOC, SRC_NETLOC_PREFIX, SRC_NETLOC_PATH, SRC_NETLOC_SUFFIX, UNDEF, JSCRIPT, ASTYLE} Xattribute;
typedef enum {xpDUMB=1, xpHTML, xpSMART} Xparser;
typedef enum {SCRIPT=1,STYLE,COMMENT,NOFRAMES,NOEMBED,NOSCRIPT,NOLAYER,TITLE,VISIBLE} Xhide;

typedef struct {
  Xstate state;
  Xattribute attribute;
  Xparser parser;
  Xhide hide;
} XML_State;

typedef enum {xmlRESET,xmlDISABLE,xmlSMART,xmlHTML,xmlDUMB,xmlUNDEF} XML_Reset;

#ifdef __cplusplus
extern "C" 
{
#endif

  /* these are defined in dbacl.c */
  void sanitize_options();
  int set_option(int op, char *optarg);

  void init_learner(learner_t *learner, char *opath, bool_t readonly);
  void free_learner(learner_t *learner);

  void reset_mbox_messages(learner_t *learner, MBOX_State *mbox);
  void count_mbox_messages(learner_t *learner, Mstate mbox_state, char *buf);
  void calc_shannon(learner_t *learner);
  void update_shannon_partials(learner_t *learner, bool_t fulldoc);
  void optimize_and_save(learner_t *learner);

  l_item_t *find_in_learner(learner_t *learner, hash_value_t id);
  bool_t grow_learner_hash(learner_t *learner);
  void hash_word_and_learn(learner_t *learner, 
			   char *tok, token_type_t tt, regex_count_t re);

  void make_dirichlet_digrams(learner_t *learner);
  void make_uniform_digrams(learner_t *learner);
  void transpose_digrams(learner_t *learner);

  bool_t read_online_learner_struct(learner_t *learner, char *opath, bool_t readonly);
  void write_online_learner_struct(learner_t *learner, char *opath);
  error_code_t save_learner(learner_t *learner, char *opath);


  /* these are defined in catfun.c */
  char *sanitize_path(char *in, char *extension);
  error_code_t sanitize_model_options(options_t *to, charparser_t *mcp, category_t *cat);
  /*@shared@*/ char *print_model_options(options_t opt, charparser_t mcp, /*@out@*/ char *buf);
  char *print_user_options(options_t opt, char *buf);

  void init_empirical(empirical_t *emp, hash_count_t dmt, hash_bit_count_t dmhb);
  void free_empirical(empirical_t *emp);
  void clear_empirical(empirical_t *emp);
  h_item_t *find_in_empirical(empirical_t *emp, hash_value_t id);
  score_t empirical_entropy(empirical_t *emp);


  void init_category(category_t *cat);
  void free_category(category_t *cat);
  c_item_t *find_in_category(category_t *cat, hash_value_t id);
  void init_purely_random_text_category(category_t *cat);
  error_code_t load_category(category_t *cat);
  error_code_t load_category_header(FILE *input, category_t *cat);
  error_code_t open_category(category_t *cat);
  void reload_all_categories();

  void score_word(char *tok, token_type_t tt, regex_count_t re);
  confidence_t gamma_pvalue(category_t *cat, double obs);

  /* file format handling in fh.c */
  void init_file_handling();
  void cleanup_file_handling();

  token_class_t get_token_class();
  regex_count_t load_regex(char *buf);
  void free_all_regexes();

  /* common multibyte and wide char functions in mbw.c */
  good_char_t good_char(char *c);
  void std_tokenizer(char *p, char **pq, char *hbuf,
		     token_order_t *hbuf_order, token_order_t max_order,
		     void (*word_fun)(char *, token_type_t, regex_count_t),
		     token_type_t (*get_tt)(token_order_t));
  void regex_tokenizer(char *p, int i,
		       void (*word_fun)(char *, token_type_t, regex_count_t),
		       token_type_t (*get_tt)(token_order_t));
  void init_decoding_caches(MBOX_State *mbox);
  void free_decoding_caches(MBOX_State *mbox);
  bool_t b64_line_filter(decoding_cache *b64cache, char *line);
  char *b64_line_filter2(char *line, char *q);
  bool_t b64_line_flush(char *line, bool_t all);
  bool_t qp_line_filter(decoding_cache *qpcache, char *line);
  char *qp_line_filter2(char *line, char *q);
  bool_t qp_line_flush(char *line, bool_t all);
  bool_t mhe_line_filter(char *line);
  int extract_header_label(MBOX_State *mbox, char *line);
  bool_t extract_mime_boundary(MBOX_State *mbox, char *line);
  bool_t check_mime_boundary(MBOX_State *mbox, const char *line);
  bool_t mbox_line_filter(MBOX_State *mbox, char *line, XML_State *xml);
  bool_t plain_text_filter(MBOX_State *mbox, char *line);
  bool_t strings1_filter(char *line);

  void xml_character_filter(XML_State *xml, char *line);
  void process_file(FILE *input, 
		    int (*line_filter)(MBOX_State *, char *),
		    void (*character_filter)(XML_State *, char *), 
		    void (*word_fun)(char *, token_type_t, regex_count_t), 
		    char *(*pre_line_fun)(char *),
		    void (*post_line_fun)(char *));
  void process_directory(char *name, 
			 int (*line_filter)(MBOX_State *, char *),
			 void (*character_filter)(XML_State *, char *), 
			 void (*word_fun)(char *, token_type_t, regex_count_t), 
			 char *(*pre_line_fun)(char *),
			 void (*post_line_fun)(char *),
			 void (*post_file_fun)(char *));

  void init_mbox_line_filter(MBOX_State *mbox);
  void free_mbox_line_filter(MBOX_State *mbox);
  void reset_mbox_line_filter(MBOX_State *mbox);
  void reset_xml_character_filter(XML_State *xml, XML_Reset reset);
  XML_Reset select_xml_defaults(MIME_Struct *mime);

  /* probabilities in probs.c */
  double log_poisson(int k, double lambda);
  double sample_mean(double x, double n);
  double sample_variance(double ss, double x, double n);
  double min_prob(int k, int n, double mu[], double sigma[]);

#if defined HAVE_MBRTOWC
/*   int w_b64_code(wchar_t c); */
/*   int w_qp_code(wchar_t c); */
  good_char_t w_good_char(wchar_t *c);
  void w_std_tokenizer(wchar_t *p, char **pq, char *hbuf,
		       token_order_t *hbuf_order, token_order_t max_order,
		       void (*word_fun)(char *, token_type_t, regex_count_t),
		       token_type_t (*get_tt)(token_order_t));
  void w_regex_tokenizer(wchar_t *p, int i,
			 void (*word_fun)(char *, token_type_t, regex_count_t),
			 token_type_t (*get_tt)(token_order_t));
  void w_init_decoding_caches(MBOX_State *mbox);
  void w_free_decoding_caches(MBOX_State *mbox);
  bool_t w_b64_line_filter(w_decoding_cache *w_b64cache, wchar_t *line);
  wchar_t *w_b64_line_filter2(wchar_t *line, wchar_t *q);
  bool_t w_b64_line_flush(wchar_t *line, bool_t all);
  bool_t w_qp_line_filter(w_decoding_cache *w_qpcache, wchar_t *line);
  wchar_t *w_qp_line_filter2(wchar_t *line, wchar_t *q);
  bool_t w_qp_line_flush(wchar_t *line, bool_t all);
  bool_t w_mhe_line_filter(wchar_t *line);
  int w_extract_header_label(MBOX_State *mbox, wchar_t *line);
  bool_t w_extract_mime_boundary(MBOX_State *mbox, wchar_t *line);
  bool_t w_check_mime_boundary(MBOX_State *mbox, const wchar_t *line);
  bool_t w_mbox_line_filter(MBOX_State *mbox, wchar_t *line, XML_State *xml);
  bool_t w_plain_text_filter(MBOX_State *mbox, wchar_t *line);
  bool_t w_strings1_filter(wchar_t *line);

  int wcsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n);

  void w_xml_character_filter(XML_State *xml, wchar_t *line);
  void w_process_file(FILE *input, 
		      int (*line_filter)(MBOX_State *, wchar_t *),
		      void (*character_filter)(XML_State *, wchar_t *), 
		      void (*word_fun)(char *, token_type_t, regex_count_t), 
		      char *(*pre_line_fun)(char *),
		      void (*post_line_fun)(char *));
  void w_process_directory(char *name,
			   int (*line_filter)(MBOX_State *, wchar_t *),
			   void (*character_filter)(XML_State *, wchar_t *), 
			   void (*word_fun)(char *, token_type_t, regex_count_t), 
			   char *(*pre_line_fun)(char *),
			   void (*post_line_fun)(char *),
			   void (*post_file_fun)(char *));

#endif

#ifdef _SC_PAGE_SIZE
#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE _SC_PAGE_SIZE
#endif
#endif


#ifdef __cplusplus
}
#endif

#endif
