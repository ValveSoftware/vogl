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
#include <time.h>

#include <pthread.h>

#include "../common/radlogging.h"
#include "../common/channel.h"

#include "../public/rpcec.h"
#include "../public/rpcconst.h"

#include "vogl_console.h"

#include "vogl_remote.h"

#include "debugserver.pb.h"
#include "gpureplaycontrol.pb.h"

// Needs to be the same in the server gpusession code
#define REQRESP_PORT 5600
#define LOGSTREAM_PORT (REQRESP_PORT + 1)

void *InvokeGLIReplayListener(void *arg);

static bool g_ping_value; //  Just for testing aliveness...

void vogl_init_listener()
{
    pthread_t threadCmd;
    int err = 0;
    syslog(RAD_INFO, "Spinning up listener for GLI Replayer...\n");

    //  Launching a GLI Listener
    err = pthread_create(&threadCmd, NULL, InvokeGLIReplayListener, NULL);
    if (0 != err)
    {
        syslog(RAD_ERROR, "Unable to create GLI Replayer cmd thread. returned %d, errno %d\n", err, errno);
        return;
    }

    return;
}

bool ProcessCommandGLIReplay(std::string strReq, std::string *pstrResp);
RPCEC rpcecFromChec(network::CHEC chec);

#define MAX_RETRIES 10
#define NET_TIMEOUT 500 // MS

void *
InvokeGLIReplayListener(void * /*arg*/) // arg is unused
{
    //
    //  Setup to wait for a client to connect
    //
    pthread_t thisThread = pthread_self();
    RPCEC rpcec = rpcecNone;
    bool fKeepRunning = true;
    network::CHEC chec = network::EC_NONE;

    int port = REQRESP_PORT;

    g_ping_value = false; //  Just for testing aliveness...

    while (fKeepRunning)
    {
        network::channel *pChannel = NULL;

        pChannel = new network::channel();
        if (NULL == pChannel)
        {
            syslog(RAD_ERROR, "%lx: InvokeGLIReplayListener: Error allocating channel object: OOM\n", thisThread);
            goto out;
        }

        chec = pChannel->Connect(port, 3, true);
        if (network::EC_NONE != chec)
        {
            syslog(RAD_ERROR, "%lx: InvokeGLIReplayListener: Error on ChannelConnect(port = %d, errno = %x): %x\nTerminating...\n", thisThread, port, errno, chec);
            goto out;
        }

        std::string strReq;
        std::string strResp;

        chec = pChannel->ReadMsg(&strReq, 1, 0);
        rpcec = rpcecFromChec(chec);
        if (rpcecNone != rpcec)
        {
            goto out;
        }

        fKeepRunning = ProcessCommandGLIReplay(strReq, &strResp);

        chec = pChannel->WriteMsg(strResp, 1, 0);
        rpcec = rpcecFromChec(chec);
        if (rpcecNone != rpcec)
        {
            goto out;
        }

        pChannel->Disconnect();
        delete pChannel;
    }

out:
    syslog(RAD_INFO, "%lx: GLI Replayer server thread: Terminating.\n", thisThread);

    return NULL;
}

RPCEC PingPB(int sessionid, std::string strReq, std::string *pstrResp, int *perrno, std::string *pstrError);
//RPCEC RegisterLogCallbackPB(int sessionid, std::string strReq, std::string *pstrResp, int *perrno, std::string *pstrError);
//RPCEC DeregisterLogCallbackPB(int sessionid, std::string strReq, std::string *pstrResp, int *perrno, std::string *pstrError);

bool
ProcessCommandGLIReplay(std::string strReq, std::string *pstrResp)
{
    bool fKeepRunning = true;
    RPCEC rpcec = rpcecNone;
    std::string strReqInner;
    std::string strRespInner;
    int err = 0;
    std::string strError;

    debugserverpb::BaseMethodReqPB gpuBaseReq;
    debugserverpb::BaseMethodRespPB gpuBaseResp;

    int interfaceid;
    int method;

    gpuBaseReq.ParseFromString(strReq);

    interfaceid = gpuBaseReq.interfaceid();
    method = gpuBaseReq.method();
    strReqInner = gpuBaseReq.embeddedrequest();

    //  Should never happen...
    if (GLIREPLAYCONTROL != interfaceid)
    {
        // Currently only a single interface is known for GLI.  If it's not this one, not sure what's supposed to happen.
        syslog(RAD_ERROR, "ProcessCommandGLIReplay: GPUReplayerControl: Unknown interfaceid (%d) passed to vogltrace from client\n", interfaceid);
        strError = "ProcessCommandGLIReplay: GPUReplayerControl: Unknown interfaceid passed to voglreplayer from client";
        rpcec = rpcecFail;
        goto out;
    }

    switch (method)
    {

        case gpureplaycontrolpb::PING:
        {
            rpcec = PingPB(gpuBaseReq.dbgsessionid(), strReqInner, &strRespInner, &err, &strError);

            if (rpcecNone != rpcec)
            {
                syslog(RAD_ERROR, "ProcessCommandGLIReplay: GPUReplayerControl: Ping() failed\n");
            }

            break;
        }
#ifdef NOTYET
        case gpucontrolpb::REGISTERLOGCALLBACK:
        {
            rpcec = RegisterLogCallbackPB(gpuBaseReq.dbgsessionid(), strReqInner, &strRespInner, &err, &strError);

            if (rpcecNone != rpcec)
            {
                syslog(RAD_ERROR, "ProcessCommandGLIReplay: GPUReplayerControl: RegisterLogCallback() failed\n");
            }

            break;
        }

        case gpucontrolpb::DEREGISTERLOGCALLBACK:
        {
            rpcec = DeregisterLogCallbackPB(gpuBaseReq.dbgsessionid(), strReqInner, &strRespInner, &err, &strError);

            if (rpcecNone != rpcec)
            {
                syslog(RAD_ERROR, "ProcessCommandGLIReplay: GPUReplayerControl: DeregisterLogCallback() failed\n");
            }

            break;
        }

#endif // NOTYET

        default:
        {
            syslog(RAD_ERROR, "ProcessCommandGLIReplay: Unknown method (%d) for GPUReplayerControl interface\n", method);
            strError = "ProcessCommandGLIReplay: GPUReplayerControl: Unknown method passed to vogltrace interface from client";
            rpcec = rpcecFail;
            break;
        }
    }

out:
    //  Set the method back into the response
    //
    gpuBaseResp.set_interfaceid(interfaceid);
    gpuBaseResp.set_method(method);
    gpuBaseResp.set_rpcec(rpcec);
    if (rpcecNone != rpcec)
    {
        syslog(RAD_ERROR, "ProcessCommandGLIReplay: GPUReplayerControl: returning errors to client (rpcec = %d)\n", rpcec);
        gpuBaseResp.set_errorstring(strError);
        gpuBaseResp.set_lowlevelerror(err);
    }

    gpuBaseResp.set_embeddedresponse(strRespInner);

    gpuBaseResp.SerializeToString(pstrResp);

    return fKeepRunning;
}

//
//  PingPB
//
//  Parameters:
//    sessionid - unused here, but the intent is that if there's any state associated with this particular request that needs to be maintained serverside
//				  that this could be used to indicate which of potentially many sets of these states should be used.  Feel free to ignore if not needed.
//	  strReq    - a serialized protobuf string (std::string) that contains the parameters to the request.  The idea is that you would ParseFromString this
//				  parameter into your request structure (defined in your .proto file) and then pull parameters as needed.
//    strResp   - a serialized protobuf string (std::string) that contains the response data from this request.  The idea is that you would set all the response
//			      values into your response protobuf (defined in your .proto file) and then serialize that structure to a std::string and return it from here.
//    perrno    - this is any service specific error number that might need to be returned as part of this specific method/interface.
//    pstrError - along with perrno, there may be a human-readable string that can be returned.  This is where you might return it.
//
//  Returns:
//	  RPCEC     - if all goes well with parsing and generating the protobufs or any network related stuff, then this should be rpcecNone.  Otherwise if there's a
//				  a problem with these areas, then an appropriate RPCEC error should be returned here (see rpcec.h).
//
RPCEC PingPB(int /*sessionid*/, std::string strReq, std::string *pstrResp, int * /*perrno*/, std::string * /*pstrError*/)
{
    RPCEC rpcec = rpcecNone;
    gpureplaycontrolpb::PingReqPB gpuPingReq;
    gpureplaycontrolpb::PingRespPB gpuPingResp;

    bool ping = false;

    gpuPingReq.ParseFromString(strReq);

    ping = gpuPingReq.ping();

    if (ping == g_ping_value)
        gpuPingResp.set_message((ping ? "Same ping value as last time: TRUE" : "Same ping value as last time: FALSE"));
    else
        gpuPingResp.set_message((ping ? "Different ping value as last time: FALSE->TRUE" : "Different ping value as last time: TRUE->FALSE"));

    g_ping_value = ping;

    gpuPingResp.SerializeToString(pstrResp);

    return rpcec;
}

#ifdef NOTYET
//
//  Log Callback Globals
static bool g_fLogCallbackEnabled = false;
static int g_cRegisteredCallbacks = 0;
static int g_logCallbackPort = LOGSTREAM_PORT;
static network::channel *g_pChannelLogCallBack = NULL;

void *InvokeLogListener(void *arg);
bool log_output_func(vogl::eConsoleMessageType type, const char *pMsg, void *pData);

typedef struct _regtype
{
    int sessionid;
    int registration;
    int maxpacketsize;
    LOGTYPE ltFilter;
} REGTYPE;

static REGTYPE g_registrations[MAX_REGISTRATIONS] = { { SESSIONID_NONE, 0, 0, LT_INVALID } };

//
//  RegisterLogCallbackPB
//
//  Parameters:
//    sessionid - unused here, but the intent is that if there's any state associated with this particular request that needs to be maintained serverside
//				  that this could be used to indicate which of potentially many sets of these states should be used.  Feel free to ignore if not needed.
//	  strReq    - a serialized protobuf string (std::string) that contains the parameters to the request.  The idea is that you would ParseFromString this
//				  parameter into your request structure (defined in your .proto file) and then pull parameters as needed.
//    strResp   - a serialized protobuf string (std::string) that contains the response data from this request.  The idea is that you would set all the response
//			      values into your response protobuf (defined in your .proto file) and then serialize that structure to a std::string and return it from here.
//    perrno    - this is any service specific error number that might need to be returned as part of this specific method/interface.
//    pstrError - along with perrno, there may be a human-readable string that can be returned.  This is where you might return it.
//
//  Returns:
//	  RPCEC     - if all goes well with parsing and generating the protobufs or any network related stuff, then this should be rpcecNone.  Otherwise if there's a
//				  a problem with these areas, then an appropriate RPCEC error should be returned here (see rpcec.h).
//
RPCEC RegisterLogCallbackPB(int /*sessionid*/, std::string strReq, std::string *pstrResp, int *perrno, std::string *pstrError)
{

    //  Registering and Deregistering these logging callbacks always happen on the same thread.  So any global that only these two functions use will be safe
    //  to use.

    RPCEC rpcec = rpcecNone;
    gpucontrolpb::RegisterLogCallbackReqPB gpuRegisterLogCallbackReq;
    gpucontrolpb::RegisterLogCallbackRespPB gpuRegisterLogCallbackResp;

    LOGTYPE ltReq = LT_INVALID;
    int registrationid = REGISTRATION_INVALID; // Invalid
    int registrationindex = MAX_REGISTRATIONS + 1;

    //
    //
    gpuRegisterLogCallbackReq.ParseFromString(strReq);

    //
    //  Register log callback
    //	typedef bool (*console_output_func)(eConsoleMessageType type, const char* pMsg, void* pData);
    //	vogl::console::add_console_output_func(LogCallbackFunc, NULL);

    ltReq = (LOGTYPE)gpuRegisterLogCallbackReq.logfilter();
    if (LT_INVALID == ltReq)
    {
        syslog(RAD_ERROR, "GLI RegisterLogCallbackPB - invalid log filter (LT_INVALID) requested\n");
        rpcec = rpcecFail;
        *pstrError = "GLI RegisterLogCallbackPB - invalid log filter (LT_INVALID) requested";
        *perrno = 0;

        goto out;
    }

    //
    //  Find the next available slot to register our filter for this registration
    for (int i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (LT_INVALID == g_registrations[i].ltFilter)
        {
            registrationid = rand() + 1; //  Not really random enough...
            registrationindex = i;
            break;
        }
    }

    if (MAX_REGISTRATIONS + 1 == registrationindex)
    {
        //  Too many registrations!
        rpcec = rpcecFail;
        *pstrError = "GLI RegisterLogCallbackPB - too many registrations already.  Deregister some and try again.";
        *perrno = 0;

        goto out;
    }

    //  Check to see if we need a connection
    //
    if (!g_fLogCallbackEnabled)
    {
        //  Spawn a thread to be connected to
        pthread_t threadLogging;
        int err = 0;
        syslog(RAD_INFO, "Spinning up thread for GLI Trace logging...\n");

        //  Launching a GLI Listener
        err = pthread_create(&threadLogging, NULL, InvokeLogListener, NULL); // &port??
        if (0 != err)
        {
            syslog(RAD_ERROR, "Unable to create GLI Logging thread. returned %d, errno %d\n", err, errno);
            rpcec = rpcecFail;
            *pstrError = "GLI RegisterLogCallbackPB - Unable to create GLI Logging thread.  See system log for more info.";
            *perrno = errno;
            goto out;
        }
        //
        //  Register log callback - we only have one of these and we get rid of it when
        //  we have no more registrations.
        //
        //	typedef bool (*console_output_func)(eConsoleMessageType type, const char* pMsg, void* pData);
        //	vogl::console::add_console_output_func(LogCallbackFunc, NULL);
        vogl::console::add_console_output_func(log_output_func, NULL);
    }

    g_registrations[registrationindex].ltFilter = ltReq;
    g_registrations[registrationindex].registration = registrationid;
    g_cRegisteredCallbacks++;

    gpuRegisterLogCallbackResp.set_registrationid(registrationid);

out:
    gpuRegisterLogCallbackResp.SerializeToString(pstrResp);

    return rpcec;
}

//
//  DeregisterLogCallbackPB
//
//  Parameters:
//    sessionid - unused here, but the intent is that if there's any state associated with this particular request that needs to be maintained serverside
//				  that this could be used to indicate which of potentially many sets of these states should be used.  Feel free to ignore if not needed.
//	  strReq    - a serialized protobuf string (std::string) that contains the parameters to the request.  The idea is that you would ParseFromString this
//				  parameter into your request structure (defined in your .proto file) and then pull parameters as needed.
//    strResp   - a serialized protobuf string (std::string) that contains the response data from this request.  The idea is that you would set all the response
//			      values into your response protobuf (defined in your .proto file) and then serialize that structure to a std::string and return it from here.
//    perrno    - this is any service specific error number that might need to be returned as part of this specific method/interface.
//    pstrError - along with perrno, there may be a human-readable string that can be returned.  This is where you might return it.
//
//  Returns:
//	  RPCEC     - if all goes well with parsing and generating the protobufs or any network related stuff, then this should be rpcecNone.  Otherwise if there's a
//				  a problem with these areas, then an appropriate RPCEC error should be returned here (see rpcec.h).
//
RPCEC DeregisterLogCallbackPB(int /*sessionid*/, std::string strReq, std::string *pstrResp, int *perrno, std::string *pstrError)
{
    RPCEC rpcec = rpcecNone;
    int registrationid = REGISTRATION_INVALID; // Invalid
    int registrationindex = MAX_REGISTRATIONS + 1;

    gpucontrolpb::DeregisterLogCallbackReqPB gpuDeregisterLogCallbackReq;
    gpucontrolpb::DeregisterLogCallbackRespPB gpuDeregisterLogCallbackResp;

    gpuDeregisterLogCallbackReq.ParseFromString(strReq);

    registrationid = gpuDeregisterLogCallbackReq.registrationid();

    //
    //  Find the registrationid in the list
    //
    for (int i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if (registrationid == g_registrations[i].registration)
        {
            registrationindex = i;
            break;
        }
    }

    if (MAX_REGISTRATIONS <= registrationindex)
    {
        //  Out of bounds
        rpcec = rpcecFail;
        *pstrError = "GLI DeregisterLogCallbackPB - Unknown registration id(1).";
        *perrno = 0;

        goto out;
    }

    if (LT_INVALID == g_registrations[registrationindex].ltFilter)
    {
        //  Invalid registrationid
        rpcec = rpcecFail;
        *pstrError = "GLI DeregisterLogCallbackPB - Unknown registration id(2).";
        *perrno = 0;
        goto out;
    }

    g_registrations[registrationindex].ltFilter = LT_INVALID;
    g_registrations[registrationindex].registration = REGISTRATION_INVALID;

    g_cRegisteredCallbacks--;
    if (0 == g_cRegisteredCallbacks)
    {
        vogl::console::remove_console_output_func(log_output_func);
        g_fLogCallbackEnabled = false;
        // Race condition?  We could be sending a log on a different thread and then try to close out socket and descroy the context here at
        // the same time.
        //
        g_pChannelLogCallBack->Disconnect();
        delete g_pChannelLogCallBack;
        g_pChannelLogCallBack = NULL;
    }

out:
    gpuDeregisterLogCallbackResp.SerializeToString(pstrResp);

    return rpcec;
}

void *
InvokeLogListener(void * /*arg*/)
{
    //
    //  Setup to wait for a client to connect
    //
    pthread_t thisThread = pthread_self();
    network::CHEC chec = network::EC_NONE;

    g_pChannelLogCallBack = new network::channel();

    chec = g_pChannelLogCallBack->Connect(g_logCallbackPort, 3, true);
    if (network::EC_NONE != chec)
    {
        g_pChannelLogCallBack->Disconnect();
        delete g_pChannelLogCallBack;
        g_pChannelLogCallBack = NULL;

        syslog(RAD_ERROR, "%lx: InvokeLogListener: Error on pChannel::Connect(port = %d, errno = %x): %x\nTerminating...\n", thisThread, g_logCallbackPort, errno, chec);
        goto out;
    }

    g_fLogCallbackEnabled = true;

out:
    return NULL;
}

LOGTYPE crnMsgTypeToLogType(vogl::eConsoleMessageType type);
RPCEC EcSendNotif(network::channel *pChannel, int notifid, int sessionid, std::string strLogMsg);

bool log_output_func(vogl::eConsoleMessageType type, const char *pMsg, void * /*pData*/)
{
    //  NOTE:  This could be called from any thread...  We're not 100% safe.
    pthread_t thisThread = pthread_self();
    LOGTYPE ltMessage = LT_INVALID;
    RPCEC rpcec = rpcecNone;

    gpucontrolpb::LogCallbackPB logMessagePB;
    std::string strLogMsg;

    bool fFoundAtLeastOneMatch = false;
    int registrationid = REGISTRATION_INVALID;

    if (!g_fLogCallbackEnabled)
    {
        //  No one is ready to receive this data.  Dump the pMsg.

        goto out;
    }

    //  Filter out those messages that we're not looking for
    ltMessage = crnMsgTypeToLogType(type);

    // Loop through all our registrations and for those that are interested in
    // this message, add them to the list of registrations this matches
    //
    for (int i = 0; i < MAX_REGISTRATIONS; i++)
    {
        if ((ltMessage & g_registrations[i].ltFilter))
        {
            if (REGISTRATION_INVALID != g_registrations[i].registration)
            {
                fFoundAtLeastOneMatch = true;

                registrationid = g_registrations[i].registration;

                logMessagePB.add_sessionids(g_registrations[i].sessionid);
                logMessagePB.add_registrationids(registrationid);
            }
        }
    }

    if (!fFoundAtLeastOneMatch)
    {
        //  Nothing matches the filter...drop the pMsg.

        goto out;
    }

    logMessagePB.set_logtype(ltMessage);

    logMessagePB.set_message(pMsg);

    logMessagePB.SerializeToString(&strLogMsg);

    //
    //  Actually send it
    //
    //  We have a list of sessionids in the actual message...no need to choose one here.
    rpcec = EcSendNotif(g_pChannelLogCallBack, gpucontrolpb::LOGCALLBACK, SESSIONID_NONE, strLogMsg);

    if (rpcecNone != rpcec)
    {
        syslog(RAD_ERROR, "%lx: log_output_func(id=%d) EcSendResponse returns %x.  Message \"%s\" unsent.\n", thisThread, registrationid, rpcec, pMsg);
        goto out;
    }

out:

    return true;
}

RPCEC EcSendNotif(network::channel *pChannel, int notifid, int sessionid, std::string strLogMsg)
{
    RPCEC rpcec = rpcecNone;
    //  Wraps the strLogMsg in the appropriate
    debugserverpb::BaseMethodNotifPB gpuNotifPB;
    std::string strNotifMsg;

    gpuNotifPB.set_notifid(notifid);
    gpuNotifPB.set_dbgsessionid(sessionid);

    gpuNotifPB.set_embeddednotif(strLogMsg);

    gpuNotifPB.SerializeToString(&strNotifMsg);

    rpcec = EcSendResponse(pChannel, strNotifMsg);

    return rpcec;
}

LOGTYPE crnMsgTypeToLogType(vogl::eConsoleMessageType type)
{

    switch (type)
    {
        case vogl::cMsgDebug:
        {
            return LT_DEBUG;
        }
        case vogl::cMsgProgress:
        {
            return LT_PROGRESS;
        }
        case vogl::cMsgPrint:
        {
            return LT_INFO;
        }
        case vogl::cConsoleConsoleMessage:
        {
            return LT_DEFAULT;
        }
        case vogl::cMsgMessage:
        {
            return LT_MESSAGE;
        }
        case vogl::cMsgWarning:
        {
            return LT_WARNING;
        }
        case vogl::cMsgError:
        {
            return LT_ERROR;
        }
        default:
        {
            break;
        }
    }

    return LT_LOG; // Not sure what else to do here...?
}
#endif // NOTYET

RPCEC
rpcecFromChec(network::CHEC chec)
{
    switch (chec)
    {
        case network::EC_NONE:
        {
            return rpcecNone;
        }
        case network::EC_TIMEOUT:
        {
            return rpcecTimeout;
        }
        case network::EC_NETWORK:
        {
            return rpcecFail;
        }
        case network::EC_MEMORY:
        {
            return rpcecFail;
        }
        default:
        {
            return rpcecFail;
        }
    }

    return rpcecFail;
}
#endif // VOGL_REMOTING
