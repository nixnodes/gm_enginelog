#define REG_MATCHES_MAX                 4
#define REG_PATTERN_BUFFER_MAX 		131072

#include <sys/types.h>

int
reg_match (char *expression, char *match, int flags);
char *
reg_sub_g (char *subject, char *pattern, int cflags, char *output,
           size_t max_size, char *rep);

