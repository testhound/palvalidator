// Copyright (C) MKC Associates, LLC - All Rights Reserved
// Unauthorized copying of this file, via any medium is strictly prohibited
// Proprietary and confidential
// Written by Michael K. Collison <collison956@gmail.com>, July 2016
//

#ifndef __TIME_SERIES_INDICATORS_H
#define __TIME_SERIES_INDICATORS_H 1

#include "TimeSeries.h"
#include "DecimalConstants.h"
#include <algorithm>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include "ThrowAssert.hpp"

namespace mkc_timeseries
{
using namespace boost::accumulators;

  // Divides each elementn of series1 by it's corresponding element in series2

 template <class Decimal>
 NumericTimeSeries<Decimal> DivideSeries (const NumericTimeSeries<Decimal>& series1,
				       const NumericTimeSeries<Decimal>& series2)
  {
    if (series1.getTimeFrame() != series2.getTimeFrame())
      throw std::domain_error (std::string("DivideSeries:: time frame of two series must be the same"));

    if (series1.getLastDate() != series2.getLastDate())
      throw std::domain_error (std::string ("DivideSeries:: end date of two series must be the same"));

    unsigned long seriesMin = std::min (series1.getNumEntries(), series2.getNumEntries());
    unsigned long initialEntries = std::max (seriesMin, (unsigned long) 1);

    NumericTimeSeries<Decimal> resultSeries(series1.getTimeFrame(), initialEntries);
    TimeFrame::Duration resultTimeFrame = series1.getTimeFrame();

    typename NumericTimeSeries<Decimal>::ConstReverseTimeSeriesIterator it1 = series1.beginReverseSortedAccess();
    typename NumericTimeSeries<Decimal>::ConstReverseTimeSeriesIterator it2 = series2.beginReverseSortedAccess();
    Decimal temp;

    for (; ((it1 != series1.endReverseSortedAccess()) && (it2 != series2.endReverseSortedAccess())); it1++, it2++)
      {
	throw_assert (it1->first == it2->first, "DivideSeries - date1: " +boost::gregorian::to_simple_string (it1->first) +" and date2: " +boost::gregorian::to_simple_string(it2->first) +" are not equal");
	if (it2->second->getValue() == DecimalConstants<Decimal>::DecimalZero)
	  temp = DecimalConstants<Decimal>::DecimalZero;
	else
	  temp = it1->second->getValue() / it2->second->getValue();

	resultSeries.addEntry (NumericTimeSeriesEntry<Decimal> (it1->first,
							     temp,
							     resultTimeFrame));
      }

    return resultSeries;
  }

  template <class Decimal>
  NumericTimeSeries<Decimal> RocSeries (const NumericTimeSeries<Decimal>& series, uint32_t period)
  {
    unsigned long initialEntries = std::max (series.getNumEntries() - 1, (unsigned long) 1);

    NumericTimeSeries<Decimal> resultSeries(series.getTimeFrame(), initialEntries);
    typename NumericTimeSeries<Decimal>::ConstRandomAccessIterator it = series.beginRandomAccess();

    if (series.getNumEntries() < (period + 1))
      return resultSeries;

    // Start at second element so we begin the rate of change calculatiom
    //it++;

    it = it + period;
    Decimal currentValue, prevValue, rocValue;
    std::shared_ptr<NumericTimeSeriesEntry<Decimal>> p;

    for (; it != series.endRandomAccess(); it++)
      {
	p = series.getTimeSeriesEntry (it, 0);
	currentValue = p->getValue();

	prevValue = series.getValue(it, period);

	rocValue = ((currentValue / prevValue) - DecimalConstants<Decimal>::DecimalOne) *
	  DecimalConstants<Decimal>::DecimalOneHundred;
	resultSeries.addEntry(NumericTimeSeriesEntry<Decimal> (p->getDate(),
							    rocValue,
							    series.getTimeFrame()));
      }

    return resultSeries;
  }

  // Calculate median of entire series

  template <class Decimal>
  Decimal Median(const NumericTimeSeries<Decimal>& series)
  {
    typedef typename std::vector<Decimal>::size_type vec_size_type;

    std::vector<Decimal> sortedVector (series.getTimeSeriesAsVector());
    std::sort (sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0)
      throw std::domain_error ("Cannot take median of empty time series");

    vec_size_type mid = size / 2;

    if ((size % 2) == 0)
      return (sortedVector[mid] + sortedVector[mid - 1])/DecimalConstants<Decimal>::DecimalTwo;
    else
      return sortedVector[mid];
  }

  template <class Decimal>
  Decimal MedianOfVec(const std::vector<Decimal>& series)
  {
    typedef typename std::vector<Decimal>::size_type vec_size_type;

    std::vector<Decimal> sortedVector (series);
    std::sort (sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0)
      throw std::domain_error ("Cannot take median of empty time series");

    vec_size_type mid = size / 2;

    if ((size % 2) == 0)
      return ((sortedVector[mid] + sortedVector[mid - 1])/DecimalConstants<Decimal>::DecimalTwo);
    else
      return sortedVector[mid];
  }

  // Calculate median of entire series

  template <typename T>
  double Median(const std::vector<T>& series)
  {
    typedef typename std::vector<T>::size_type vec_size_type;

    std::vector<T> sortedVector (series);
    std::sort (sortedVector.begin(), sortedVector.end());

    vec_size_type size = sortedVector.size();
    if (size == 0)
      throw std::domain_error ("Cannot take median of empty time series");

    vec_size_type mid = size / 2;

    if ((size % 2) == 0)
      return (double) ((sortedVector[mid] + sortedVector[mid - 1])/2.0);
    else
      return (double) sortedVector[mid];
  }

  // Calculate standard deviation

  template <typename T>
  double StandardDeviation(const std::vector<T>& series)
  {
    if (series.size() > 0)
      {
	accumulator_set<T, features<tag::variance>> varianceStats;

	varianceStats = for_each (series.begin(), series.end(), varianceStats);
	return sqrt (variance (varianceStats));
      }
    else
      return 0.0;
  }

// Calculate median of entire series

  template <typename T>
  double MedianAbsoluteDeviation(const std::vector<T>& series)
  {
    if (series.size() > 0)
      {
	double firstMedian = Median (series);
	typename std::vector<T>::const_iterator it = series.begin();
	double temp;

	std::vector<double> secondMedianVector;

	for (; it != series.end(); it++)
	  {
	    temp = abs ((double) *it - firstMedian);
	    secondMedianVector.push_back (temp);
	  }

	return Median<double> (secondMedianVector) * 1.4826;
      }
    else
      return 0.0;
  }

/* c##################################################################### */
  /* c######################  file Qn.for :  ############################## */
  /* c##################################################################### */
  /* c */
  /* c   This file contains a Fortran function for a new robust estimator */
  /* c   of scale denoted as Qn, proposed in Rousseeuw and Croux (1993). */
  /* c   The estimator has a high breakdown point and a smooth and bounded */
  /* c   influence function. The algorithm given here is very fast (running */
  /* c   in O(nlogn) time) and needs only O(n) storage space. */
  /* c */
  /* c   Rousseeuw, P.J. and Croux, C. (1993), "Alternatives to the */
  /* c      Median Absolute Deviation," Journal of the American */
  /* c      Statistical Association, Vol. 88, 1273-1283. */
  /* c */
  /* c   A Fortran function for the estimator Sn, described in the same */
  /* c   paper, is attached above. For both estimators, implementations */
  /* c   in the Pascal language can be obtained from the authors. */
  /* c */
  /* c   This software may be used and copied freely, provided */
  /* c   reference is made to the abovementioned paper. */
  /* c */
  /* c   For questions, problems or comments contact: */
  /* c */
  /* c              Peter Rousseeuw (rousse@wins.uia.ac.be) */
  /* c              Christophe Croux (croux@wins.uia.ac.be) */
  /* c              Department of Mathematics and Computing */
  /* c              Universitaire Instelling Antwerpen */
  /* c              Universiteitsplein 1 */
  /* c              B-2610 Wilrijk (Antwerp) */
  /* c              Belgium */
  /* c */
  /* c-------------------------------------------------------------------- */
  /* c */
  /* c   Efficient algorithm for the scale estimator: */
  /* c */
  /* c       Qn = dn * 2.2219 * {|x_i-x_j|; i<j}_(k) */
  /* c */
  /* c   Parameters of the function Qn : */
  /* c       x  : real array containing the observations */
  /* c       n  : number of observations (n >=2) */
  /* c */
  /* c   The function Qn uses the procedures: */
  /* c      whimed(a,iw,n): finds the weighted high median of an array */
  /* c                      a of length n, using the array iw (also of */
  /* c                      length n) with positive integer weights. */
  /* c      sort(x,n,y) : sorts an array x of length n, and stores the */
  /* c                    result in an array y (of size at least n) */
  /* c      pull(a,n,k) : finds the k-th order statistic of an */
  /* c                    array a of length n */
  /* c */

  template <class Decimal>
  class RobustQn
  {
  public:
    RobustQn(const NumericTimeSeries<Decimal>& series) :
      mNumericSeries (series)
    {}

    RobustQn() :
      mNumericSeries (TimeFrame::DAILY)
    {}

    ~RobustQn()
    {}

    Decimal getRobustQn()
    {
      std::vector<Decimal> aVector(mNumericSeries.getTimeSeriesAsVector());
      long size = (long) aVector.size();

      return qn_(aVector.data(), &size);
    }

    Decimal getRobustQn(std::vector<Decimal>& inputVec)
    {
      long size = (long) inputVec.size();

      return qn_(inputVec.data(), &size);
    }

  private:
    Decimal qn_(Decimal *x, long int *n)
    {
      long int i__1, i__2;
      Decimal ret_val;
      static long int h__, i__, j, k;
      long int p[*n], q[*n];
      Decimal y[*n];
      static Decimal dn;
      static long int jj, nl, nr, knew;
      long int left[*n];
      Decimal work[*n];
      static long int sump, sumq;
      static long int jhelp;
      static int found;
      static Decimal trial;
      long int right[*n];
      long int weight[*n];

      /* Parameter adjustments */
      --x;

      /* Function Body */
      h__ = *n / 2 + 1;
      k = h__ * (h__ - 1) / 2;
      sort_(&x[1], n, y);
      i__1 = *n;

      for (i__ = 1; i__ <= i__1; ++i__)
	{
	  left[i__ - 1] = *n - i__ + 2;
	  right[i__ - 1] = *n;
	}

      jhelp = *n * (*n + 1) / 2;
      knew = k + jhelp;
      nl = jhelp;
      nr = *n * *n;
      found = 0;

    L200:
      if (nr - nl > *n && ! found)
	{
	  j = 1;
	  i__1 = *n;

	  for (i__ = 2; i__ <= i__1; ++i__)
	    {
	      if (left[i__ - 1] <= right[i__ - 1])
		{
		  weight[j - 1] = right[i__ - 1] - left[i__ - 1] + 1;
		  jhelp = left[i__ - 1] + weight[j - 1] / 2;
		  work[j - 1] = y[i__ - 1] - y[*n + 1 - jhelp - 1];
		  ++j;
		}
	    }

	  i__1 = j - 1;
	  trial = whimed_(work, weight, &i__1);
	  j = 0;

	  for (i__ = *n; i__ >= 1; --i__)
	    {
	    L45:
	      if (j < *n && y[i__ - 1] - y[*n - j - 1] < trial)
		{
		  ++j;
		  goto L45;
		}
	      p[i__ - 1] = j;
	    }

	  j = *n + 1;
	  i__1 = *n;

	  for (i__ = 1; i__ <= i__1; ++i__)
	    {
	    L55:
	      if (y[i__ - 1] - y[*n - j + 1] > trial)
		{
		  --j;
		  goto L55;
		}
	      q[i__ - 1] = j;
	    }

	  sump = 0;
	  sumq = 0;
	  i__1 = *n;

	  for (i__ = 1; i__ <= i__1; ++i__)
	    {
	      sump += p[i__ - 1];
	      sumq = sumq + q[i__ - 1] - 1;
	    }

	  if (knew <= sump)
	    {
	      i__1 = *n;
	      for (i__ = 1; i__ <= i__1; ++i__)
		{
		  right[i__ - 1] = p[i__ - 1];
		}

	      nr = sump;
	    }
	  else
	    {
	      if (knew > sumq)
		{
		  i__1 = *n;
		  for (i__ = 1; i__ <= i__1; ++i__)
		    {
		      left[i__ - 1] = q[i__ - 1];
		    }

		  nl = sumq;
		}
	      else
		{
		  ret_val = trial;
		  found = 1;
		}
	    }
	  goto L200;
	}

      if (! found)
	{
	  j = 1;
	  i__1 = *n;
	  for (i__ = 2; i__ <= i__1; ++i__)
	    {
	      if (left[i__ - 1] <= right[i__ - 1])
		{
		  i__2 = right[i__ - 1];
		  for (jj = left[i__ - 1]; jj <= i__2; ++jj)
		    {
		      work[j - 1] = y[i__ - 1] - y[*n - jj];
		      ++j;
		    }
		}
	    }

	  i__1 = j - 1;
	  i__2 = knew - nl;
	  ret_val = pull_(work, &i__1, &i__2);
	}

      if (*n <= 9)
	{
	  if (*n == 2)
	    dn = dec::fromString<Decimal>("0.399");
	  else if (*n == 3)
	    dn = dec::fromString<Decimal>("0.994");
	  else if (*n == 4)
	    dn = dec::fromString<Decimal>("0.512");
	  else if (*n == 5)
	    dn = dec::fromString<Decimal>("0.844");
	  else if (*n == 6)
	    dn = dec::fromString<Decimal>("0.611");
	  else if (*n == 7)
	    dn = dec::fromString<Decimal>("0.857");
	  else if (*n == 8)
	    dn = dec::fromString<Decimal>("0.669");
	  else
	    dn = dec::fromString<Decimal>("0.872");
	}
      else
	{
	  if (*n % 2 == 1)
	    dn = Decimal((int) *n) / (Decimal((int) *n) + dec::fromString<Decimal>("1.4"));

	  if (*n % 2 == 0)
	    dn = Decimal((int) *n) / (Decimal((int)*n) + dec::fromString<Decimal>("3.8"));
	}

      ret_val = dn * dec::fromString<Decimal>("2.21914") * ret_val;
      return ret_val;
    }

    int sort_(Decimal *a, long int *n, Decimal *b)
    {
      long int i__1;

      static long int j, jr;
      static Decimal xx;
      static long int jnc;
      static Decimal amm;
      static long int jss, jndl, jtwe;
      long int jlv[*n], jrv[*n];

      /* c  Sorts an array a of length n<=1000, and stores */
      /* c  the result in the array b. */
      /* Parameter adjustments */
      --b;
      --a;

      i__1 = *n;
      for (j = 1; j <= i__1; ++j)
	b[j] = a[j];

      jss = 1;
      jlv[0] = 1;
      jrv[0] = *n;
    L10:
      jndl = jlv[jss - 1];
      jr = jrv[jss - 1];
      --jss;
    L20:
      jnc = jndl;
      j = jr;
      jtwe = (jndl + jr) / 2;
      xx = b[jtwe];

    L30:
      if (b[jnc] >= xx)
	goto L40;

      ++jnc;
      goto L30;

    L40:
      if (xx >= b[j])
	goto L50;
      --j;
      goto L40;

    L50:
      if (jnc > j)
	goto L60;

      amm = b[jnc];
      b[jnc] = b[j];
      b[j] = amm;
      ++jnc;
      --j;

    L60:
      if (jnc <= j)
	goto L30;

      if (j - jndl < jr - jnc)
	goto L80;

      if (jndl >= j)
	goto L70;

      ++jss;
      jlv[jss - 1] = jndl;
      jrv[jss - 1] = j;

    L70:
      jndl = jnc;
      goto L100;

    L80:
      if (jnc >= jr)
	goto L90;

      ++jss;
      jlv[jss - 1] = jnc;
      jrv[jss - 1] = jr;

    L90:
      jr = j;

    L100:
      if (jndl < jr)
	goto L20;

      if (jss != 0)
	goto L10;

      return 0;
    }

    Decimal pull_(Decimal *a, long int *n, long int *k)
    {
      /* System generated locals */
      long int i__1;
      Decimal ret_val;

      /* Local variables */
      Decimal b[*n];
      static long int j, l;
      static Decimal ax;
      static long int lr, jnc;
      static Decimal buffer;

      /* c  Finds the kth order statistic of an array a of length n<=1000 */
      /* Parameter adjustments */
      --a;

      i__1 = *n;
      for (j = 1; j <= i__1; ++j)
	{
	  b[j - 1] = a[j];
	}

      l = 1;
      lr = *n;

    L20:
      if (l >= lr)
	{
	  goto L90;
	}

      ax = b[*k - 1];
      jnc = l;
      j = lr;

    L30:
      if (jnc > j)
	{
	  goto L80;
	}

    L40:
      if (b[jnc - 1] >= ax)
	{
	  goto L50;
	}

      ++jnc;
      goto L40;

    L50:
      if (b[j - 1] <= ax)
	{
	  goto L60;
	}
      --j;
      goto L50;

    L60:
      if (jnc > j)
	{
	  goto L70;
	}

      buffer = b[jnc - 1];
      b[jnc - 1] = b[j - 1];
      b[j - 1] = buffer;
      ++jnc;
      --j;
    L70:
      goto L30;

    L80:
      if (j < *k)
	{
	  l = jnc;
	}
      if (*k < jnc)
	{
	  lr = j;
	}
      goto L20;

    L90:
      ret_val = b[*k - 1];
      return ret_val;
    } /* pull_ */


    /* c */
    /* c  Algorithm to compute the weighted high median in O(n) time. */
    /* c */
    /* c  The whimed is defined as the smallest a(j) such that the sum */
    /* c  of the weights of all a(i) <= a(j) is strictly greater than */
    /* c  half of the total weight. */
    /* c */
    /* c  Parameters of this function: */
    /* c        a: real array containing the observations */
    /* c        n: number of observations */
    /* c       iw: array of integer weights of the observations. */
    /* c */
    /* c  This function uses the function pull. */
    /* c */
    /* c  The size of acand, iwcand must be at least n. */
    /* c */

    Decimal whimed_(Decimal *a, long int *iw, long int *n)
    {
      /* System generated locals */
      long int i__1;
      Decimal ret_val;

      /* Local variables */
      static long int i__, nn, wmid;

      Decimal acand[*n];
      static long int kcand;
      static Decimal trial;
      static long int wleft, wrest, wright, wtotal;
      long int iwcand[*n];

      /* Parameter adjustments */
      --iw;
      --a;

      nn = *n;
      wtotal = 0;
      i__1 = nn;
      for (i__ = 1; i__ <= i__1; ++i__)
	wtotal += iw[i__];

      wrest = 0;

    L100:
      i__1 = nn / 2 + 1;
      trial = pull_(&a[1], &nn, &i__1);
      wleft = 0;
      wmid = 0;
      wright = 0;
      i__1 = nn;
      for (i__ = 1; i__ <= i__1; ++i__)
	{
	  if (a[i__] < trial)
	    {
	      wleft += iw[i__];
	    }
	  else
	    {
	      if (a[i__] > trial) {
		wright += iw[i__];
	      }
	      else
		{
		  wmid += iw[i__];
		}
	    }
	}

      if ((wrest << 1) + (wleft << 1) > wtotal)
	{
	  kcand = 0;
	  i__1 = nn;
	  for (i__ = 1; i__ <= i__1; ++i__)
	    {
	      if (a[i__] < trial)
		{
		  ++kcand;
		  acand[kcand - 1] = a[i__];
		  iwcand[kcand - 1] = iw[i__];
		}
	    }
	  nn = kcand;
	}
      else
	{
	  if ((wrest << 1) + (wleft << 1) + (wmid << 1) > wtotal)
	    {
	      ret_val = trial;
	      return ret_val;
	    }
	  else
	    {
	      kcand = 0;
	      i__1 = nn;
	      for (i__ = 1; i__ <= i__1; ++i__)
		{
		  if (a[i__] > trial)
		    {
		      ++kcand;
		      acand[kcand - 1] = a[i__];
		      iwcand[kcand - 1] = iw[i__];
		    }
		}

	      nn = kcand;
	      wrest = wrest + wleft + wmid;
	    }
	}

      i__1 = nn;

      for (i__ = 1; i__ <= i__1; ++i__)
	{
	  a[i__] = acand[i__ - 1];
	  iw[i__] = iwcand[i__ - 1];
	}
      goto L100;
    }

  private:
    NumericTimeSeries<Decimal> mNumericSeries;
  };
}

#endif

