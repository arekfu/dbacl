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

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include "dbacl.h"

/* external commands */
#define CMD_QUITNOW                     1
#define CMD_RELOAD_CATS                 2

/* in gcc, most calls to extern inline functions are inlined */

#if defined JENKINS4

#define JENKINS_HASH_VALUE unsigned long int
/* string hash function in jenkins.c */
unsigned long int hash( unsigned char *k, 
			unsigned long int length, 
			unsigned long int initval);
#elif defined JENKINS8

#define JENKINS_HASH_VALUE unsigned long long
unsigned long long hash( unsigned char *k,
			 unsigned long long length,
			 unsigned long long initval);
#endif

/* this should make them as fast as a macro */
hash_value_t hash_full_token(const char *tok);
hash_value_t hash_partial_token(const char *tok, int len, 
				const char *extra);

/* this should make them as fast as a macro */
digitized_weight_t digitize_a_weight(weight_t w, token_order_t o);
weight_t undigitize_a_weight(digitized_weight_t d, token_order_t o);

double nats2bits(double score);

double chi2_cdf(double df, double x);
double gamma_tail(double a, double b, double x);
double normal_cdf(double x);

extern double igamc(double, double);
extern double ndtr(double);

typedef struct {
  const char *tempfile;
} signal_cleanup_t;

void init_signal_handling();
void process_pending_signal(FILE *input);
void cleanup_signal_handling();

void init_buffers();
void cleanup_buffers();
void cleanup_tempfiles();
void set_iobuf_mode(FILE *input);

bool_t fill_textbuf(FILE *input, int *extra_lines);
#if defined HAVE_MBRTOWC
bool_t fill_wc_textbuf(char *pptextbuf, mbstate_t *shiftstate);
#endif

#define E_ERROR 0
#define E_WARNING 1
#define E_FATAL 2
void errormsg(int type, const char *fmt, ...)
#ifdef __GNUC__
__attribute__((format (printf, 2, 3)))
#endif
;
void print_token(FILE *out, const char *tok);

/* Solaris needs this */
#if !defined isinf
#define isinf(x) (!finite(x) && (x)==(x))
#endif

/* Solaris needs this */
#if !defined isblank
#define isblank(c) (((c) == ' ') || ((c) == '\t'))
#endif

#endif
