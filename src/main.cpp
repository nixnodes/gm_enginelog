#include "GarrysMod/Lua/Interface.h"
#include <stdio.h>

#include "m.h"

using namespace GarrysMod::Lua;
using namespace NX;

Engine* nx_engine = NULL;

GMOD_MODULE_OPEN()
{

	if ( nx_engine == NULL ) {
		nx_engine = new Engine();
		nx_engine->Initialize(state);
	} else {
		printf("nx engine already initialized\n");
	}
	
	return 0;
}

GMOD_MODULE_CLOSE()
{
	if ( nx_engine != NULL ) {
		nx_engine->Shutdown(state);
		delete nx_engine;
		nx_engine = NULL;
		return 0;
	} else {
		return 0;
	}
}
