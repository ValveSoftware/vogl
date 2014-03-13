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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <argp.h>
#include <libgen.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

/*
 * To kill dialog asking if you want to launch the game:
 * You also need a steam_dev.cfg in your steam install folder (next to steam.sh) with:
 * bPromptGameLaunchBypass 1
 *
 * in it so you don't get a dialog bugging you before letting the game run.
 */

static const struct
{
    int id;
    const char *name;
} g_gameids[] =
{
    { 214910,   "AirConflicts" },
    { 400,      "Portal1" },
    { 218060,   "BitTripRunner" },
    { 570,      "Dota2" },
    { 35720,    "Trine2" },
    { 440,      "TF2" },
    { 41070,    "Sam3" },
    { 1500,     "Darwinia" },
    { 550,      "L4D2" },
    { 1500,     "Darwinia2" },
    { 570,      "Dota2Beta" },
    { 221810,   "TheCave" },
    { 220200,   "KerbalSpaceProgram" },
    { 44200,    "GalconFusion"},
    { 201040,   "GalconLegends"},
    { 25000,    "Overgrowth"},
    { 211820,   "Starbound"} // 64-bit game
};

#define F_VERBOSE     0x00000001
#define F_AMD64       0x00000002

static bool g_dryrun = false;
static bool g_debugspew = false;

struct arguments_t
{
    unsigned int flags;
    std::string cmdline;
    std::string vogl_tracefile;
    std::string gameid;
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
    arguments_t *arguments = (arguments_t *)state->input;

    switch (key)
    {
    case '?':
        fprintf(stderr, "\nGameIDS:\n");
        for(size_t i = 0; i < sizeof(g_gameids) / sizeof(g_gameids[0]); i++)
        {
            fprintf(stderr, "  %-6d - %s\n", g_gameids[i].id, g_gameids[i].name);
        }
        printf("\n");
        argp_state_help(state, stderr, ARGP_HELP_LONG | ARGP_HELP_EXIT_OK);
        break;
    case -1:
        arguments->cmdline += " ";
        arguments->cmdline += state->argv[state->next - 1];
        break;
    case 'i': arguments->gameid = arg; break;
    case 'f': arguments->vogl_tracefile = arg; break;
    case 'v': arguments->flags |= F_VERBOSE; break;
    case '6': arguments->flags |= F_AMD64; break;
    case 'y': g_dryrun = true; break;
    case 'd': g_debugspew = true; break;
        break;

    case -2:
        // --show-type-list: Whitespaced list of options for bash autocomplete.
        for (size_t i = 0; ; i++)
        {
            const char *name = state->root_argp->options[i].name;
            if (!name)
                break;
            printf("--%s ", name);
        }
        exit(0);
    }
    return 0;
}

void
errorf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    exit(-1);
}

std::string
url_encode(const std::string &value)
{
    std::ostringstream escaped;

    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
    {
        std::string::value_type c = (*i);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else if (c == ' ')
            escaped << "%20";
        else
            escaped << '%' << std::setw(2) << ((int) c) << std::setw(0);
    }

    return escaped.str();
}

int
main(int argc, char **argv)
{
    static struct argp_option options[] =
    {
        { "gameid",                     'i', "[GAMEID]",    0, "Steam game id. Ie, 400 for Portal." },
        { "vogl_tracefile",             'f', "[FILENAME]",  0, "Output trace's file name." },
        { "verbose",                    'v', 0,             0, "Produce verbose output" },
        { "amd64",                      '6', 0,             0, "Use 64-bit libvogltrace.so." },
        { "dry-run",                    'y', 0,             0, "Don't execute commands." },
        { "debug",                      'd', 0,             0, "Debug spew from LD_DEBUG=lib."},
        { "vogl_pause",                 -1,  0,             0, "Add --vogl_pause." },
        { "vogl_backtrace_all_calls",   -1,  0,             0, "Add --vogl_backtrace_all_calls." },
        { "show-type-list",             -2,  0,             0, "Produce list of whitespace-separated words used for command completion." },
        { "help",                       '?', 0,             0, "Give this help message." },
        { 0 }
    };

    arguments_t args;
    args.flags = 0;
    struct argp argp = { options, parse_opt, 0, "Steam Launcher helper app." };

    argp_parse(&argp, argc, argv, ARGP_NO_HELP, 0, &args);

    if (!args.gameid.size())
        errorf("ERROR: No gameid specified (--gameid ###).\n");

    bool is_steam_file = true;
    if (atoi(args.gameid.c_str()) == 0)
    {
        if (access(args.gameid.c_str(), F_OK) == 0)
        {
            char *filename = realpath(args.gameid.c_str(), NULL);
            if (filename)
            {
                // This is a local executable.
                is_steam_file = false;
                args.gameid = filename;
                free(filename);
            }
        }
        else
        {
            // lower case what the user gave us.
            std::transform(args.gameid.begin(), args.gameid.end(), args.gameid.begin(), ::tolower);
            // Try to find the name and map it back to the id.
            for (size_t i = 0; i < sizeof(g_gameids) / sizeof(g_gameids[0]); i++)
            {
                std::string name = g_gameids[i].name;
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name == args.gameid)
                {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", g_gameids[i].id);
                    args.gameid = buf;
                    break;
                }
            }
        }
    }

    int gameid = is_steam_file ? atoi(args.gameid.c_str()) : -1;
    if (!gameid)
        errorf("ERROR: Could not find game number for %s\n", args.gameid.c_str());

    std::string gameid_str;
    if (is_steam_file)
    {
        gameid_str = "gameid" + args.gameid;
        printf("\nGameID: %d", gameid);
        for (size_t i = 0; i < sizeof(g_gameids) / sizeof(g_gameids[0]); i++)
        {
            if (gameid == g_gameids[i].id)
            {
                printf(" (%s)", g_gameids[i].name);
                gameid_str = g_gameids[i].name;
                break;
            }
        }
        printf("\n");
    }
    else
    {
        gameid_str = basename((char *)args.gameid.c_str());
        printf("\nGame: %s\n", args.gameid.c_str());
    }

    if (!args.vogl_tracefile.size())
    {
        char timestr[200];
        time_t t = time(NULL);

        timestr[0] = 0;
        struct tm *tmp = localtime(&t);
        if (tmp)
        {
            strftime(timestr, sizeof(timestr), "%Y_%m_%d-%H_%M_%S", tmp);
        }

        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/vogltrace.%s.%s.bin", P_tmpdir, gameid_str.c_str(), timestr);
        args.vogl_tracefile = buf;
    }

    // Get the full path to our executable. It should be running in vogl/bin/x86_64 (or i386).
    char buf32[PATH_MAX];
    char buf64[PATH_MAX];
    if ((readlink("/proc/self/exe", buf32, sizeof(buf32)) <= 0))
        errorf("ERROR: readlink failed '%s.'\n", strerror(errno));

    // Get just the directory name and relative path to libvogltrace.so.
    std::string filedir = dirname(buf32);
    snprintf(buf32, sizeof(buf32), "%s/../../vogl_build/bin/libvogltrace32.so", filedir.c_str());
    snprintf(buf64, sizeof(buf64), "%s/../../vogl_build/bin/libvogltrace64.so", filedir.c_str());

    // Trim all the relative path parts.
    char *vogltracepath32 = realpath(buf32, NULL);
    char *vogltracepath64 = realpath(buf64, NULL);
    if (args.flags & F_AMD64)
    {
        if (!vogltracepath64)
            errorf("ERROR: realpath %s failed '%s.'\n", buf64, strerror(errno));
    }
    else
    {
        if (!vogltracepath32)
            errorf("ERROR: realpath %s failed '%s.'\n", buf32, strerror(errno));
    }

    // Make sure our libvogltrace.so exists.
    if (args.flags & F_AMD64)
    {
        if(access(vogltracepath64, F_OK) != 0)
            errorf("ERROR: %s file does not exist.\n", vogltracepath64);
    }
    else
    {
        if(access(vogltracepath32, F_OK) != 0)
            errorf("ERROR: %s file does not exist.\n", vogltracepath32);
    }


    // set up LD_PRELOAD string
    std::string LD_PRELOAD = "LD_PRELOAD=";
    if (args.flags & F_AMD64)
        LD_PRELOAD += vogltracepath64;
    else
        LD_PRELOAD += vogltracepath32;
        
    //LD_PRELOAD += ":";
    // LD_PRELOAD += vogltracepath64;
    if (is_steam_file || getenv("LD_PRELOAD"))
        LD_PRELOAD += ":$LD_PRELOAD";
    printf("\n%s\n", LD_PRELOAD.c_str());

    // set up VOGL_CMD_LINE string
    std::string VOGL_CMD_LINE = "VOGL_CMD_LINE=\"";
    VOGL_CMD_LINE += "--vogl_tracefile " + args.vogl_tracefile;
    VOGL_CMD_LINE += args.cmdline;
    VOGL_CMD_LINE += "\"";
    printf("\n%s\n", VOGL_CMD_LINE.c_str());

    std::string LD_DEBUG = "";
    if (g_debugspew)
    {
        LD_DEBUG += "LD_DEBUG=libs";
    }

    if (is_steam_file)
    {
        // set up xterm string
        const char *env_user = getenv("USER");
        std::string xterm_str = "xterm -geom 120x80+20+20";
        // If this is mikesart, specify using the Consolas font (which he likes).
        if (env_user && !strcmp(env_user, "mikesart"))
            xterm_str += " -fa Consolas -fs 10";

        // Add the command part.
        xterm_str += " -e %command%";
        printf("\nSTEAMCMD: %s\n", xterm_str.c_str());
    
        // set up steam string
        std::string steam_str = "steam steam://run/" + args.gameid + "//";
        std::string steam_args = VOGL_CMD_LINE + " " + LD_PRELOAD + " " + LD_DEBUG + " " + xterm_str;
        std::string steam_fullcmd = steam_str + url_encode(steam_args);
    
        // Spew this whole mess out.
        printf("\nURL encoded launch string:\n%s\n", steam_fullcmd.c_str());
    
        // And launch it...
        if (!g_dryrun)
            system(steam_fullcmd.c_str());
    }
    else
    {
        std::string system_cmd = VOGL_CMD_LINE + " " + LD_PRELOAD + " " + args.gameid;

        printf("\nlaunch string:\n%s\n", system_cmd.c_str());

        if (!g_dryrun)
            system(system_cmd.c_str());
    }

    free(vogltracepath32);
    free(vogltracepath64);
    return 0;
}
