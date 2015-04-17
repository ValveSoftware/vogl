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

#ifndef VOGLEDITOR_H
#define VOGLEDITOR_H

#include <QMainWindow>
#include <QString>

#include "vogleditor_qtextureexplorer.h"
#include "vogleditor_tracereplayer.h"

namespace Ui
{
    class VoglEditor;
}

typedef uint64_t GLuint64;

class QGridLayout;

class QModelIndex;
class QProcess;
class QProcessEnvironment;
class QToolButton;
class QScrollBar;
class vogl_arb_program_state;
class vogl_program_state;
class vogleditor_apiCallTimelineModel;
class vogleditor_QApiCallTreeModel;
class vogleditor_QBufferExplorer;
class vogleditor_QStateTreeModel;
class vogleditor_QProgramArbExplorer;
class vogleditor_QProgramExplorer;
class vogleditor_QFramebufferExplorer;
class vogleditor_QShaderExplorer;
class vogleditor_QSnapshotOverlayWidget;
class vogleditor_QTimelineView;
class vogleditor_QVertexArrayExplorer;
class vogleditor_QLaunchTracerDialog;
class vogleditor_snapshotItem;

class VoglEditor : public QMainWindow
{
    Q_OBJECT

    enum Prompt_Result
    {
        vogleditor_prompt_error = -1,
        vogleditor_prompt_cancelled = 0,
        vogleditor_prompt_success = 1
    };

public:
    explicit VoglEditor(QWidget *parent = 0);
    ~VoglEditor();

    // Opens the trace file and reads the UUID so that it can access associated session data from the user's app data folder
    bool pre_open_trace_file(dynamic_string filename);
    void close_trace_file();

    QString get_sessionfile_name(const QString &tracefile, const vogl_trace_file_reader &traceReader);
    QString get_sessionfile_path(const QString &tracefile, const vogl_trace_file_reader &traceReader);
    QString get_sessiondata_folder(const QString &tracefile, const vogl_trace_file_reader &traceReader);
    QString get_sessiondata_path(const QString &tracefile, const vogl_trace_file_reader &traceReader);

public
slots:
    void slot_takeSnapshotButton_clicked();

private
slots:
    void on_action_Open_triggered();
    void on_action_Close_triggered();
    void on_actionE_xit_triggered();
    void on_actionExport_API_Calls_triggered();
    void on_actionEdit_triggered();

    void slot_treeView_currentChanged(const QModelIndex &current, const QModelIndex &previous);

    void on_treeView_clicked(const QModelIndex &index);

    Prompt_Result prompt_generate_trace();
    void playCurrentTraceFile();
    void trimCurrentTraceFile();
    void dumpDrawFrameBuffers();
    void collect_screenshots();

    Prompt_Result prompt_trim_trace_file(QString filename, uint maxFrameIndex, uint maxAllowedTrimLen);
    Prompt_Result prompt_dump_draws(QString filename);

    void on_stateTreeView_clicked(const QModelIndex &index);

    void on_searchTextBox_textChanged(const QString &searchText);
    void on_searchNextButton_clicked();
    void on_searchPrevButton_clicked();
    void on_prevSnapshotButton_clicked();
    void on_nextSnapshotButton_clicked();
    void on_prevDrawcallButton_clicked();
    void on_nextDrawcallButton_clicked();

    void slot_program_edited(vogl_arb_program_state *pNewProgramState);
    void slot_program_edited(vogl_program_state *pNewProgramState);

    void on_searchTextBox_returnPressed();

    void slot_readReplayStandardOutput();
    void slot_readReplayStandardError();

    void on_contextComboBox_currentIndexChanged(int index);

    void on_treeView_activated(const QModelIndex &index);

    void selectAPICallItem(vogleditor_snapshotItem *pItem);

private:
    Ui::VoglEditor *ui;

    // Opens a trace file without looking for associated session data
    bool open_trace_file(vogl::dynamic_string filename);

    void onApiCallSelected(const QModelIndex &index, bool bAllowStateSnapshot);
    void setTimeline(vogleditor_apiCallTreeItem *pCallTreeItem);
    bool displayTexture(GLuint64 textureHandle, bool bBringTabToFront);
    void displayFramebuffer(GLuint64 framebufferHandle, bool bBringTabToFront);
    bool displayShader(GLuint64 shaderHandle, bool bBringTabToFront);
    void displayProgramArb(GLuint64 programArbHandle, bool bBringTabToFront);
    void displayProgram(GLuint64 programHandle, bool bBringTabToFront);
    bool displayBuffer(GLuint64 bufferHandle, bool bBringTabToFront);
    bool resetApiCallTreeModel();

    bool launch_application_to_generate_trace(const QString &cmdLine, const QProcessEnvironment &environment);

    Prompt_Result prompt_load_new_trace(const char *tracefile);

    // temp?
    vogl_gl_state_snapshot *read_state_snapshot_from_trace(vogl_trace_file_reader *pTrace_reader);

    void reset_tracefile_ui();
    void reset_snapshot_ui();

    void update_ui_for_snapshot(vogleditor_gl_state_snapshot *pStateSnapshot);
    void update_ui_for_context(vogl_context_snapshot *pContext, vogleditor_gl_state_snapshot *pStateSnapshot);
    void update_vertex_array_explorer_for_apicall(vogleditor_apiCallTreeItem *pApiCall);

    void displayMachineInfoHelper(QString prefix, const QString &sectionKeyStr, const vogl::json_value &value, QString &rMachineInfoStr);
    void displayMachineInfo();
    void recursive_update_snapshot_flags(vogleditor_apiCallTreeItem *pItem, bool &bFoundEditedSnapshot);

    vogleditor_gl_state_snapshot *findMostRecentSnapshot_helper(vogleditor_apiCallTreeItem *pItem, vogleditor_gl_state_snapshot *&pMostRecentSnapshot, const vogleditor_gl_state_snapshot *pCurSnapshot);
    vogleditor_gl_state_snapshot *findMostRecentSnapshot(vogleditor_apiCallTreeItem *pItem, const vogleditor_gl_state_snapshot *pCurSnapshot);

    void selectApicallModelIndex(QModelIndex index, bool scrollTo, bool select);

    void write_child_api_calls(vogleditor_apiCallTreeItem *pItem, FILE *pFile);

    // returns the name of the session file
    bool load_or_create_session(const char *tracefile, vogl_trace_file_reader *pTraceReader);
    bool load_session_from_disk(const QString &sessionFile);
    bool save_session_to_disk(const QString &sessionFile, const QString &traceFile, vogl_trace_file_reader *pTraceReader, vogleditor_QApiCallTreeModel *pApiCallTreeModel);
    bool save_snapshot_to_disk(vogl_gl_state_snapshot *pSnapshot, dynamic_string filename, vogl_blob_manager *pBlob_manager);

    QString m_openFilename;
    vogleditor_QFramebufferExplorer *m_pFramebufferExplorer;
    vogleditor_QTextureExplorer *m_pTextureExplorer;
    vogleditor_QTextureExplorer *m_pRenderbufferExplorer;
    vogleditor_QProgramArbExplorer *m_pProgramArbExplorer;
    vogleditor_QProgramExplorer *m_pProgramExplorer;
    vogleditor_QShaderExplorer *m_pShaderExplorer;
    vogleditor_QBufferExplorer *m_pBufferExplorer;
    vogleditor_QVertexArrayExplorer *m_pVertexArrayExplorer;
    vogleditor_QTimelineView *m_timeline;

    QGridLayout *m_pFramebufferTab_layout;
    QGridLayout *m_pTextureTab_layout;
    QGridLayout *m_pRenderbufferTab_layout;
    QGridLayout *m_pProgramArbTab_layout;
    QGridLayout *m_pProgramTab_layout;
    QGridLayout *m_pShaderTab_layout;
    QGridLayout *m_pBufferTab_layout;
    QGridLayout *m_pVertexArrayTab_layout;

    vogleditor_gl_state_snapshot *m_currentSnapshot;
    vogleditor_apiCallTreeItem *m_pCurrentCallTreeItem;

    QProcess *m_pVoglReplayProcess;
    QToolButton *m_pGenerateTraceButton;
    QToolButton *m_pPlayButton;
    QToolButton *m_pTrimButton;
    QToolButton *m_pDumpButton;

    QToolButton *m_pCollectScreenshotsButton;

    vogleditor_traceReplayer m_traceReplayer;
    vogl_trace_file_reader *m_pTraceReader;
    vogl::json_document m_backtraceDoc;
    vogl::hash_map<uint32_t, vogl::json_node *> m_backtraceToJsonMap;

    vogleditor_apiCallTimelineModel *m_pTimelineModel;
    vogleditor_QApiCallTreeModel *m_pApiCallTreeModel;
    vogleditor_QStateTreeModel *m_pStateTreeModel;

    vogleditor_QLaunchTracerDialog *m_pLaunchTracerDialog;

    vogleditor_QSnapshotOverlayWidget *m_pSnapshotStateOverlay;

    QColor m_searchTextboxBackgroundColor;
    bool m_bDelayUpdateUIForContext;
};

#endif // VOGLEDITOR_H
