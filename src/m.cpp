#include "GarrysMod/Lua/Interface.h"

#include "m.h"
#include "int_memory.h"
#include "str.h"
#include "io.h"
#include "nthread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>

using namespace GarrysMod::Lua;

namespace NX
{

  pthread_t dump_thread;
  mqueue msq;
  FILE *cl;
  lctx lcontext;

  pthread_mutex_t global_mutex;
  bool GlobalShutdown = false;

  char ftag[128] = "enginelog";

#define PT_EXIT(va) pthread_mutex_unlock (&queue->mutex); return va;

  static const char *
  PopMsgQ (pmqueue queue)
  {
    pthread_mutex_lock (&queue->mutex);

    p_md_obj ptr = queue->queue.first;

    if (0 == queue->queue.offset ||
    NULL == ptr)
      {
	PT_EXIT(NULL)
      }

    const char *data = (const char*) ptr->ptr;

    md_unlink (&queue->queue, ptr);

    pthread_mutex_unlock (&queue->mutex);

    return data;
  }

  static void
  PushMsgQ (pmqueue queue, const char *msg)
  {
    pthread_mutex_lock (&queue->mutex);

    if (queue->queue.offset > MSG_QUEUE_MAX)
      {
	PT_EXIT()
      }

    size_t l = strlen (msg) + 1;

    void *ptr = (void *) malloc (l);

    if ( NULL == ptr)
      {
	PT_EXIT()
      }

    memcpy (ptr, (const void *) msg, l);

    md_alloc (&queue->queue, 0, 0, ptr);

    pthread_mutex_unlock (&queue->mutex);

    pthread_kill (dump_thread, SIGUSR1);

  }

  static size_t
  GetQueueCount (pmqueue queue)
  {
    pthread_mutex_lock (&queue->mutex);

    size_t res = queue->queue.offset;

    pthread_mutex_unlock (&queue->mutex);

    return res;
  }

  static lctx
  CreateNewLogInstance ()
  {
    lctx ctx;

    ctx.fh = NULL;

    self_get_path ("srcds_linux", (char*) ctx.base_path);
    g_dirname (ctx.base_path);

    char b[PATH_MAX];
    snprintf (b, sizeof(b), "%s/logs", ctx.base_path);

    if (dir_exists (b))
      {
	if (mkdir (b, S_IRWXU) == -1)
	  {
	    STRERR()
	    fprintf (stderr,
		     "ENGINELOG: failed to create base path [%s]: '%s'\n",
		     errno_str, b);
	    pthread_mutex_unlock (&global_mutex);
	    return ctx;
	  }
      }

    snprintf (ctx.path, sizeof(ctx.path), "%s/%s.%u.log", b, ftag,
	      (unsigned int) time (NULL));

    ctx.fh = fopen (ctx.path, "a");

    return ctx;
  }

  static void
  CloseLogInstance (lctx *ctx)
  {
    if ( NULL != ctx->fh)
      {
	fclose (ctx->fh);
	ctx->fh = NULL;
      }
  }

  static void
  DumpToDrive (lctx *ctx, const char *msg)
  {
    size_t msglen = strlen (msg);

    size_t w = fwrite ((const void *) msg, msglen, 1, ctx->fh);

    if (w == 1)
      {
	return;
      }

    if (ferror (ctx->fh))
      {
	printf ("ENGINELOG: [%d] write error occured\n", fileno (ctx->fh));
	return;
      }

    if (feof (ctx->fh))
      {
	printf ("ENGINELOG: [%d]: Warning, EOF occured on log [%zu]\n",
		fileno (ctx->fh), w);
	return;
      }

  }

  static bool
  IsShuttingDown ()
  {
    pthread_mutex_lock (&global_mutex);
    bool res = GlobalShutdown;
    pthread_mutex_unlock (&global_mutex);

    return res;
  }

#define CONTNULL() if ( ferror (ctx->fh) ) continue;

  static void*
  LoggerDumpThread (void *data)
  {
    lctx *ctx = (lctx*) data;

    const char *msg;
    while (!IsShuttingDown ())
      {
	if (GetQueueCount (&msq) > 0)
	  {
	    usleep (10000);
	  }
	else
	  {
	    sleep (-1);
	  }

	while ((msg = PopMsgQ (&msq)))
	  {
	    if (ftell (ctx->fh) > MAX_LOG_FILE_SIZE || ferror (ctx->fh))
	      {
		CloseLogInstance (ctx);
		*ctx = CreateNewLogInstance ();
		CONTNULL()
	      }

	    DumpToDrive (ctx, msg);

	    free ((void*) msg);
	  }
      }
    return NULL;
  }

  int
  Engine::MsgInbound (lua_State* state)
  {

    if (!LUA->IsType (2, Type::STRING))
      {
	return 0;
      }

    const char *msg = LUA->GetString (2);

    PushMsgQ (&msq, msg);

    return 0;
  }

  static void
  sig_dummy_handler (int signal)
  {
  }

  static int
  InitializeGlobalContext ()
  {
    lcontext = CreateNewLogInstance ();

    if ( NULL == lcontext.fh)
      {
	fprintf (stderr, "ENGINELOG: could not open log handle [ %s ]\n",
		 lcontext.path);
	return 1;
      }

    return 0;
  }

  static void
  DisableLogging (lua_State * state)
  {
    LUA->PushSpecial (GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField (-1, "hook");
    LUA->GetField (-1, "Remove");
    LUA->PushString ("EngineSpew");
    LUA->PushString ("CPP_ENGINELOG::Spew");
    LUA->Call (2, 0);
    LUA->Pop ();
  }

  static void
  _EnableLogging (lua_State * state)
  {
    LUA->PushSpecial (GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField (-1, "hook");
    LUA->GetField (-1, "Add");
    LUA->PushString ("EngineSpew");
    LUA->PushString ("CPP_ENGINELOG::Spew");
    LUA->PushCFunction (Engine::MsgInbound);
    LUA->Call (3, 0);
    LUA->Pop ();
  }

  int
  Engine::EnableLogging (lua_State* state)
  {
    if (!LUA->IsType (1, Type::BOOL))
      {
	return 0;
      }

    if ( LUA->GetBool (1) == true)
      {
	_EnableLogging (state);
      }
    else
      {
	DisableLogging (state);
      }

    return 0;
  }

  int
  Engine::SetFileNamePrefix (lua_State* state)
  {

    return 0;
  }

  int
  Engine::GetFileName (lua_State* state)
  {
    LUA->PushString (lcontext.path);
    return 1;
  }

  int
  Engine::Initialize (lua_State* state)
  {

    if (InitializeGlobalContext ())
      {
	return 0;
      }

    mutex_init (&global_mutex, PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ROBUST);
    mutex_init (&msq.mutex, PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ROBUST);

    signal (SIGUSR1, sig_dummy_handler);

    md_init (&msq.queue);
    msq.queue.flags |= F_MDA_REFPTR;


    int r;

    if ((r = pthread_create (&dump_thread, NULL, LoggerDumpThread, &lcontext)))
      {
	fprintf (stderr, "ENGINELOG: dump thread failed to start: %d\n", r);
	return 0;
      }

    LUA->PushSpecial (GarrysMod::Lua::SPECIAL_GLOB);	// Push global table
    LUA->CreateTable ();
    LUA->SetField (-2, "ELog");
    LUA->Pop ();

    LUA->PushSpecial (GarrysMod::Lua::SPECIAL_GLOB);
    LUA->GetField (-1, "ELog");
    ADDFUNC("EnableLogging", Engine::EnableLogging)
    ADDFUNC("GetFileName", Engine::GetFileName)
    LUA->Pop ();

    return 0;
  }

  int
  Engine::Shutdown (lua_State* state)
  {
    DisableLogging (state);

    pthread_mutex_lock (&global_mutex);
    GlobalShutdown = 1;
    pthread_mutex_unlock (&global_mutex);

    pthread_kill (dump_thread, SIGUSR1);
    pthread_join (dump_thread, NULL);

    CloseLogInstance (&lcontext);

    md_free (&msq.queue);

    signal (SIGUSR1, SIG_DFL);

    return 0;
  }

}
