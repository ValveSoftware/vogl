

//
//  Default port that client talks to server on.
//
#define DEFAULT_PORT 29900
//
//  Default port that server talks to game/app on.
//
#define TRACE_PORT 29905

//
//  Message enums.  All bodies to messages are assumed to be JSON unless documented here that they aren't.
//
enum 
{
    LAUNCHSTEAMGAME = 0,        // C->S
    TOCLIENTMSG,                // G,S->C
    TRACE_SETNULLMODE,          // C->S
    TRACE_SETDUMPGLCALLS,       // C->S
    TRACE_SETDUMPGLBUFFERS,     // C->S
    TRACE_SETDUMPGLSHADERS,     // C->S
    TRACE_SETBACKTRACE,         // C->S
    TRACE_KILLTRACER,           // C->S
    TRACE_STARTCAPTURE,         // C->S
    TRACE_STOPCAPTURE,          // C->S
    TRACE_CAPTURESTATUS,        // G->C
    PING_GAME,                  // C->G
    TRACE_RETRIEVE_CAPTURE,     // C->S
    TRACE_RETRIEVE_PART,        // S->C
    TRACE_LIST_TRACES,          // C->S
    TRACE_LIST,                 // S->C
    MAX_COMMAND
};

