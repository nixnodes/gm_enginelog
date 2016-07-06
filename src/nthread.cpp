#include "nthread.h"

#include <pthread.h>

int
mutex_init (pthread_mutex_t *mutex, int flags, int robust)
{
  pthread_mutexattr_t mattr;

  pthread_mutexattr_init (&mattr);
  pthread_mutexattr_settype (&mattr, flags);
  pthread_mutexattr_setrobust (&mattr, robust);

  return pthread_mutex_init (mutex, &mattr);
}
