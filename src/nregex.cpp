#include "nregex.h"

#include <sys/types.h>
#include <string.h>
#include <regex.h>

int
reg_match (char *expression, char *match, int flags)
{
  regex_t preg;
  size_t r;
  regmatch_t pmatch[REG_MATCHES_MAX];

  if ((r = regcomp (&preg, expression, (flags | REG_EXTENDED))))
    return r;

  r = regexec (&preg, match, REG_MATCHES_MAX, pmatch, 0);

  regfree (&preg);

  return r;
}

char *
reg_sub_g (char *subject, char *pattern, int cflags, char *output,
	   size_t max_size, char *rep)
{
  regex_t preg;

  output[0] = 0x0;

  if ((regcomp (&preg, pattern, cflags)))
    {
      return NULL;
    }

  char *rs_p = subject;
  regoff_t o_l = (regoff_t)strlen (rs_p);
  size_t rep_l = strlen (rep);

  regmatch_t rm[2];
  char *m_p = rs_p;
  size_t rs_w = 0;

  regoff_t r_eo = -1;

  while (!regexec (&preg, m_p, 1, rm, 0))
    {
      if (rm[0].rm_so == 0 && rm[0].rm_eo == o_l)
	{
	  strncpy (&output[rs_w], rep, rep_l);
	  output[rep_l + rs_w] = 0x0;
	  return output;
	}
      if (rm[0].rm_so == rm[0].rm_eo)
	{
	  if (!rm[0].rm_so)
	    {
	      strncpy (output, rep, rep_l);
	      strncpy (&output[rep_l], rs_p, o_l);
	      output[rep_l + o_l] = 0x0;
	    }
	  else if (rm[0].rm_so == o_l)
	    {
	      strncpy (output, rs_p, o_l);
	      strncpy (&output[o_l], rep, rep_l);
	      output[rep_l + o_l] = 0x0;
	    }
	  else
	    {
	      return rs_p;
	    }
	  break;
	}

      if (rm[0].rm_so == (regoff_t) -1)
	{
	  return output;
	}

      strncpy (&output[rs_w], m_p, rm[0].rm_so);
      rs_w += (size_t) rm[0].rm_so;
      strncpy (&output[rs_w], rep, rep_l);
      rs_w += rep_l;
      m_p = &m_p[rm[0].rm_eo];
      r_eo = rm[0].rm_eo;
    }

  if (m_p != rs_p)
    {
      size_t fw_l = strlen (rs_p) - r_eo;
      strncpy (&output[rs_w], m_p, fw_l);
      rs_w += fw_l;
      output[rs_w] = 0x0;
    }
  else
    {
      return rs_p;
    }

  return output;
}
