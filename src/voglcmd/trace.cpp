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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#include <vogl_core.h>
#include <vogl_json.h>

#include "../common/SimpleOpt.h"
#include "../common/channel.h"

#include "../common/commands.h"
#include "../common/launchsteamgame.h"
#include "../common/toclientmsg.h"
#include "../common/pinggame.h"
#include "../common/listfiles.h"

enum
{
    OPT_HELP = 0,
    OPT_SERVER,
    OPT_PORT,
    OPT_GAMEID,
    OPT_BITNESS,
    OPT_MONITOR,
    OPT_CAPTURE,
    OPT_RETRIEVE,
    OPT_LISTGAMES,
    OPT_LISTTRACES,
    OPT_MAX
};


CSimpleOpt::SOption g_rgOptions[] =
    {
        { OPT_HELP,     "-?",         SO_NONE },
        { OPT_HELP,     "-h",         SO_NONE },
        { OPT_HELP,     "-help",      SO_NONE },
        { OPT_HELP,     "--help",     SO_NONE },

        { OPT_PORT,     "-p",         SO_REQ_SEP },
        { OPT_PORT,     "-port",      SO_REQ_SEP },
        { OPT_PORT,     "--port",     SO_REQ_SEP },

        { OPT_SERVER,   "-s",         SO_REQ_SEP },
        { OPT_SERVER,   "-server",    SO_REQ_SEP },
        { OPT_SERVER,   "--server",   SO_REQ_SEP },

        { OPT_GAMEID,   "--game",     SO_REQ_SEP },
        { OPT_GAMEID,   "-game",      SO_REQ_SEP },
        { OPT_GAMEID,   "-g",         SO_REQ_SEP },

        { OPT_LISTGAMES,   "--listgames",     SO_NONE },
        { OPT_LISTGAMES,   "-listgames",      SO_NONE },
        { OPT_LISTGAMES,   "-lg",             SO_NONE },

        { OPT_BITNESS,  "--bitness",  SO_REQ_SEP },
        { OPT_BITNESS,  "-bitness",   SO_REQ_SEP },
        { OPT_BITNESS,  "-b",         SO_REQ_SEP },

        { OPT_MONITOR,  "--monitor",  SO_NONE },
        { OPT_MONITOR,  "-monitor",   SO_NONE },
        { OPT_MONITOR,  "-m",         SO_NONE },

        { OPT_CAPTURE,  "--capture",  SO_REQ_SEP },
        { OPT_CAPTURE,  "-capture",   SO_REQ_SEP },
        { OPT_CAPTURE,  "-c",         SO_REQ_SEP },

        { OPT_RETRIEVE, "--retrieve", SO_REQ_SEP },
        { OPT_RETRIEVE, "-retrieve",  SO_REQ_SEP },
        { OPT_RETRIEVE, "-r",         SO_REQ_SEP },

        { OPT_LISTTRACES,   "--listtraces",     SO_NONE },
        { OPT_LISTTRACES,   "-listtraces",      SO_NONE },
        { OPT_LISTTRACES,   "-lt",             SO_NONE },

        SO_END_OF_OPTIONS
    };

void ShowUsage(char *szAppName);
static const char *GetLastErrorText(int a_nError);

void ctrlc_handler(int s) __attribute__ ((noreturn));

char g_localhost[] = "localhost";
bool g_fMonitor = false;
network::channelmgr *g_pChannelMgr = NULL;

void ProcessCommand(void *callbackParam, unsigned int cbData, char *pbData);

int
main_trace(int argc, char *argv[])
{
    char *szServer = g_localhost;
    struct sigaction sigIntHandler;
    int port = DEFAULT_PORT;
    network::CHEC ec = network::EC_NONE;
    bool fListTraces = false;

    char *gameid = NULL;
    int bitness = 32;

    CSimpleOpt args((argc-1), (++argv), g_rgOptions);
    while (args.Next())
    {
        if (args.LastError() != SO_SUCCESS)
        {
            printf("%s: '%s' (use --help to get command line help)\n",
                   GetLastErrorText(args.LastError()), args.OptionText());
            ShowUsage(argv[0]);
            return -1;
        }

        switch (args.OptionId())
        {

            case OPT_HELP:
                ShowUsage(argv[0]);
                return 0;

            case OPT_PORT:
                sscanf(args.OptionArg(), "%d", &port); //or atoi()
                printf("%s configured to connect to port %d\n", argv[0], port);
                break;

            case OPT_SERVER:
                szServer = args.OptionArg();
                printf("%s configured to connect to the server %s\n", argv[0], szServer);
                break;

            case OPT_GAMEID:
                gameid = args.OptionArg();
                break;

            case OPT_BITNESS:
                sscanf(args.OptionArg(), "%d", &bitness);
                if (bitness != 32 && bitness != 64)
                {
                    printf("The bitness of a game needs to be either 64-bit or 32-bit.  %d is unknown.\n\n", bitness);
                    ShowUsage(argv[0]);
                    return -1;
                }
                break;

            case OPT_MONITOR:
                g_fMonitor = true;
                break;

            case OPT_LISTTRACES:
                g_fMonitor = true; // Need to wait for the list of trace files to come back
                fListTraces = true;
                break;

            default:
                if (args.OptionArg())
                {
                    printf("option: %2d, text: '%s', arg: '%s'\n",
                           args.OptionId(), args.OptionText(), args.OptionArg());
                }
                else
                {
                    printf("option: %2d, text: '%s'\n",
                           args.OptionId(), args.OptionText());
                }
                return -1;
        }
    }

    if ((NULL == gameid) && (false == g_fMonitor) && (false == fListTraces))
    {
        // GameID or monitoring is required
        printf("Nothing has been asked of the client to do.  See help.\n\n");
        ShowUsage(argv[0]);
        return -1;
    }


    //  Setup Ctrl-C handling so we can shut down any active connections properly
    {
        sigIntHandler.sa_handler = ctrlc_handler;
        sigemptyset(&sigIntHandler.sa_mask);
        sigIntHandler.sa_flags = 0;

        sigaction(SIGINT, &sigIntHandler, NULL);
    }

    {
        unsigned int cbBuff = 0;
        char *pbBuff = NULL;
        char *vogl_cmd_params = getenv("VOGL_CMD_LINE");
        const char *gameport = NULL;

        int ret = 0;
        int cMonitorSleeps = 0;

        std::string strVoglCmdParams;

        if ( NULL != vogl_cmd_params)
            strVoglCmdParams += vogl_cmd_params;

        if (0 == strcmp(szServer, g_localhost))
        {
            //  For localhost, then use a different port for the server and game and then launch the server.
            gameport = "6125";
            //strVoglCmdParams += " --vogl_traceport ";
            //strVoglCmdParams += gameport; // This says what the game should listen on

            system("./voglserver64 -p 6120 2>/tmp/voglserver_stderr.txt 1>/tmp/voglserver_stdout.txt&");
            port = 6120;
        }

        network::channelmgr *pChannelMgr = NULL;

        pChannelMgr = new network::channelmgr();
        if (NULL == pChannelMgr)
        {
            printf("%s:%d %s Unable to allocation networking channelmgr.  OOM?\n", __FILE__, __LINE__, __func__);
            return 1;
        }

        //  Create a client channel
        ec = pChannelMgr->Connect( szServer, port, (SENDASYNC|RECVASYNC), ProcessCommand, NULL );
        if (network::EC_NONE != ec)
        {
            printf("%s:%d %s Unable to connect to server %s\nExiting.\n", __FILE__, __LINE__, __func__, szServer);
            return 1;
        }

        g_pChannelMgr = pChannelMgr;

        //
        //  Need to ensure that our recvasync thread is up and running...


        //  Launch a game
        //
        if (NULL != gameid)
        {
            ret = LaunchSteamGameReq(gameid, strVoglCmdParams.c_str(), bitness, gameport, &cbBuff, &pbBuff);
            if (0 != ret)
            {
                printf("%s:%d %s Unable to serialize Launch Game command (error = %x)\n", __FILE__, __LINE__, __func__, ret);
                return -1;
            }

            ec = pChannelMgr->SendData(cbBuff, pbBuff);
            if (network::EC_NONE != ec)
            {
                printf("%s:%d %s Client: Error sending message to server (%d): errno = %d\n", __FILE__, __LINE__, __func__, ec, errno);
            }

            free(pbBuff);
            pbBuff = NULL;
        }


        //
        //  List the available trace files
        //
        if (true == fListTraces)
        {
            ret = AskForTraceFileList(&cbBuff, &pbBuff);
            if (0 != ret)
            {
                printf("%s:%d %s Unable to serialize Listing Traces command (error = %x)\n", __FILE__, __LINE__, __func__, ec);
                return -1;
            }

            ec = pChannelMgr->SendData(cbBuff, pbBuff);
            if (network::EC_NONE != ec)
            {
                printf("%s:%d %s Client: Error sending Listing Traces message to server (%d): errno = %d\n", __FILE__, __LINE__, __func__, ec, errno);
            }

            free(pbBuff);
            pbBuff = NULL;
        }


        cMonitorSleeps = 0;
        while (g_fMonitor)
        {

            //  Every 20 seconds, ping for status from the game
            sleep(1);
            cMonitorSleeps++;

            if ((cMonitorSleeps % 20) == 0)
            {
                ret = PingGameReq(cMonitorSleeps, &cbBuff, &pbBuff);
                if (0 != ret)
                {
                    printf("%s:%d %s Unable to serialize ping command (error = %x)\n", __FILE__, __LINE__, __func__, ret);
                    continue;
                }

                ec = pChannelMgr->SendData(cbBuff, pbBuff);
                if (network::EC_NONE != ec)
                {
                    printf("%s:%d %s Client: Error sending message to game (%d): errno = %d\n", __FILE__, __LINE__, __func__, ec, errno);
                }

                free(pbBuff);
                pbBuff = NULL;
                cbBuff = 0;
            }
        }

        //(void) pChannelMgr->SendData(strlen("done"), "done");
        g_pChannelMgr = NULL;
        pChannelMgr->Disconnect();
        delete pChannelMgr;
    }

    return 0;
}


void ShowUsage(char *szAppName)
{
    printf("Usage: %s [Options]\n\n", szAppName);
    printf("Where Options are:\n\n");

    printf("Setting Ports:\n");
    printf("Specifies the port that this client should connect to.  The default value is %d.\n", DEFAULT_PORT);
    printf("-p <portNumber>\n");
    printf("-port <portNumber>\n");
    printf("--port <portNumber>\n\n");

    printf("Server name:\n");
    printf("Specifies the name of the server we want to connect to to start debugging.\n");
    printf("The default value is \"%s\".\n", g_localhost);
    printf("-s hostname\n");
    printf("-server hostname\n");
    printf("--server hostname\n\n");

    printf("Game:\n");
    printf("Identifies the game to be launched\nIt can identify the game by it's id or name.\n");
    printf("To see a list of games supported by name, use --listgames\n");
    printf("-g <game>\n");
    printf("-gameid <game>\n");
    printf("--gameid <game>\n\n");

    printf("ListGames:\n");
    printf("Lists the known set of names for games.  Use the appid of the game if you don't see your game listed here.\n");
    printf("-lg\n");
    printf("-listgames\n");
    printf("--listgames\n\n");

    printf("Bitness:\n");
    printf("Specifies if the game being launched is 64-bit or 32-bit (defaults to 32)\n");
    printf("-b [32|64]\n");
    printf("-bitness [32|64]\n");
    printf("--bitness [32|64]\n\n");

    printf("Monitor:\n");
    printf("Keep the client running to monitor the activity on the server\n");
    printf("-m\n");
    printf("-monitor\n");
    printf("--monitor\n\n");

    printf("Capture:\n");
    printf("Captures a trace file of the game/application being traced\n");
    printf("-c <framecount> <basefilename>\n");
    printf("-capture <framecount> <basefilename>\n");
    printf("--capture <framecount> <basefilename>\n");
    printf("Where, \n");
    printf("[required] <framecount> - is the number of frames to capture in the trace.\n");
    printf("[optional] <basefilename> - is the base file name for the trace file.\n\n");

    printf("Retrieve:\n");
    printf("Retrieves a remote capture file and brings it locally.\n");
    printf("If the file is already local, this doesn't do anything.\n");
    printf("-r <filename>\n");
    printf("-retrieve <filename>\n");
    printf("--retrieve <filename>\n");
    printf("Where, \n");
    printf("[required] <filename> - is the name of the file to bring locally.  This is just the filename\n");
    printf("  and does not include a path.  The filename will be looked for on the remote host in the default\n");
    printf("  location that traces are stored.\n\n");

    printf("ListTraces:\n");
    printf("Lists the set of traces available for download.\n");
    printf("-lt\n");
    printf("-listtraces\n");
    printf("--listtraces\n\n");

    printf("Help message:\n");
    printf("Gives this useful help message again.\n");
    printf("-?\n");
    printf("-h\n");
    printf("-help\n");
    printf("--help\n\n");
}

static const char *
GetLastErrorText(int a_nError)
{
    switch (a_nError)
    {
        case SO_SUCCESS:
            return ("Success");
        case SO_OPT_INVALID:
            return ("Unrecognized option");
        case SO_OPT_MULTIPLE:
            return ("Option matched multiple strings");
        case SO_ARG_INVALID:
            return ("Option does not accept argument");
        case SO_ARG_INVALID_TYPE:
            return ("Invalid argument format");
        case SO_ARG_MISSING:
            return ("Required argument is missing");
        case SO_ARG_INVALID_DATA:
            return ("Invalid argument data");
        default:
            return ("Unknown error");
    }
}

void ctrlc_handler(int s)
{
    printf("Caught signal %d\n",s);

    if (NULL != g_pChannelMgr)
    {
        g_pChannelMgr->Disconnect();
        delete g_pChannelMgr;

        g_pChannelMgr = NULL;
    }
    exit(1);
#ifdef NEVER
    g_fMonitor = false;  //  Tell the main loop to exit and shutdown

    return;
#endif // NEVER
}

void ProcessCommand(void * /*callbackParam*/, unsigned int buffer_size, char *buffer)
{
    int32_t command = -1;
    int ec = 0;

    command = *(int32_t *)buffer;

    unsigned int buffer_size_temp = buffer_size - sizeof(int32_t);
    char *buffer_temp = buffer + sizeof(int32_t);

    switch (command)
    {
        case TOCLIENTMSG:
        {
            ec = PrintToClientMsg(buffer_size_temp, buffer_temp);
            if (0 != ec)
            {
                //  Failed to parse?
                printf("Unable to parse TOCLIENTMSG json\n");
                break;
            }
            break;
        }

        case TRACE_LIST:
        {
            ec = DumpTraceFileList(buffer_size_temp, buffer_temp);
            if (0 != ec)
            {
                //  Failed to parse?
                printf("Unable to parse TRACE_LIST json\n");
                break;
            }
            break;
        }

        default:
        {
            printf("Unknown command sent to client: %d  Dropped.\n", command);
            break;
        }
    }

    return;
}

