/* 
 * Copyright (C) 2002, 2003 Laird Breyer
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

#include <math.h>
#include "util.h"

#ifndef M_PI
#define M_PI           3.14159265358979323846
#define M_SQRT2        1.41421356237309504880
#define M_2_SQRTPI     1.12837916709551257390
#endif


double sample_mean(double x, double n) {
  return x/n;
}

double sample_variance(double s, double x, double n) {
  return (n * s - (x * x))/(n * (n - 1.0));
}

/* returns the logarithm of a Poisson distribution 
   (calculations based on Stirling's formula) */
double log_poisson(int k, double lambda) {

  return (double)k * (log(lambda) -log((double)k) + 1.0) - 
    log(2.0 * M_PI * (double)k ) / 2.0 - lambda;

}

/* returns the probability that a chi squared with df
   degrees of freedom is less than or equal to x */
double chi2_cdf(double df, double x) {
  /* to be implemented */
  return 1.0 - igamc(df/2.0, x/2.0);
}

double gamma_tail(double a, double b, double x) {
  /* don't call igamc with extreme numbers, it can exit with an error */
  return igamc(a, x/b);
}

double normal_cdf(double x) {
  return ndtr(x);
}

/* Given X_1,...,X_n independent Gaussians, computes the probability
   that X_k = min_j X_j. 
   eg:

   Prob(X_1 <= min_j X_j) = \int \phi(x) \prod_{j\neq 1} Prob(X_j > x)
 
   Call this function once for each X_j. The computation uses Gaussian
   quadrature with Hermite polynomials, 10 points from Abramovitz and
   Stegun, p.924. It doesn't need to be very accurate, but an error of
   more than 1% looks bad when displayed to the user. 
 */
double min_prob(int k, int n, double mu[], double sigma[]) {
  const double gauss_hermite[10][2] = {
    {-3.4361591188, 1.0254516913},
    {-2.5327316742, 0.8206661264},
    {-1.7566836492, 0.7414419319},
    {-1.0366108297 , 0.7032963231},
    {-0.3429013272, 0.6870818539},
    {0.3429013272, 0.6870818539},
    {1.0366108297 , 0.7032963231},
    {1.7566836492, 0.7414419319},
    {2.5327316742, 0.8206661264},
    {3.4361591188, 1.0254516913},
  };
  double x, p, srt, f, g;
  int i,j;

  srt = sigma[k] * M_SQRT2;
  p = 0.0;
  for(i = 0; i < 10; i++) {
    x = gauss_hermite[i][0];
    f = exp(-x*x);
    g = f;
    /* don't compute full product if too small to matter (less than 1%) */
    for(j = 0; (j < n) && (g > 0.01); j++) {
      if( j == k ) continue;
      g *= (1.0 - ndtr( ((mu[k] + x * srt) - mu[j])/sigma[j] ));
    }
    p += gauss_hermite[i][1] * g;
  }
  return p * M_2_SQRTPI / 2.0;
}
