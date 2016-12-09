/* Floating point helpers - from Mesa u_math.h */
#ifndef H_FLOAT_HELPERS
#define H_FLOAT_HELPERS

#include <stdbool.h>

union fi {
   float f;
   int32_t i;
   uint32_t ui;
};

/**
 * Return float bits.
 */
static inline unsigned
fui( float f )
{
   union fi fi;
   fi.f = f;
   return fi.ui;
}

static inline float
uif(uint32_t ui)
{
   union fi fi;
   fi.ui = ui;
   return fi.f;
}

/**
 * Single-float
 */
static inline bool
util_is_inf_or_nan(float x)
{
   union fi tmp;
   tmp.f = x;
   return (tmp.ui & 0x7f800000) == 0x7f800000;
}


static inline bool
util_is_nan(float x)
{
   union fi tmp;
   tmp.f = x;
   return (tmp.ui & 0x7fffffff) > 0x7f800000;
}


static inline int
util_inf_sign(float x)
{
   union fi tmp;
   tmp.f = x;
   if ((tmp.ui & 0x7fffffff) != 0x7f800000) {
      return 0;
   }

   return (x < 0) ? -1 : 1;
}

#endif
