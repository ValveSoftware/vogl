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

#ifdef VOGL_REMOTING
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include <pthread.h>

#include <vogl_core.h>
#include <vogl_json.h> 

#include "vogl_intercept.h"

#include "../common/vogllogging.h"
#include "../common/channel.h"

#include "../common/commands.h"
#include "../common/toclientmsg.h"
#include "../common/pinggame.h"


// Needs to be the same in the server gpusession code
int g_vogl_traceport = TRACE_PORT;
char g_procname[256] = { 0 };

void *invoke_listener(void *arg);

bool log_output_func(vogl::eConsoleMessageType type, const char *pMsg, void * /*pData*/);
void send_status_to_client(const char *message);
void send_status_to_client(const char *message, int token);

void vogl_init_listener(int vogl_traceport)
{
    pthread_t threadCmd;
    int err = 0;

    openlog("vogl-remote", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    //
    //  Get the process name  
    //    If, in the unlikely event, the name of the process doesn't fit, then readlink won't 0-terminate the string.
    //    We initialize it to all 0, so if it all got overwritten, then 0 out the last character in the string.
    //
    (void) readlink("/proc/self/exe", g_procname, sizeof(g_procname));
    g_procname[sizeof(g_procname)-1] = '\0';

    

    //  Launching a VOGL Listener
    if (0 == vogl_traceport)
    {
        vogl_traceport = TRACE_PORT;
        syslog(VOGL_INFO, "No port # passed in, %s listening on default port %d\n", g_procname, vogl_traceport);
    }

    syslog(VOGL_INFO, "Spinning up listener for vogl_remoting of %s on port %d...\n", g_procname, vogl_traceport);

    g_vogl_traceport = vogl_traceport;

    err = pthread_create(&threadCmd, NULL, invoke_listener, NULL);
    if (0 != err)
    {
        syslog(VOGL_ERROR, "%s:%d %s  Unable to create vogl_remoting cmd thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
        return;
    }

    err = pthread_setname_np(threadCmd, "vogl_remoting");
    if (0 != err)
    {
        syslog(VOGL_ERROR, "%s:%d %s  Unable to set the name of the vogl_remoting cmd thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
        return;
    }

    return;
}

void process_trace_command(void *callback_param, unsigned int buffer_size, char *buffer);

bool g_fKillApp = false;
network::channelmgr *g_clientChannelMgr = NULL;

void *
invoke_listener(void * /*arg*/) 
{
    //
    //  Setup to wait for a client to connect
    //
    pthread_t thisThread = pthread_self();
    network::CHEC chec = network::EC_NONE;

    int err = 0;

    err = pthread_setname_np(thisThread, "vogl_listen");
    if (0 != err)
    {
        syslog(VOGL_ERROR, "%s:%d %s Unable to set the name of the thread. returned %d, errno %d\n", __FILE__, __LINE__, __func__, err, errno);
    }

    syslog(VOGL_INFO, "Listener for VOGL Tracing in operation...\n");

    g_clientChannelMgr = new network::channelmgr();
    if (NULL == g_clientChannelMgr)
    {
        syslog(VOGL_ERROR, "%s:%d %s Unable to allocation networking channelmgr.  OOM?\n", __FILE__, __LINE__, __func__);
        return NULL;
    }

    //  Create a client channel
    chec = g_clientChannelMgr->Connect( const_cast<char *>("localhost"), g_vogl_traceport, (SENDASYNC|RECVASYNC), process_trace_command, NULL );
    //chec = g_clientChannelMgr->Accept( g_vogl_traceport, (RECVASYNC|SENDASYNC), true,  process_trace_command, NULL, NULL, NULL );
    if (network::EC_NONE != chec)
    {
        syslog(VOGL_ERROR, "%s:%d %s Unable to connect on port %d\nExiting.\n", __FILE__, __LINE__, __func__, g_vogl_traceport);
        return NULL;
    }

    //
    //  Tell the client we're up and running.
    send_status_to_client("Game is up and running.");

    //
    //  Start capturing output from the tracer
    vogl::console::add_console_output_func(log_output_func, NULL);

    while (true)
    {
        if (g_fKillApp)
        {
            break;
        }
        sleep(1);  //  Don't busy spin...
    }

    if (g_fKillApp)
    {
        syslog(VOGL_INFO, "%lx: VOGL Tracing server thread: Terminating the application.\n", thisThread);
        kill(getpid(), SIGTERM); // SIGINT);
    }

    syslog(VOGL_INFO, "%lx: VOGL Tracing server thread: Terminating.\n", thisThread);

    return NULL;
}

bool SetNullMode(unsigned int buffer_size, char *buffer);
bool SetDumpGLCalls(unsigned int buffer_size, char *buffer);
bool SetDumpGLBuffers(unsigned int buffer_size, char *buffer);
bool SetDumpGLShaders(unsigned int buffer_size, char *buffer);
bool SetBackTrace(unsigned int buffer_size, char *buffer);

bool KillTracer(unsigned int buffer_size, char *buffer);
int StartCapture(unsigned int buffer_size, char *buffer);
int StopCapture(unsigned int buffer_size, char *buffer);

extern bool g_null_mode;
extern bool g_backtrace_all_calls;
extern bool g_dump_gl_calls_flag;
extern bool g_dump_gl_buffers_flag;
extern bool g_dump_gl_shaders_flag;

//  Async calls to send contents to our client
//
void CaptureCompleteCallback(const char *pFilename, void *pOpaque);


void process_trace_command(void * /*callback_param*/, unsigned int buffer_size, char *buffer)
{
    int err = 0;
    int32_t command = -1;

    command = *(int32_t *)buffer;

    unsigned int buffer_size_temp = buffer_size - sizeof(int32_t);
    char *buffer_temp = buffer + sizeof(int32_t);

    switch (command)
    {
        case TRACE_SETNULLMODE:
        {
            g_null_mode = SetNullMode(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_SETDUMPGLCALLS:
        {
            g_dump_gl_calls_flag = SetDumpGLCalls(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_SETDUMPGLBUFFERS:
        {
            g_dump_gl_buffers_flag = SetDumpGLBuffers(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_SETDUMPGLSHADERS:
        {
            g_dump_gl_shaders_flag = SetDumpGLShaders(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_SETBACKTRACE:
        {
            g_backtrace_all_calls = SetBackTrace(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_KILLTRACER:
        {
            g_fKillApp = KillTracer(buffer_size_temp, buffer_temp);

            break;
        }

        case TRACE_STARTCAPTURE:
        {

            err = StartCapture(buffer_size_temp, buffer_temp);

            if (0 != err)
            {
                syslog(VOGL_ERROR, "%s:%d %s  StartCapture() failed\n", __FILE__, __LINE__, __func__);
            }

            break;
        }

        case TRACE_STOPCAPTURE:
        {

            err = StopCapture(buffer_size_temp, buffer_temp);

            if (0 != err)
            {
                syslog(VOGL_ERROR, "%s:%d %s  StopCapture() failed\n", __FILE__, __LINE__, __func__);
            }

            break;
        }

        case PING_GAME:
        {
            int notif_id = -1;
            notif_id = PingGame(buffer_size_temp, buffer_temp);
            //
            //  Tell the client we're up and running.
            send_status_to_client("Game is up and running.", notif_id);

            break;
        }

        default:
        {
            syslog(VOGL_ERROR, "%s:%d %s  Unknown method (%d)\n", __FILE__, __LINE__, __func__, command);
            break;
        }
    }

    return;
}

//
//  SetNullModePB
//
bool SetNullMode(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;

    if (0 == buffer_size)
        return false;

    cur_doc.deserialize(buffer);
    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    return pjson_node->value_as_bool("nullmode", false);
}

//
//  SetDumpGLCallsPB
//
bool SetDumpGLCalls(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;

    if (0 == buffer_size)
        return false;

    cur_doc.deserialize(buffer);
    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    return pjson_node->value_as_bool("dumpglcalls", false);
}

//
//  SetDumpGLBuffersPB
//
//
bool SetDumpGLBuffers(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;

    if (0 == buffer_size)
        return false;

    cur_doc.deserialize(buffer);
    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    return pjson_node->value_as_bool("dumpglbuffers", false);
}

//
//  SetDumpGLShaders
//
//
bool SetDumpGLShaders(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;

    if (0 == buffer_size)
        return false;

    cur_doc.deserialize(buffer);
    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    return pjson_node->value_as_bool("dumpglshaders", false);
}

//
//  SetBackTrace
//
//  Parameters:
//    If turned on, captures callstacks for any GL call and put it in the tracefile.
//
bool SetBackTrace(unsigned int buffer_size, char *buffer)
{
    vogl::json_document cur_doc;

    if (0 == buffer_size)
        return false;

    cur_doc.deserialize(buffer);
    vogl::json_node *pjson_node = cur_doc.get_root()->find_child_object("parameters");

    return pjson_node->value_as_bool("backtrace", false);
}

//
//  KillTracer
//
//  Kills the application that is currently being traced.
//
bool KillTracer(unsigned int /*buffer_size*/, char * /*buffer*/)
{
    // No body to this message.  Just an instruction.
    return true;
}


char szTraceFileLocal[] = "/.local/share";

int StartCapture(unsigned int buffer_size, char *buffer)
{
    const char *szBaseFileName;
    int cFrames = 0;
    bool fWorked = true;
    vogl::json_document cur_doc;
    vogl::json_node *pjson_node;
    int status = 0;

    std::string strTraceLocation;
    char *szTraceLocationT = NULL;

    syslog(VOGL_INFO, "Checking to see if we're already capturing\n");

    if (vogl_is_capturing())
    {
        goto out;
    }

    if (0 == buffer_size)
    {
        syslog(VOGL_ERROR, "%s:%d %s  No parameters passed\n", __FILE__, __LINE__, __func__);
        goto out;
    }

    cur_doc.deserialize(buffer);
    pjson_node = cur_doc.get_root()->find_child_object("parameters");
    
    syslog(VOGL_INFO, "Not already capturing...continuing\n");

    cFrames = pjson_node->value_as_int("framestocapture", -1);
    szBaseFileName = pjson_node->value_as_string_ptr("tracename", "");

    syslog(VOGL_INFO, "Capturing to %s for %d frames\n", szBaseFileName, cFrames);


    //
    //  Handle the destination directory for the file as per spec
    //    $XDG_DATA_HOME defines the base directory relative to which user specific data files should be stored. 
    //    If $XDG_DATA_HOME is either not set or empty, a default equal to $HOME/.local/share should be used.
    szTraceLocationT = getenv("XDG_DATA_HOME");
    syslog(VOGL_INFO, "XDG_DATA_HOME is '%s'\n", (NULL == szTraceLocationT? "NULL": szTraceLocationT));
    if (NULL == szTraceLocationT)
    {
        syslog(VOGL_INFO, "Building strTraceLocation from '%s' + '%s'\n", getenv("HOME"), szTraceFileLocal);
        strTraceLocation = getenv("HOME");
        strTraceLocation += szTraceFileLocal;
    }
    else
    {
        strTraceLocation = szTraceLocationT;
    }
    strTraceLocation += "/vogl/";

    syslog(VOGL_INFO, "Will be tracing to '%s'\n", strTraceLocation.c_str());

    //  Ensure the destination directory is actually there
    status = mkdir(strTraceLocation.c_str(), 0777 );
    if (-1 == status && EEXIST != errno)
    {
        syslog(VOGL_INFO, "StartCapturePB: Unable to create trace destination directory (%d) %s\n", errno, strTraceLocation.c_str());

        return -1;
    }

    syslog(VOGL_INFO, "StartCapturePB: Capturing trace to %s%s for %d frames\n", strTraceLocation.c_str(), szBaseFileName, cFrames);

    fWorked = vogl_capture_on_next_swap(cFrames, strTraceLocation.c_str(), szBaseFileName, CaptureCompleteCallback, NULL);

    syslog(VOGL_INFO, "StartCapturePB: Tracing now started (%s).  Trace Basefilename = %s\n", (fWorked ? "success" : "failed"), szBaseFileName);

out:

    return 0;
}


int StopCapture(unsigned int /*buffer_size*/, char * /*buffer*/)
{
    bool fStillCapturing = false;
    bool fStopping = false;

    //
    //  No body to this message.  Not necessary.

    fStillCapturing = vogl_is_capturing();
    syslog(VOGL_INFO, "StopCapturePB(): Trace status = %d\n", fStillCapturing);

    if (true == fStillCapturing)
    {
        //  Need to terminate the trace
        fStopping = vogl_stop_capturing(CaptureCompleteCallback, NULL);
        syslog(VOGL_INFO, "StopCapturePB(): vogl_stop_capturing status = %d\n", fStopping);
    }

    return 0;
}


//
//  Sends off a message to the client that tracing has completed.
//
int CaptureCompleteCallbackMsg(const char *pFilename, int cFramesCaptured, unsigned int *message_size, char **pmessage);
void CaptureCompleteCallback(const char *pFilename, void * /*pOpaque*/)
{
    int ec = 0;
    network::CHEC chec = network::EC_NONE;
    char *message = NULL;
    unsigned int message_size = 0;
    int cFramesCaptured = -1;

    syslog(VOGL_INFO, "CaptureCompleteCallback(): Tracing complete...\n");

    //  Send off a status message to the client with:
    //  process_name
    //  file_name
    //  frames_capture (TBD)
    //  Anything else?
    ec = CaptureCompleteCallbackMsg(pFilename, cFramesCaptured, &message_size, &message);
    if (0 != ec)
    {
        syslog(VOGL_ERROR, "%s:%d %s  Unable to send tracefile name (%s) back to client - OOM?\n", __FILE__, __LINE__, __func__, pFilename);
        return;       
    }

    chec = g_clientChannelMgr->SendData(message_size, message);
    if (network::EC_NONE != chec)
    {
        syslog(VOGL_ERROR, "%s:%d %s  Unable to send tracefile name (%s) back to client - Network error.\n", __FILE__, __LINE__, __func__, pFilename);
    }

    if (message)
        free(message);

    return;
}


int CaptureCompleteCallbackMsg(const char *pFilename, int cFramesCaptured, unsigned int *message_size, char **pmessage)
{
    vogl::json_document cur_doc;
    vogl::dynamic_string dst;

    char *pbBuff;
    unsigned int cbBuff;

    vogl::json_node &meta_node = cur_doc.get_root()->add_object("parameters");

    meta_node.add_key_value("trace_file_name", pFilename);
    meta_node.add_key_value("process_name", g_procname);
    meta_node.add_key_value("frames_captured", cFramesCaptured);

    cur_doc.serialize(dst);

    cbBuff = dst.get_len() + 1 + sizeof(int);
    pbBuff = (char *)malloc(cbBuff);
    if (NULL == pbBuff)
    {
        printf("OOM\n");
        return -1;
    }

    //  First part of buffer is the command id
    *((int *)pbBuff) = TRACE_CAPTURESTATUS;
    strncpy((char *)(pbBuff+sizeof(int)), dst.get_ptr(), cbBuff - sizeof(int));

    *pmessage = pbBuff;
    *message_size = cbBuff;

    return 0;
}


bool log_output_func(vogl::eConsoleMessageType /*type*/, const char *pMsg, void * /*pData*/)
{

    send_status_to_client(pMsg);

    return true;
}


void send_status_to_client(const char *message)
{
    int ec = 0;
    unsigned int cbBuff = 0;
    char *pbBuff = NULL;
    //
    //  Prepare message to send

    ec = ToClientMsgReq(g_procname, 0, message, &cbBuff, &pbBuff);
    if (0 != ec)
    {
        syslog(VOGL_INFO, "ToClientMsgReq error: %d\n", ec);
    }

    (void)g_clientChannelMgr->SendData(cbBuff, pbBuff);

    if (pbBuff)
        free(pbBuff);

    return;
}

void send_status_to_client(const char *message, int /*token*/)
{
    //  Ignoring token for a moment.
    send_status_to_client(message);

    return;
}

#endif // VOGL_REMOTING
