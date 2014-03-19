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
#include <QLabel>
#include <QModelIndexList>

#include "vogl_dynamic_string.h"
#include "vogl_hash_map.h"
#include "vogl_json.h"
#include "vogleditor_qframebufferexplorer.h"
#include "vogleditor_qprogramexplorer.h"
#include "vogleditor_qshaderexplorer.h"
#include "vogleditor_qtextureexplorer.h"
#include "vogleditor_qtimelineview.h"
#include "vogleditor_tracereplayer.h"

namespace Ui {
class VoglEditor;
}

typedef uint64_t GLuint64;

class vogleditor_QTextureExplorer;

class QItemSelection;
class QModelIndex;
class QSortFilterProxyModel;
class QToolButton;
class vogl_context_snapshot;
class vogl_framebuffer_state;
class vogl_program_state;
class vogl_replay_window;
class vogl_shader_state;
class vogl_texture_state;
class vogl_trace_file_reader;
class vogl_trace_file_writer;
class vogl_trace_packet;
class vogl_gl_state_snapshot;
class vogleditor_apiCallTimelineModel;
class vogleditor_apiCallTreeItem;
class vogleditor_gl_state_snapshot;
class vogleditor_QApiCallTreeModel;
class vogleditor_QStateTreeModel;

class VoglEditor : public QMainWindow
{
   Q_OBJECT

public:
   explicit VoglEditor(QWidget* parent = 0);
   ~VoglEditor();

   bool open_trace_file(vogl::dynamic_string filename);
   void close_trace_file();

private slots:
   void on_action_Open_triggered();
   void on_action_Close_triggered();
   void on_actionE_xit_triggered();
   void on_actionExport_API_Calls_triggered();

   void on_treeView_currentChanged(const QModelIndex & current, const QModelIndex & previous);

   void on_treeView_clicked(const QModelIndex& index);

   void playCurrentTraceFile();
   void pauseCurrentTraceFile();
   void trimCurrentTraceFile();
   void stopCurrentTraceFile();

   void on_stateTreeView_clicked(const QModelIndex &index);

   void on_searchTextBox_textChanged(const QString &searchText);
   void on_searchNextButton_clicked();
   void on_searchPrevButton_clicked();
   void on_prevSnapshotButton_clicked();
   void on_nextSnapshotButton_clicked();
   void on_prevDrawcallButton_clicked();
   void on_nextDrawcallButton_clicked();

   void on_program_edited(vogl_program_state* pNewProgramState);   

   void on_actionSave_Session_triggered();

   void on_actionOpen_Session_triggered();

   void on_searchTextBox_returnPressed();

private:
   Ui::VoglEditor* ui;

   void onApiCallSelected(const QModelIndex &index, bool bAllowStateSnapshot);
   bool displayTexture(GLuint64 textureHandle, bool bBringTabToFront);
   void displayFramebuffer(GLuint64 framebufferHandle, bool bBringTabToFront);
   bool displayShader(GLuint64 shaderHandle, bool bBringTabToFront);
   void displayProgram(GLuint64 programHandle, bool bBringTabToFront);

   // temp?
   vogl_gl_state_snapshot *read_state_snapshot_from_trace(vogl_trace_file_reader* pTrace_reader);

   void reset_tracefile_ui();
   void reset_snapshot_ui();

   void update_ui_for_snapshot(vogleditor_gl_state_snapshot *pStateSnapshot);
   void displayMachineInfoHelper(QString prefix, const QString& sectionKeyStr, const vogl::json_value& value, QString& rMachineInfoStr);
   void displayMachineInfo();
   void recursive_update_snapshot_flags(vogleditor_apiCallTreeItem* pItem, bool& bFoundEditedSnapshot);

   vogleditor_gl_state_snapshot* findMostRecentSnapshot_helper(vogleditor_apiCallTreeItem* pItem, vogleditor_gl_state_snapshot*& pMostRecentSnapshot, const vogleditor_gl_state_snapshot* pCurSnapshot);
   vogleditor_gl_state_snapshot* findMostRecentSnapshot(vogleditor_apiCallTreeItem* pItem, const vogleditor_gl_state_snapshot* pCurSnapshot);

   void selectApicallModelIndex(QModelIndex index, bool scrollTo, bool select);

   void write_child_api_calls(vogleditor_apiCallTreeItem* pItem, FILE* pFile);

   bool load_session_from_disk(QString sessionFile);
   bool save_session_to_disk(QString sessionFile);
   bool save_snapshot_to_disk(vogl_gl_state_snapshot* pSnapshot, dynamic_string filename, vogl_blob_manager *pBlob_manager);

   QString m_openFilename;
   QLabel* m_pStatusLabel;
   vogleditor_QFramebufferExplorer* m_pFramebufferExplorer;
   vogleditor_QTextureExplorer* m_pTextureExplorer;
   vogleditor_QTextureExplorer* m_pRenderbufferExplorer;
   vogleditor_QProgramExplorer* m_pProgramExplorer;
   vogleditor_QShaderExplorer* m_pShaderExplorer;
   vogleditor_QTimelineView* m_timeline;

   QGridLayout* m_pFramebufferTab_layout;
   QGridLayout* m_pTextureTab_layout;
   QGridLayout* m_pRenderbufferTab_layout;
   QGridLayout* m_pProgramTab_layout;
   QGridLayout* m_pShaderTab_layout;

   vogleditor_gl_state_snapshot* m_currentSnapshot;
   vogleditor_apiCallTreeItem* m_pCurrentCallTreeItem;

   QToolButton* m_pPlayButton;
   QToolButton* m_pPauseButton;
   QToolButton* m_pTrimButton;
   QToolButton* m_pStopButton;

   vogleditor_traceReplayer m_traceReplayer;
   vogl_trace_file_reader* m_pTraceReader;
   vogl::json_document m_backtraceDoc;
   vogl::hash_map<vogl::uint32, vogl::json_node*> m_backtraceToJsonMap;

   vogleditor_apiCallTimelineModel* m_pTimelineModel;
   vogleditor_QApiCallTreeModel* m_pApiCallTreeModel;
   vogleditor_QStateTreeModel* m_pStateTreeModel;

   QColor m_searchTextboxBackgroundColor;
};

#endif // VOGLEDITOR_H
