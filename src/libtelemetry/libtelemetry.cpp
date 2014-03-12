/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

//
// libtelemetry.cpp
//
#include "libtelemetry.h"
#include "vogl_core.h"
#include "vogl_command_line_params.h"

#if !defined( USE_TELEMETRY )
#error "libtelemetry.cpp should not be built when USE_TELEMETRY is not defined."
#endif

telemetry_ctx_t g_tminfo;

// How much memory we want Telemetry to use. Mr. Hook says 2 megs is optimal.
static const size_t g_telemetry_arena_size = 2 * 1024 * 1024;

static struct
{
    bool loaded;          // Telemetry is loaded?
    HTELEMETRY ctx;       // Telemetry context.

    int level;            // Telemetry level.
    int new_level;        // New Telemetry level to set (on next telemetry_tick)
    char servername[256]; // servername (defaults to localhost)
    char appname[256];    // application name.
    
    // False if you want to use the release mode DLL or true if you want to
    // use the checked DLL.  The checked DLL is compiled with optimizations but
    // does extra run time checks and reporting.
    bool use_checked_dll;
    
    TmU8 *mem_arena;      // allocated mem arena.
} g_tmdata;

/*
 * Initialize Telemetry
 */
static bool
telemetry_initialize()
{
	TmErrorCode retval;

	if (g_tmdata.ctx)
	{
        // If we've already got a context, check if it's connected.
		TmConnectionStatus status = tmGetConnectionStatus(g_tmdata.ctx);
		if (status == TMCS_CONNECTED || status == TMCS_CONNECTING)
			return true;
	}

	if (!g_tmdata.loaded)
	{
		/* int load_telemetry = */ tmLoadTelemetry( g_tmdata.use_checked_dll );

		retval = tmStartup();
		if (retval != TM_OK)
		{
            // Warning( "TelemetryInit() failed: tmStartup() returned %d, tmLoadTelemetry() returned %d.\n", retval, load_telemetry );
			return false;
		}

		if (!g_tmdata.mem_arena)
		{
            // Allocate mem arena.
			g_tmdata.mem_arena = new TmU8[g_telemetry_arena_size];
		}

        // Initialize our context.
		retval = tmInitializeContext(&g_tmdata.ctx, g_tmdata.mem_arena, g_telemetry_arena_size);
		if ( retval != TM_OK )
		{
			delete [] g_tmdata.mem_arena;
			g_tmdata.mem_arena = NULL;

            // Warning( "TelemetryInit() failed: tmInitializeContext() returned %d.\n", retval );
			return false;
		}

		g_tmdata.loaded = true;
	}

	const char *app_name = telemetry_get_appname();
	const char *server_address = telemetry_get_servername();
	TmConnectionType server_type = TMCT_TCP;

	char build_info[2048];
    vogl::dynamic_string cmdline = vogl::get_command_line();
	snprintf( build_info, sizeof( build_info ), "%s: %s", __DATE__ __TIME__, cmdline.c_str() );
	build_info[ sizeof( build_info ) - 1 ] = 0;

	TmU32 open_flags = TMOF_DEFAULT | TMOF_MINIMAL_CONTEXT_SWITCHES;
	/* open_flags |= TMOF_DISABLE_CONTEXT_SWITCHES | TMOF_INIT_NETWORKING*/

	retval = tmOpen( g_tmdata.ctx, app_name, build_info, server_address, server_type,
		TELEMETRY_DEFAULT_PORT, open_flags, 1000 );
	if ( retval != TM_OK )
	{
        // Warning( "TelemetryInitialize() failed: tmOpen returned %d.\n", retval );
		return false;
	}

	// printf( "Telemetry initialized at level %u.\n", g_tmdata.level );
	return true;
}

/*
 * Shutdown Telemetry
 */
static void
telemetry_shutdown()
{
    HTELEMETRY ctx = g_tmdata.ctx;

	if (ctx)
	{
		TmConnectionStatus status = tmGetConnectionStatus(ctx);
		if (status == TMCS_CONNECTED || status == TMCS_CONNECTING)
			tmClose(ctx);

        // Clear all our global ctx levels.
		memset(get_tminfo().ctx, 0, sizeof(get_tminfo().ctx));
		g_tmdata.ctx = NULL;

        // Sleep for one second and hopefully all the background threads will be done
        // with their contexts? If not, we'll most likely crash. Not sure what else we
        // can do here other than just pause telemetry perhaps?
        sleep(1);

		tmShutdownContext( ctx ); 

        delete [] g_tmdata.mem_arena;
        g_tmdata.mem_arena = NULL;

		tmShutdown();
		g_tmdata.loaded = false;
	}
}

class CTelemetryShutdown
{
public:
	CTelemetryShutdown() 	{}
	~CTelemetryShutdown() 	{ telemetry_shutdown(); }
} g_TelemetryShutdown;

SO_API_EXPORT void
telemetry_tick()
{
	if (g_tmdata.ctx)
	{
		tmTick(g_tmdata.ctx);
	}

    // If the level has changed, reinit things.
	if (g_tmdata.level != g_tmdata.new_level)
	{
        static const int max_levels = (int)(sizeof(get_tminfo().ctx) / sizeof(get_tminfo().ctx[0]));
        if (g_tmdata.new_level > max_levels)
            g_tmdata.new_level = max_levels;

		g_tmdata.level = g_tmdata.new_level;
		memset(get_tminfo().ctx, 0, sizeof(get_tminfo().ctx));

		if (g_tmdata.level == -1)
		{
            // Should we just pause or try to shutdown entirely?
            //  Try shutdown for now...
            // tmPause(g_tmdata.ctx, 1);
			telemetry_shutdown();
		}
		else
		{
			if (!telemetry_initialize())
			{
				g_tmdata.level = -1;
			}
			else
			{
				tmPause(g_tmdata.ctx, 0);

                for (int i = 0; i <= g_tmdata.level; i++)
                {
                    get_tminfo().ctx[i] = g_tmdata.ctx;
				}
			}
		}

        g_tmdata.new_level = g_tmdata.level;
	}
}

SO_API_EXPORT const char *
telemetry_get_servername()
{
    if (!g_tmdata.servername[0])
        return "localhost";
    return g_tmdata.servername;
}

SO_API_EXPORT void 
telemetry_set_servername(const char *servername)
{
    if (servername)
        strncpy(g_tmdata.servername, servername, sizeof(g_tmdata.servername));
    else
        g_tmdata.servername[0] = 0;
}

SO_API_EXPORT const char *
telemetry_get_appname()
{
    if (!g_tmdata.appname[0])
    {
        vogl::dynamic_string_array params(vogl::get_command_line_params());
        if (params.size())
        {
            char *appname = basename(params[0].c_str());
            if (appname)
                strncpy(g_tmdata.appname, appname, sizeof(g_tmdata.appname));
        }
    }
    if (!g_tmdata.appname[0])
        return "AppName";
    return g_tmdata.appname;
}

SO_API_EXPORT void 
telemetry_set_appname(const char *appname)
{
    if (appname)
        strncpy(g_tmdata.appname, appname, sizeof(g_tmdata.appname));
    else
        g_tmdata.appname[0] = 0;
}

SO_API_EXPORT int
telemetry_get_level()
{
    // If we've never loaded telemetry, our level is -1.
    if (!g_tmdata.loaded)
        g_tmdata.level = -1;

    return g_tmdata.level;
}

SO_API_EXPORT void
telemetry_set_level(int level)
{
    // If we've never loaded telemetry, our level is -1.
    if (!g_tmdata.loaded)
        g_tmdata.level = -1;

    // Set our new level.
    g_tmdata.new_level = level;
}
