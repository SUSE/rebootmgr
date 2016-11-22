/* Based on parse-duration.c from libopts/gnulib.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "parse-duration.h"

/* True if the arithmetic type T is signed.  */
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))

#define TYPE_MAXIMUM(t)                                                 \
  ((t) (! TYPE_SIGNED (t)                                               \
        ? (t) -1                                                        \
        : ((((t) 1 << (sizeof (t) * CHAR_BIT - 2)) - 1) * 2 + 1)))

#define SEC_PER_MIN 60
#define SEC_PER_HR (SEC_PER_MIN * 60)

#define MAX_DURATION TYPE_MAXIMUM(time_t)

/* Wrapper around strtoul that does not require a cast.  */
static unsigned long
str_const_to_ul (const char *str, const char **ppz, int base)
{
  return strtoul (str, (char **)ppz, base);
}

/* Returns BASE + VAL * SCALE, interpreting BASE = BAD_TIME
   with errno set as an error situation, and returning BAD_TIME
   with errno set in an error situation.  */
static time_t
scale_n_add (time_t base, time_t val, int scale)
{
  if (base == BAD_TIME)
    {
      if (errno == 0)
        errno = EINVAL;
      return BAD_TIME;
    }

  if (val > MAX_DURATION / scale)
    {
      errno = ERANGE;
      return BAD_TIME;
    }

  val *= scale;
  if (base > MAX_DURATION - val)
    {
      errno = ERANGE;
      return BAD_TIME;
    }

  return base + val;
}

/* Parses a value and returns BASE + value * SCALE, interpreting
   BASE = BAD_TIME with errno set as an error situation, and returning
   BAD_TIME with errno set in an error situation.  */
static time_t
parse_scaled_value (time_t base, const char **ppz, const char *endp, int scale)
{
  const char *pz = *ppz;
  time_t val;

  if (base == BAD_TIME)
    return base;

  errno = 0;
  val = str_const_to_ul (pz, &pz, 10);
  if (errno != 0)
    return BAD_TIME;
  while (isspace ((unsigned char)*pz))
    pz++;
  if (pz != endp)
    {
      errno = EINVAL;
      return BAD_TIME;
    }

  *ppz = pz;
  return scale_n_add (base, val, scale);
}

/* Parses the syntax HH:MM[:SS].
   PS points into the string, after "HH", before ":MM[:SS]".  */
static time_t
parse_hour_minute_second (const char *pz, const char *ps)
{
  time_t res = 0;

  res = parse_scaled_value (0, &pz, ps, SEC_PER_HR);

  pz++;
  ps = strchr (pz, ':');
  if (ps == NULL)
    ps = pz + strlen (pz);

  res = parse_scaled_value (res, &pz, ps, SEC_PER_MIN);

  /* No seconds specified */
  if (*pz == '\0')
    return res;

  pz++;
  ps = pz + strlen (pz);
  return parse_scaled_value (res, &pz, ps, 1);
}

/* Parses the syntax HHMMSS.  */
static time_t
parse_hourminutesecond (const char *in_pz)
{
  time_t res = 0;
  char   buf[4];
  const char *pz;

  if (strlen (in_pz) != 6)
    {
      errno = EINVAL;
      return BAD_TIME;
    }

  memcpy (buf, in_pz, 2);
  buf[2] = '\0';
  pz = buf;
  res = parse_scaled_value (0, &pz, buf + 2, SEC_PER_HR);

  memcpy (buf, in_pz + 2, 2);
  buf[2] = '\0';
  pz =   buf;
  res = parse_scaled_value (res, &pz, buf + 2, SEC_PER_MIN);

  memcpy (buf, in_pz + 4, 2);
  buf[2] = '\0';
  pz =   buf;
  return parse_scaled_value (res, &pz, buf + 2, 1);
}

/* Parses the syntax hh H mm M ss S.  */
static time_t
parse_HMS (const char *pz)
{
  time_t res = 0;
  const char *ps = strpbrk (pz, "Hh");
  if (ps != NULL)
    {
      res = parse_scaled_value (0, &pz, ps, SEC_PER_HR);
      pz++;
    }

  ps = strpbrk (pz, "Mm");
  if (ps != NULL)
    {
      res = parse_scaled_value (res, &pz, ps, SEC_PER_MIN);
      pz++;
    }

  ps = strpbrk (pz, "Ss");
  if (ps != NULL)
    {
      res = parse_scaled_value (res, &pz, ps, 1);
      pz++;
    }

  while (isspace ((unsigned char)*pz))
    pz++;

  if (*pz != '\0')
    {
      errno = EINVAL;
      return BAD_TIME;
    }

  return res;
}

/* Parses duration (hours, minutes, seconds).  */
time_t
parse_duration (const char *pz)
{
  const char *ps;
  time_t  res = BAD_TIME;

  while (isspace ((unsigned char)*pz))
    pz++;

  if ((ps = strchr (pz, ':')) != NULL) /* HH:MM[:SS] format */
    res = parse_hour_minute_second (pz, ps);
  else if (strpbrk (pz, "HMShms") != NULL) /* H,M or S suffix */
    res = parse_HMS (pz);
  else /* Its a HHMMSS format: */
    res = parse_hourminutesecond (pz);

  return res;
}
