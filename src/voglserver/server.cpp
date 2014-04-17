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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <vogl_core.h>
#include <vogl_json.h>

#include <iomanip>
#include <sstream>

 #include <pthread.h>

#include "../common/SimpleOpt.h"

#include "../common/vogllogging.h"
#include "../common/channel.h"

#include "../common/commands.h"
#include "../common/launchsteamgame.h"
#include "../common/toclientmsg.h"
#include "../common/listfiles.h"

enum
{
    OPT_HELP = 0,
    OPT_PORT,
    OPT_MAX
};

CSimpleOpt::SOption g_rgOptions[] =
{
    //  Prints out help for these command line parameters.
    { OPT_HELP, "-?", SO_NONE },
    { OPT_HELP, "-h", SO_NONE },
    { OPT_HELP, "-help", SO_NONE },
    { OPT_HELP, "--help", SO_NONE },

    //  The network port the server should listen to.
    { OPT_PORT, "-p", SO_REQ_SEP },
    { OPT_PORT, "-port", SO_REQ_SEP },
    { OPT_PORT, "--port", SO_REQ_SEP },

    SO_END_OF_OPTIONS
};

bool g_KeepRunning = true;
char *g_processName = NULL;
network::channelmgr *g_clientChannelMgr = NULL;
network::channelmgr *g_gameChannelMgr = NULL;
bool g_fAttemptingGameConnection = false;

void ShowUsage(char *szAppName);
static const char *GetLastErrorText(int a_nError);

void send_status_to_client(const char *message);

void ProcessCommand(void *callbackParam, unsigned int cbData, char *pbData);

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    network::CHEC ec = network::EC_NONE;

    CSimpleOpt args(argc, argv, g_rgOptions);
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
            {
                ShowUsage(argv[0]);
                return 0;
            }

            case OPT_PORT:
            {
                sscanf(args.OptionArg(), "%d", &port); //or atoi()
                printf("Server configured to bind to port %d\n", port);
                break;
            }

            default:
            {
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
    }

    g_processName = argv[0];

    {
        g_clientChannelMgr = new network::channelmgr();
        if (NULL == g_clientChannelMgr)
        {
            syslog(VOGL_ERROR, "%s:%d %s Unable to allocation networking channelmgr.  OOM?\n", __FILE__, __LINE__, __func__);
            return -1;
        }

        //  Create a client channel
        ec = g_clientChannelMgr->Accept( port, (RECVASYNC|SENDASYNC), false,  ProcessCommand, NULL, NULL, NULL );
        if (network::EC_NONE != ec)
        {
            syslog(VOGL_ERROR, "%s:%d %s Unable to listen on port %d\nExiting.\n", __FILE__, __LINE__, __func__, port);
            return -1;
        }

        //
        //  Loop 'til dead?
        int cLoop = 0;
        while (g_KeepRunning)
        {

            sleep(1);
            cLoop++;
            if (0 == (cLoop%10))
                send_status_to_client("Server is still alive.");
        }

    }

    return 0;
}

void ShowUsage(char *szAppName)
{
    printf("Usage: %s [Options]\n\n", szAppName);
    printf("Where Options are:\n\n");

    printf("Setting Ports:\n");
    printf("[Optional] Specifies the port that this server should listen to for connections.  The default value is %d.\n", DEFAULT_PORT);
    printf("-p <portNumber>\n");
    printf("-port <portNumber>\n");
    printf("--port <portNumber>\n\n");

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



void *connect_to_launched_game(void *arg);

int g_gameport = TRACE_PORT;
void ProcessCommand(void * /*callbackParam*/, unsigned int buffer_size, char *buffer)
{

    int32_t command = -1;
    pthread_t threadGameConnect = 0;

    int ec = 0;

    command = *(int32_t *)buffer;

    unsigned int buffer_size_temp = buffer_size - sizeof(int32_t);
    char *buffer_temp = buffer + sizeof(int32_t);

    switch (command)
    {
        case LAUNCHSTEAMGAME:
        {

            //send_status_to_client("Attempting to launch game.");
            ec = LaunchSteamGame(buffer_size_temp, buffer_temp);
            if (0 > ec)
            {
                //  Failed to parse?
                syslog(VOGL_ERROR, "%s:%d %s Unable to parse LaunchSteamGame json\n", __FILE__, __LINE__, __func__);
                send_status_to_client("Failed to launch game. OOM.");
                break;
            }

            if (0 != ec)
                g_gameport = ec;

            send_status_to_client("Connecting to game.");

            if (NULL == g_gameChannelMgr && !g_fAttemptingGameConnection)
            {
                g_fAttemptingGameConnection = true;
                ec = pthread_create(&threadGameConnect, NULL, connect_to_launched_game, NULL);
                if (0 != ec)
                    g_fAttemptingGameConnection = false;
            }
            break;
        }

        case TRACE_LIST_TRACES:
        {
            network::CHEC chec = network::EC_NONE;
            unsigned int buffer_size_list = 0;
            char *buffer_list = NULL;
            //  The command just is.  We simply send back the list of files.
            //
            ec = ListTraceFiles(&buffer_size_list, &buffer_list);
            if (0 != ec)
                syslog(VOGL_ERROR, "%s:%d %s Unable to create trace list message (%d)\n", __FILE__, __LINE__, __func__, command);

            chec = g_clientChannelMgr->SendData(buffer_size_list, buffer_list);
            if (network::EC_NONE != chec)
            {
                syslog(VOGL_ERROR, "%s:%d %s Unable to send trace list message (%d)\n", __FILE__, __LINE__, __func__, chec);
            }

            if (NULL != buffer_list)
                free(buffer_list);

            break;
        }

        default:
        {
            //  Don't know what this is, so just forwarding on the original buffer to the game.
            if (NULL != g_gameChannelMgr)
            {
                network::CHEC chec = network::EC_NONE;

                //syslog(VOGL_INFO, "%s:%d %s Sending message (%d) on to game\n", __FILE__, __LINE__, __func__, command);

                chec = g_gameChannelMgr->SendData(buffer_size, buffer);
                if (network::EC_NONE != chec)
                {
                    syslog(VOGL_ERROR, "%s:%d %s Unable to send message (%d) on to game\n", __FILE__, __LINE__, __func__, command);
                }
            }

            break;
        }
    }

    return;

}

void send_status_to_client(const char *message)
{
    int ec = 0;
    unsigned int buffer_size = 0;
    char *buffer = NULL;
    //
    //  Prepare message to send

    ec = ToClientMsgReq(g_processName, 0, message, &buffer_size, &buffer);
    if (0 != ec)
    {
        syslog(VOGL_INFO, "ToClientMsgReq error: %d\n", ec);
    }

    (void)g_clientChannelMgr->SendData(buffer_size, buffer);

    if (buffer)
        free(buffer);

    return;
}

void process_command_from_game(void * /*callbackParam*/, unsigned int buffer_size, char *buffer);

void *connect_to_launched_game(void * /*arg*/)
{

    network::channelmgr *gameChannelMgr = NULL;
    network::CHEC ec = network::EC_NONE;
    pthread_t thisThread = pthread_self();
    int err = 0;

    err = pthread_setname_np(thisThread, "cnt2game");
    if (0 != err)
    {
        syslog(VOGL_ERROR, "%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    gameChannelMgr = new network::channelmgr();
    if (NULL == gameChannelMgr)
        goto out;

    //ec = gameChannelMgr->Connect( const_cast<char *>("localhost"), g_gameport, (SENDASYNC|RECVASYNC), process_command_from_game, NULL );
    ec = gameChannelMgr->Accept( g_gameport, (RECVASYNC|SENDASYNC), true,  process_command_from_game, NULL, NULL, NULL );
    if (network::EC_NONE != ec)
    {
        delete gameChannelMgr;
        syslog(VOGL_ERROR, "%s:%d %s Unable to listen to game on port %d\n", __FILE__, __LINE__, __func__, g_gameport);
        goto out;
    }

    /*
    if (NULL != g_gameChannelMgr)
    {
        delete g_gameChannelMgr;
        g_gameChannelMgr = NULL;
    }
    */

    g_gameChannelMgr = gameChannelMgr;
out:
    g_fAttemptingGameConnection = false;

    return NULL;
}

void process_command_from_game(void * /*callbackParam*/, unsigned int buffer_size, char *buffer)
{
    network::CHEC chec = network::EC_NONE;

    //syslog(VOGL_INFO, "%s:%d %s Sending message on to client\n", __FILE__, __LINE__, __func__);

    chec = g_clientChannelMgr->SendData(buffer_size, buffer);
    if (network::EC_NONE != chec)
    {
        syslog(VOGL_ERROR, "%s:%d %s Unable to send message on to client\n", __FILE__, __LINE__, __func__);
    }

    return;
}

