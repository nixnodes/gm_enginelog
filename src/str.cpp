

#include "str.h"

#include "int_memory.h"

#include <stdio.h>
#include <string.h>

char *
bb_to_ascii (unsigned char *block, size_t len, char *out)
{

  size_t i;
  char *o_out = out;
  for (i = 0; i < len; i++)
    {
      snprintf (out, 3, "%.2hhx", block[i]);
      out += 2;
    }

  return o_out;
}

int
split_string (char *line, char dl, pmda output_t)
{
  int i, p, c, llen = strlen (line);

  for (i = 0, p = 0, c = 0; i <= llen; i++)
    {

      while (line[i] == dl && line[i])
	i++;
      p = i;

      while (line[i] != dl && line[i] != 0xA && line[i])
	i++;

      if (i > p)
	{
	  char *buffer = (char*) md_alloc (output_t, (i - p) + 10, 0, NULL);
	  if (!buffer)
	    return -1;
	  memcpy (buffer, &line[p], i - p);
	  c++;
	}
    }
  return c;
}

char *
g_dirname (char *input)
{

  char *b_ptr = strrchr (input, 0x2F);
  if (NULL == b_ptr || b_ptr == input)
    {
      input[0] = 0x2F;
      input[1] = 0;
    }
  else
    {
      b_ptr[0] = 0x0;
    }
  return input;
}
