#ifndef VOGLEDITOR_TRACEREPLAYER_H
#define VOGLEDITOR_TRACEREPLAYER_H

#include "vogl_common.h"
#include "vogl_replay_window.h"

class vogl_gl_replayer;
class vogleditor_gl_state_snapshot;
class vogl_gl_state_snapshot;
class vogleditor_apiCallTreeItem;
class vogl_trace_file_reader;

enum vogleditor_tracereplayer_result
{
    VOGLEDITOR_TRR_SUCCESS = 0,
    VOGLEDITOR_TRR_SNAPSHOT_SUCCESS,
    VOGLEDITOR_TRR_USER_EXIT,
    VOGLEDITOR_TRR_ERROR
};

class vogleditor_traceReplayer
{
public:
    vogleditor_traceReplayer();
    virtual ~vogleditor_traceReplayer();

    vogleditor_tracereplayer_result replay(vogl_trace_file_reader *m_pTraceReader, vogleditor_apiCallTreeItem *pRootItem, vogleditor_gl_state_snapshot **ppNewSnapshot, uint64_t apiCallNumber, bool endlessMode);
    bool pause();
    bool restart();
    bool trim();
    bool stop();

    void enable_screenshot_capturing(std::string screenshot_prefix);
    void enable_fs_preprocessor(std::string fs_preprocessor);

private:
    bool applying_snapshot_and_process_resize(const vogl_gl_state_snapshot *pSnapshot);

    vogleditor_tracereplayer_result take_state_snapshot_if_needed(vogleditor_gl_state_snapshot **ppNewSnapshot, uint32_t apiCallNumber);
    vogleditor_tracereplayer_result recursive_replay_apicallTreeItem(vogleditor_apiCallTreeItem *pItem, vogleditor_gl_state_snapshot **ppNewSnapshot, uint64_t apiCallNumber);

    bool process_events();
    vogl_gl_replayer *m_pTraceReplayer;
    vogl_replay_window m_window;

    std::string m_screenshot_prefix;
    std::string m_fs_preprocessor;
    std::string m_fs_preprocessor_options;
    std::string m_fs_preprocessor_prefix;
};

#endif // VOGLEDITOR_TRACEREPLAYER_H
