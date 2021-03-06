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
  gctx gcontext;

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
    LOCK(&queue->mutex, size_t res = queue->queue.offset)
    return res;
  }

  static lctx
  CreateNewLogInstance (int inst)
  {

    lctx ctx;

    ctx.fh = NULL;
    ctx.inst = inst;

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
	    return ctx;
	  }
      }

    char tb[255];
    time_t t_t = (time_t) time (NULL);
    strftime (tb, sizeof(tb), "%d-%m-%y", localtime (&t_t));

    if (inst > 0)
      {
	snprintf (ctx.path, sizeof(ctx.path), "%s/%s.%s-%d.log", b, ftag, tb,
		  inst);
      }
    else
      {
	snprintf (ctx.path, sizeof(ctx.path), "%s/%s.%s.log", b, ftag, tb);
      }

    ctx.fh = fopen (ctx.path, "a");

    if ( NULL != ctx.fh)
      {
	char l[PATH_MAX];
	snprintf (l, sizeof(l), "%s/engine.log", b);

	if (symlink_exists (l))
	  {
	    unlink (l);
	  }

	if (symlink (ctx.path, l) == -1)
	  {
	    STRERR()
	    fprintf (stderr,
		     "ENGINELOG: failed to create symlink [%s]: '%s' -> '%s'\n",
		     errno_str, ctx.path, l);
	  }
      }

    lcontext.last_nl = 1;

    return ctx;
  }

  static void
  CloseLogInstance (lctx *ctx)
  {

    if ( NULL != ctx->fh)
      {
	fflush (ctx->fh);
	fclose (ctx->fh);
	ctx->fh = NULL;
      }
  }

  static int
  Write (lctx *ctx, const char *msg, size_t msglen)
  {
    size_t w = fwrite ((const void *) msg, msglen, 1, ctx->fh);

    if (w != 1)
      {
	if (ferror (ctx->fh))
	  {
	    printf ("ENGINELOG: [%d] write error occured\n", fileno (ctx->fh));
	    return 1;
	  }

	if (feof (ctx->fh))
	  {
	    printf ("ENGINELOG: [%d]: Warning, EOF occured on log [%zu]\n",
		    fileno (ctx->fh), w);
	    return 1;
	  }
      }
    else
      {
	return 0;
      }

    return 2;
  }

  static void
  DumpToDrive (lctx *ctx, const char *msg)
  {
    if (ctx->last_nl)
      {
	ctx->last_nl = 0;

	char tb[255];
	time_t t_t = (time_t) time (NULL);

	LOCK(&gcontext.global_mutex,
	     strftime (tb, sizeof(tb), gcontext.tfmt_string, localtime (&t_t)))

	if (Write (ctx, tb, strlen (tb)))
	  {
	    return;
	  }
      }

    if (strrchr (msg, 0xA))
      {
	ctx->last_nl = 1;
      }

    size_t msglen = strlen (msg);

    if (Write (ctx, msg, msglen))
      {
	return;
      }

    fflush (ctx->fh);

  }

  static bool
  IsShuttingDown ()
  {
    LOCK(&gcontext.global_mutex, bool res = gcontext.GlobalShutdown)
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
	    usleep (1000);
	  }
	else
	  {
	    sleep (-1);
	  }

	while ((msg = PopMsgQ (&msq)))
	  {
	    if (ftell (ctx->fh) > MAX_LOG_FILE_SIZE || ferror (ctx->fh))
	      {
		int cinst = ctx->inst;
		CloseLogInstance (ctx);
		*ctx = CreateNewLogInstance (cinst + 1);

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
  InitializeLogContext ()
  {
    lcontext = CreateNewLogInstance (0);

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
  Engine::SetTimeFormat (lua_State* state)
  {
    if (!LUA->IsType (1, Type::STRING))
      {
	return 0;
      }

    const char *format = LUA->GetString (1);

    if (!strlen (format))
      {
	return 0;
      }

    LOCK(
	&gcontext.global_mutex,
	snprintf (gcontext.tfmt_string, sizeof(gcontext.tfmt_string), "[ %s ] ",
		  format))

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

    if (InitializeLogContext ())
      {
	return 0;
      }

    strcpy (gcontext.tfmt_string, "[ %F %T ] ");

    mutex_init (&gcontext.global_mutex, PTHREAD_MUTEX_RECURSIVE,
		PTHREAD_MUTEX_ROBUST);
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
    ADDFUNC("SetTimeFormat", Engine::SetTimeFormat)
    LUA->Pop ();

    return 0;
  }

  int
  Engine::Shutdown (lua_State* state)
  {
    DisableLogging (state);

    LOCK(&gcontext.global_mutex, gcontext.GlobalShutdown = 1)

    pthread_kill (dump_thread, SIGUSR1);
    pthread_join (dump_thread, NULL);

    CloseLogInstance (&lcontext);

    md_free (&msq.queue);

    signal (SIGUSR1, SIG_DFL);

    return 0;
  }

}
