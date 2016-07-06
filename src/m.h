#ifndef __H_STDCAP_O_STDCAP
#define __H_STDCAP_O_STDCAP

#include "GarrysMod/Lua/Interface.h"

#include <pthread.h>

#include "int_memory.h"

#define MSG_QUEUE_MAX 25000

#define MAX_LOG_FILE_SIZE (1024 * 1024) * 5

#define DEBUGPRINT(msg) LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB); LUA->GetField(-1, "print"); LUA->PushString(msg); LUA->Call(1, 0); LUA->Pop();
#define ADDFUNC(fn, f) LUA->PushCFunction(f); LUA->SetField(-2, fn);

#include <pthread.h>

typedef struct _msg_queue_chunk
{
  void *data;

} mq_data, *pmq_data;

typedef struct msg_queue
{
  mda queue;
  pthread_mutex_t mutex;
} mqueue, *pmqueue;

#include <limits.h>

typedef struct log_context
{
  char base_path[PATH_MAX];
  char path[PATH_MAX];
  FILE *fh;
} lctx, *plctx;

#define STRERR() char errno_str[1024]; strerror_r(errno, errno_str, sizeof(b));

namespace NX
{
  class Engine
  {
  public:
    int
    Initialize (lua_State* luaState);
    int
    Shutdown (lua_State* luaState);

    static int
    MsgInbound (lua_State* state);
    static int
    EnableLogging (lua_State* state);
    static int
    SetFileNamePrefix (lua_State* state);
    static int GetFileName (lua_State* state);
  };
}

#endif
