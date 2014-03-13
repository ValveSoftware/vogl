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

#include <QFileDialog>
#include <QHBoxLayout>
#include <QItemSelection>
#include <QPalette>
#include <QSortFilterProxyModel>
#include <QSpacerItem>
#include <QToolButton>
#include <QMessageBox>

#include "ui_vogleditor.h"
#include "vogleditor.h"

#include "vogleditor_qapicalltreemodel.h"
#include "vogleditor_apicalltimelinemodel.h"

#include "vogl_platform.h"
#include "vogl_assert.h"
#include "vogl_file_utils.h"
#include "vogl_find_files.h"

#include "vogl_texture_format.h"
#include "vogl_trace_file_reader.h"
#include "vogl_trace_file_writer.h"
#include "vogleditor_qstatetreemodel.h"
#include "vogleditor_statetreetextureitem.h"
#include "vogleditor_statetreeprogramitem.h"
#include "vogleditor_statetreeshaderitem.h"
#include "vogleditor_statetreeframebufferitem.h"
#include "vogleditor_qtextureexplorer.h"

#define VOGLEDITOR_DISABLE_TAB(tab) ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(tab), false);
#define VOGLEDITOR_ENABLE_TAB(tab) ui->tabWidget->setTabEnabled(ui->tabWidget->indexOf(tab), true);

//----------------------------------------------------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------------------------------------------------
static void *g_actual_libgl_module_handle;
static QString g_PROJECT_NAME = "Vogl Editor";

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_proc_address_helper
//----------------------------------------------------------------------------------------------------------------------
static vogl_void_func_ptr_t vogl_get_proc_address_helper(const char *pName)
{
   VOGL_FUNC_TRACER

   vogl_void_func_ptr_t pFunc = g_actual_libgl_module_handle ? reinterpret_cast<vogl_void_func_ptr_t>(dlsym(g_actual_libgl_module_handle, pName)) : NULL;

   if ((!pFunc) && (GL_ENTRYPOINT(glXGetProcAddress)))
      pFunc = reinterpret_cast<vogl_void_func_ptr_t>( GL_ENTRYPOINT(glXGetProcAddress)(reinterpret_cast<const GLubyte*>(pName)) );

   return pFunc;
}


//----------------------------------------------------------------------------------------------------------------------
// load_gl
//----------------------------------------------------------------------------------------------------------------------
static bool load_gl()
{
   VOGL_FUNC_TRACER

   g_actual_libgl_module_handle = dlopen("libGL.so.1", RTLD_LAZY);
   if (!g_actual_libgl_module_handle)
   {
      vogl_error_printf("%s: Failed loading libGL.so.1!\n", VOGL_FUNCTION_NAME);
      return false;
   }

   GL_ENTRYPOINT(glXGetProcAddress) = reinterpret_cast<glXGetProcAddress_func_ptr_t>(dlsym(g_actual_libgl_module_handle, "glXGetProcAddress"));
   if (!GL_ENTRYPOINT(glXGetProcAddress))
   {
      vogl_error_printf("%s: Failed getting address of glXGetProcAddress() from libGL.so.1!\n", VOGL_FUNCTION_NAME);
      return false;
   }

   return true;
}

VoglEditor::VoglEditor(QWidget *parent) :
   QMainWindow(parent),
   ui(new Ui::VoglEditor),
   m_statusLabel(NULL),
   m_framebufferExplorer(NULL),
   m_textureExplorer(NULL),
   m_renderbufferExplorer(NULL),
   m_programExplorer(NULL),
   m_shaderExplorer(NULL),
   m_timeline(NULL),
   m_currentSnapshot(NULL),
   m_pCurrentCallTreeItem(NULL),
   m_pPlayButton(NULL),
   m_pPauseButton(NULL),
   m_pTrimButton(NULL),
   m_pStopButton(NULL),
   m_pTraceReader(NULL),
   m_pApicallTreeModel(NULL)
{
   ui->setupUi(this);

   if (load_gl())
   {
      vogl_init_actual_gl_entrypoints(vogl_get_proc_address_helper);
   }

   m_statusLabel = new QLabel(ui->statusBar);
   m_statusLabel->setBaseSize(150, 12);
   ui->statusBar->addWidget(m_statusLabel, 1);

   // setup framebuffer tab
   QGridLayout* framebufferTab_layout = new QGridLayout;
   m_framebufferExplorer = new vogleditor_QFramebufferExplorer(ui->framebufferTab);
   framebufferTab_layout->addWidget(m_framebufferExplorer, 0, 0);
   ui->framebufferTab->setLayout(framebufferTab_layout);

   // setup texture tab
   QGridLayout* textureTab_layout = new QGridLayout;
   m_textureExplorer = new vogleditor_QTextureExplorer(ui->textureTab);
   textureTab_layout->addWidget(m_textureExplorer, 0, 0);
   ui->textureTab->setLayout(textureTab_layout);

   // setup renderbuffer tab
   QGridLayout* rbTab_layout = new QGridLayout;
   m_renderbufferExplorer = new vogleditor_QTextureExplorer(ui->renderbufferTab);
   rbTab_layout->addWidget(m_renderbufferExplorer, 0, 0);
   ui->renderbufferTab->setLayout(rbTab_layout);

   // setup program tab
   QGridLayout* programTab_layout = new QGridLayout;
   m_programExplorer = new vogleditor_QProgramExplorer(ui->programTab);
   programTab_layout->addWidget(m_programExplorer, 0, 0);
   ui->programTab->setLayout(programTab_layout);

   // setup shader tab
   QGridLayout* shaderTab_layout = new QGridLayout;
   m_shaderExplorer = new vogleditor_QShaderExplorer(ui->shaderTab);
   shaderTab_layout->addWidget(m_shaderExplorer, 0, 0);
   ui->shaderTab->setLayout(shaderTab_layout);

   // setup timeline
   m_timeline = new vogleditor_QTimelineView();
   m_timeline->setMinimumHeight(100);
   ui->verticalLayout->addWidget(m_timeline);
   ui->verticalLayout->removeWidget(ui->timelineViewPlaceholder);

   // add buttons to toolbar
   m_pPlayButton = new QToolButton(ui->mainToolBar);
   m_pPlayButton->setText("Play trace");
   m_pPlayButton->setEnabled(false);

   m_pPauseButton = new QToolButton(ui->mainToolBar);
   m_pPauseButton->setText("Pause");
   m_pPauseButton->setEnabled(false);

   m_pTrimButton = new QToolButton(ui->mainToolBar);
   m_pTrimButton->setText("Trim");
   m_pTrimButton->setEnabled(false);

   m_pStopButton = new QToolButton(ui->mainToolBar);
   m_pStopButton->setText("Stop");
   m_pStopButton->setEnabled(false);

   // Temporarily hide the other buttons (until asyncronous playback is supported)
   m_pPauseButton->setVisible(false);
   m_pTrimButton->setVisible(false);
   m_pStopButton->setVisible(false);

   ui->mainToolBar->addWidget(m_pPlayButton);
   ui->mainToolBar->addWidget(m_pPauseButton);
   ui->mainToolBar->addWidget(m_pTrimButton);
   ui->mainToolBar->addWidget(m_pStopButton);

   connect(m_pPlayButton, SIGNAL(clicked()), this, SLOT(playCurrentTraceFile()));
   connect(m_pPauseButton, SIGNAL(clicked()), this, SLOT(pauseCurrentTraceFile()));
   connect(m_pTrimButton, SIGNAL(clicked()), this, SLOT(trimCurrentTraceFile()));
   connect(m_pStopButton, SIGNAL(clicked()), this, SLOT(stopCurrentTraceFile()));

   connect(m_programExplorer, SIGNAL(program_edited(vogl_program_state*)), this, SLOT(on_program_edited(vogl_program_state*)));

   reset_tracefile_ui();
}

VoglEditor::~VoglEditor()
{
   close_trace_file();
   delete ui;

   if (m_textureExplorer != NULL)
   {
       delete m_textureExplorer;
       m_textureExplorer = NULL;
   }

   if (m_renderbufferExplorer != NULL)
   {
       delete m_renderbufferExplorer;
       m_renderbufferExplorer = NULL;
   }

   if (m_programExplorer != NULL)
   {
       delete m_programExplorer;
       m_programExplorer = NULL;
   }

   if (m_shaderExplorer != NULL)
   {
       delete m_shaderExplorer;
       m_shaderExplorer = NULL;
   }
}

void VoglEditor::playCurrentTraceFile()
{
    QCursor origCursor = cursor();
    setCursor(Qt::WaitCursor);

    // update UI
    m_pPlayButton->setEnabled(false);
    m_pPauseButton->setEnabled(true);
    m_pTrimButton->setEnabled(true);
    m_pStopButton->setEnabled(true);
    m_statusLabel->clear();

    if (m_traceReplayer.replay(m_pTraceReader, m_pApicallTreeModel->root(), NULL, 0, true))
    {
        // replay was successful
        m_pPlayButton->setEnabled(true);
        m_pPauseButton->setEnabled(false);
        m_pTrimButton->setEnabled(false);
        m_pStopButton->setEnabled(false);
    }
    else
    {
        m_statusLabel->setText("Failed to replay the trace.");
    }

    setCursor(origCursor);
}

void VoglEditor::pauseCurrentTraceFile()
{
    if (m_traceReplayer.pause())
    {
       // update UI
       m_pPlayButton->setEnabled(true);
       m_pPauseButton->setEnabled(false);
       m_pTrimButton->setEnabled(true);
       m_pStopButton->setEnabled(true);
       m_statusLabel->clear();
    }
    else
    {
        m_statusLabel->setText("Failed to pause the trace replay.");
    }
}

void VoglEditor::trimCurrentTraceFile()
{
    if (m_traceReplayer.trim())
    {
        m_statusLabel->clear();
    }
    else
    {
        m_statusLabel->setText("Failed to trim the trace replay.");
    }
}

void VoglEditor::stopCurrentTraceFile()
{
    if (m_traceReplayer.stop())
    {
        // update UI
        m_pPlayButton->setEnabled(true);
        m_pPauseButton->setEnabled(false);
        m_pTrimButton->setEnabled(false);
        m_pStopButton->setEnabled(false);
        m_statusLabel->clear();
    }
    else
    {
        m_statusLabel->setText("Failed to stop the trace replay.");
    }
}

void VoglEditor::on_actionE_xit_triggered()
{
   qApp->quit();
}

void VoglEditor::on_action_Open_triggered()
{
   QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
           tr("GLI Binary Files (*.bin);;JSON Files (*.json)"));

   if (!fileName.isEmpty()) {
      vogl::dynamic_string filename;
      filename.set(fileName.toStdString().c_str());

      if (open_trace_file(filename) == false) {
          QMessageBox::critical(this, tr("Error"), tr("Could not open file"));
          return;
      }
   }
}

void VoglEditor::on_action_Close_triggered()
{
    close_trace_file();
}

void VoglEditor::close_trace_file()
{
   if (m_pTraceReader != NULL)
   {
      m_pTraceReader->close();
      vogl_delete(m_pTraceReader);
      m_pTraceReader = NULL;

      setWindowTitle(g_PROJECT_NAME);

      m_openFilename.clear();
      m_backtraceToJsonMap.clear();
      m_backtraceDoc.clear();
      m_searchApicallResults.clear();

      reset_tracefile_ui();

      ui->treeView->setModel(NULL);
      ui->machineInfoText->clear();
      ui->backtraceText->clear();
      m_timeline->setModel(NULL);
      m_timeline->repaint();

      if (m_pTimelineModel != NULL)
      {
          delete m_pTimelineModel;
          m_pTimelineModel = NULL;
      }
   }
}

void VoglEditor::write_child_api_calls(vogleditor_apiCallTreeItem* pItem, FILE* pFile)
{
    QString string = pItem->columnData(VOGL_ACTC_APICALL, Qt::DisplayRole).toString();
    vogl_fwrite(string.toStdString().c_str(), 1, string.size(), pFile);
    vogl_fwrite("\r\n", 1, 2, pFile);

    for (int i = 0; i < pItem->childCount(); i++)
    {
        write_child_api_calls(pItem->child(i), pFile);
    }
}

void VoglEditor::on_actionExport_API_Calls_triggered()
{
    QString suggestedName = m_openFilename;

    int lastIndex = suggestedName.lastIndexOf('-');
    if (lastIndex != -1)
    {
        suggestedName = suggestedName.remove(lastIndex, suggestedName.size() - lastIndex);
    }
    suggestedName += "-ApiCalls.txt";

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export API Calls"), suggestedName, tr("Text (*.txt)"));

    if (!fileName.isEmpty())
    {
        vogl::dynamic_string filename;
        filename.set(fileName.toStdString().c_str());

        FILE* pFile = vogl_fopen(filename.c_str(), "w");
        vogleditor_QApiCallTreeModel* pModel = static_cast<vogleditor_QApiCallTreeModel*>(ui->treeView->model());
        vogleditor_apiCallTreeItem* pRoot = pModel->root();
        for (int i = 0; i < pRoot->childCount(); i++)
        {
            write_child_api_calls(pRoot->child(i), pFile);
        }
        vogl_fclose(pFile);
    }
}

static const unsigned int VOGLEDITOR_SESSION_FILE_FORMAT_VERSION_1 = 1;
static const unsigned int VOGLEDITOR_SESSION_FILE_FORMAT_VERSION = VOGLEDITOR_SESSION_FILE_FORMAT_VERSION_1;

bool VoglEditor::load_session_from_disk(QString sessionFile)
{
    // open the json doc
    json_document sessionDoc;
    if (!sessionDoc.deserialize_file(sessionFile.toStdString().c_str()))
    {
        return false;
    }

    // look for expected metadata
    json_node* pMetadata = sessionDoc.get_root()->find_child_object("metadata");
    if (pMetadata == NULL)
    {
        return false;
    }

    const json_value& rFormatVersion = pMetadata->find_value("session_file_format_version");
    if (!rFormatVersion.is_valid())
    {
        return false;
    }

    if (rFormatVersion.as_uint32() != VOGLEDITOR_SESSION_FILE_FORMAT_VERSION_1)
    {
        return false;
    }

    // load base trace file
    json_node* pBaseTraceFile = sessionDoc.get_root()->find_child_object("base_trace_file");
    if (pBaseTraceFile == NULL)
    {
        return false;
    }

    const json_value& rBaseTraceFilePath = pBaseTraceFile->find_value("rel_path");
    const json_value& rBaseTraceFileUuid = pBaseTraceFile->find_value("uuid");

    if (!rBaseTraceFilePath.is_valid() || !rBaseTraceFileUuid.is_valid())
    {
        return false;
    }

    dynamic_string sessionPathName;
    dynamic_string sessionFileName;
    file_utils::split_path(sessionFile.toStdString().c_str(), sessionPathName, sessionFileName);

    dynamic_string traceFilePath = sessionPathName;
    traceFilePath.append(rBaseTraceFilePath.as_string());

    if (!open_trace_file(traceFilePath))
    {
        return false;
    }

    // TODO: verify UUID of the loaded trace file

    // load session data if it is available
    json_node* pSessionData = sessionDoc.get_root()->find_child_object("session_data");
    if (pSessionData != NULL)
    {
        const json_value& rSessionPath = pSessionData->find_value("rel_path");
        if (!rSessionPath.is_valid())
        {
            return false;
        }

        dynamic_string sessionDataPath = sessionPathName;
        sessionDataPath.append(rSessionPath.as_string());

        vogl_loose_file_blob_manager file_blob_manager;
        file_blob_manager.init(cBMFReadWrite, sessionDataPath.c_str());
        vogl_blob_manager* pBlob_manager = static_cast<vogl_blob_manager*>(&file_blob_manager);

        // load snapshots
        const json_node* pSnapshots = pSessionData->find_child_array("snapshots");
        for (unsigned int i = 0; i < pSnapshots->size(); i++)
        {
            const json_node* pSnapshotNode = pSnapshots->get_value_as_object(i);

            const json_value& uuid = pSnapshotNode->find_value("uuid");
            const json_value& isValid = pSnapshotNode->find_value("is_valid");
            const json_value& isEdited = pSnapshotNode->find_value("is_edited");
            const json_value& isOutdated = pSnapshotNode->find_value("is_outdated");
            const json_value& frameNumber = pSnapshotNode->find_value("frame_number");
            const json_value& callIndex = pSnapshotNode->find_value("call_index");
            const json_value& path = pSnapshotNode->find_value("rel_path");

            // make sure expected nodes are valid
            if (!isValid.is_valid() || !isEdited.is_valid() || !isOutdated.is_valid())
            {
                return false;
            }

            vogl_gl_state_snapshot* pSnapshot = NULL;

            if (path.is_valid() && isValid.as_bool() && uuid.is_valid())
            {
                dynamic_string snapshotPath = sessionDataPath;
                snapshotPath.append(path.as_string());

                // load the snapshot
                json_document snapshotDoc;
                if (!snapshotDoc.deserialize_file(snapshotPath.c_str()))
                {
                    return false;
                }

                // attempt to verify the snapshot file
                json_node* pSnapshotRoot = snapshotDoc.get_root();
                if (pSnapshotRoot == NULL)
                {
                    vogl_warning_printf("Invalid snapshot file at %s.", path.as_string_ptr());
                    continue;
                }

                const json_value& snapshotUuid = pSnapshotRoot->find_value("uuid");
                if (!snapshotUuid.is_valid())
                {
                    vogl_warning_printf("Invalid 'uuid' in snapshot file at %s.", path.as_string_ptr());
                    continue;
                }

                if (snapshotUuid.as_string() != uuid.as_string())
                {
                    vogl_warning_printf("Mismatching 'uuid' between snapshot file at %s and that stored in the session file at %s.", path.as_string_ptr(), sessionFile.toStdString().c_str());
                    continue;
                }

                vogl_ctypes trace_gl_ctypes(m_pTraceReader->get_sof_packet().m_pointer_sizes);
                pSnapshot = vogl_new(vogl_gl_state_snapshot);
                if (!pSnapshot->deserialize(*snapshotDoc.get_root(), *pBlob_manager, &trace_gl_ctypes))
                {
                    vogl_delete(pSnapshot);
                    pSnapshot = NULL;
                    vogl_warning_printf("Unable to deserialize the snapshot with uuid %s.", uuid.as_string_ptr());
                    continue;
                }
            }

            vogleditor_gl_state_snapshot* pContainer = vogl_new(vogleditor_gl_state_snapshot, pSnapshot);
            pContainer->set_edited(isEdited.as_bool());
            pContainer->set_outdated(isOutdated.as_bool());

            if (callIndex.is_valid())
            {
                // the snapshot is associated with an api call
                vogleditor_apiCallTreeItem* pItem = m_pApicallTreeModel->find_call_number(callIndex.as_uint64());
                if (pItem != NULL)
                {
                    pItem->set_snapshot(pContainer);
                }
                else
                {
                    vogl_warning_printf("Unable to find API call index %" PRIu64 " to load the snapshot into.", callIndex.as_uint64());
                    if (pSnapshot != NULL) { vogl_delete(pSnapshot); pSnapshot = NULL; }
                    if (pContainer != NULL) { vogl_delete(pContainer); pContainer = NULL; }
                }
            }
            else if (frameNumber.is_valid())
            {
                // the snapshot is associated with a frame.
                // frame snapshots have the additional requirement that the snapshot itself MUST exist since
                // we only save a frame snapshot if it is the inital frame and it has been edited.
                // If we allow NULL snapshots, that we could accidently remove the initial snapshot that was loaded with the trace file.
                if (pSnapshot != NULL)
                {
                    vogleditor_apiCallTreeItem* pItem = m_pApicallTreeModel->find_frame_number(frameNumber.as_uint64());
                    if (pItem != NULL)
                    {
                        pItem->set_snapshot(pContainer);
                    }
                    else
                    {
                        vogl_warning_printf("Unable to find frame number %" PRIu64 " to load the snapshot into.", frameNumber.as_uint64());
                        if (pSnapshot != NULL) { vogl_delete(pSnapshot); pSnapshot = NULL; }
                        if (pContainer != NULL) { vogl_delete(pContainer); pContainer = NULL; }
                    }
                }
            }
            else
            {
                vogl_warning_printf("Session file contains invalid call or frame number for snapshot with uuid %s", uuid.as_string_ptr());
                if (pSnapshot != NULL) { vogl_delete(pSnapshot); pSnapshot = NULL; }
                if (pContainer != NULL) { vogl_delete(pContainer); pContainer = NULL; }
            }
        }
    }

    return true;
}

/*
 * Below is a summary of the information that needs to be saved out in a session's json file so that we can reload the session and be fully-featured.
 * Note that not all of this information is currently supported (either by VoglEditor or the save/load functionality).
 *
 * sample data structure for version 1:
{
   "metadata" : {
      "session_file_format_version" : "0x1"  <- would need to be updated when organization of existing data is changed
   },
   "base_trace_file" : {
      "path" : "../traces/trimmed4.bin",
      "uuid" : [ 2761638124, 1361789091, 2623121922, 1789156619 ]
   },
   "session_data" : {
      "path" : "/home/peterl/voglproj/vogl_build/traces/trimmed4-vogleditor-sessiondata/",
      "snapshots" : [
         {
            "uuid" : "B346B680801ED2F5144E421DEA5EFDCC",
            "is_valid" : true,
            "is_edited" : false,
            "is_outdated" : false,
            "frame_number" : 0
         },
         {
            "uuid" : "BC261B884088DBEADF376A03A489F2B9",
            "is_valid" : true,
            "is_edited" : false,
            "is_outdated" : false,
            "call_index" : 881069,
            "path" : "/home/peterl/voglproj/vogl_build/traces/trimmed4-vogleditor-sessiondata/snapshot_call_881069.json"
         },
         {
            "uuid" : "176DE3DEAA437B871FE122C84D5432E3",
            "is_valid" : true,
            "is_edited" : true,
            "is_outdated" : false,
            "call_index" : 881075,
            "path" : "/home/peterl/voglproj/vogl_build/traces/trimmed4-vogleditor-sessiondata/snapshot_call_881075.json"
         },
         {
            "is_valid" : false,
            "is_edited" : false,
            "is_outdated" : true,
            "call_index" : 881080
         }
      ]
   }
}
*/
bool VoglEditor::save_session_to_disk(QString sessionFile)
{
    dynamic_string sessionPathName;
    dynamic_string sessionFileName;
    file_utils::split_path(sessionFile.toStdString().c_str(), sessionPathName, sessionFileName);

    // modify the session file name to make a sessiondata folder
    QString sessionDataFolder(sessionFileName.c_str());
    int lastIndex = sessionDataFolder.lastIndexOf('.');
    if (lastIndex != -1)
    {
        sessionDataFolder = sessionDataFolder.remove(lastIndex, sessionDataFolder.size() - lastIndex);
    }
    sessionDataFolder += "-sessiondata/";

    dynamic_string sessionDataPath = sessionPathName;
    sessionDataPath.append(sessionDataFolder.toStdString().c_str());
    file_utils::create_directories(sessionDataPath, false);

    vogl_loose_file_blob_manager file_blob_manager;
    file_blob_manager.init(cBMFReadWrite, sessionDataPath.c_str());
    vogl_blob_manager* pBlob_manager = static_cast<vogl_blob_manager*>(&file_blob_manager);

    QCursor origCursor = this->cursor();
    setCursor(Qt::WaitCursor);

    json_document sessionDoc;
    json_node& metadata = sessionDoc.get_root()->add_object("metadata");
    metadata.add_key_value("session_file_format_version", to_hex_string(VOGLEDITOR_SESSION_FILE_FORMAT_VERSION));

    // find relative path from session file to trace file
    QDir relativeAppDir;
    QString absoluteTracePath = relativeAppDir.absoluteFilePath(m_openFilename.toStdString().c_str());
    QDir absoluteSessionFileDir(sessionPathName.c_str());
    QString tracePathRelativeToSessionFile = absoluteSessionFileDir.relativeFilePath(absoluteTracePath);

    json_node& baseTraceFile = sessionDoc.get_root()->add_object("base_trace_file");
    baseTraceFile.add_key_value("rel_path", tracePathRelativeToSessionFile.toStdString().c_str());
    json_node &uuid_array = baseTraceFile.add_array("uuid");
    for (uint i = 0; i < VOGL_ARRAY_SIZE(m_pTraceReader->get_sof_packet().m_uuid); i++)
    {
        uuid_array.add_value(m_pTraceReader->get_sof_packet().m_uuid[i]);
    }

    json_node& sessionDataNode = sessionDoc.get_root()->add_object("session_data");
    sessionDataNode.add_key_value("rel_path", sessionDataFolder.toStdString().c_str());
    json_node& snapshotArray = sessionDataNode.add_array("snapshots");

    vogleditor_apiCallTreeItem* pItem = m_pApicallTreeModel->find_next_snapshot(NULL);
    vogleditor_apiCallTreeItem* pLastItem = NULL;
    bool bSavedSuccessfully = true;
    while (pItem != pLastItem && pItem != NULL)
    {
        dynamic_string filename;

        json_node& snapshotNode = snapshotArray.add_object();
        if (pItem->get_snapshot()->get_snapshot() != NULL)
        {
            dynamic_string strUUID;
            snapshotNode.add_key_value("uuid", pItem->get_snapshot()->get_snapshot()->get_uuid().get_string(strUUID));
        }
        snapshotNode.add_key_value("is_valid", pItem->get_snapshot()->is_valid());
        snapshotNode.add_key_value("is_edited", pItem->get_snapshot()->is_edited());
        snapshotNode.add_key_value("is_outdated", pItem->get_snapshot()->is_outdated());

        if (pItem->apiCallItem() != NULL)
        {
            uint64_t callIndex = pItem->apiCallItem()->globalCallIndex();
            snapshotNode.add_key_value("call_index", callIndex);
            if (pItem->get_snapshot()->get_snapshot() != NULL)
            {
                filename = filename.format("snapshot_call_%" PRIu64 ".json", callIndex);
                snapshotNode.add_key_value("rel_path", filename);
                dynamic_string filepath = sessionDataPath;
                filepath.append(filename);
                if (!save_snapshot_to_disk(pItem->get_snapshot()->get_snapshot(), filepath, pBlob_manager))
                {
                    bSavedSuccessfully = false;
                    break;
                }
            }
        }
        else if (pItem->frameItem() != NULL)
        {
            // the first frame of a trim will have a snapshot.
            // this should only be saved out if the snapshot has been edited
            uint64_t frameNumber = pItem->frameItem()->frameNumber();
            snapshotNode.add_key_value("frame_number", frameNumber);
            if (pItem->get_snapshot()->is_edited())
            {
                filename = filename.format("snapshot_frame_%" PRIu64 ".json", frameNumber);
                snapshotNode.add_key_value("rel_path", filename);
                dynamic_string filepath = sessionDataPath;
                filepath.append(filename);
                if (!save_snapshot_to_disk(pItem->get_snapshot()->get_snapshot(), filepath, pBlob_manager))
                {
                    bSavedSuccessfully = false;
                    break;
                }
            }
        }

        pLastItem = pItem;
        pItem = m_pApicallTreeModel->find_next_snapshot(pLastItem);
    }

    if (bSavedSuccessfully)
    {
        bSavedSuccessfully = sessionDoc.serialize_to_file(sessionFile.toStdString().c_str());
    }

    setCursor(origCursor);

    return bSavedSuccessfully;
}

bool VoglEditor::save_snapshot_to_disk(vogl_gl_state_snapshot *pSnapshot, dynamic_string filename, vogl_blob_manager *pBlob_manager)
{
    if (pSnapshot == NULL)
    {
        return false;
    }

    json_document doc;

    vogl_ctypes trace_gl_ctypes(m_pTraceReader->get_sof_packet().m_pointer_sizes);

    if (!pSnapshot->serialize(*doc.get_root(), *pBlob_manager, &trace_gl_ctypes))
    {
        vogl_error_printf("Failed serializing state snapshot document!\n");
        return false;
    }
    else if (!doc.serialize_to_file(filename.get_ptr(), true))
    {
        vogl_error_printf("Failed writing state snapshot to file \"%s\"!\n", filename.get_ptr());
        return false;
    }
    else
    {
        vogl_printf("Successfully wrote JSON snapshot to file \"%s\"\n", filename.get_ptr());
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// read_state_snapshot_from_trace
//----------------------------------------------------------------------------------------------------------------------
vogl_gl_state_snapshot* VoglEditor::read_state_snapshot_from_trace(vogl_trace_file_reader* pTrace_reader)
{
   vogl_ctypes trace_gl_ctypes(pTrace_reader->get_sof_packet().m_pointer_sizes);

   vogl_trace_packet keyframe_trace_packet(&trace_gl_ctypes);

   pTrace_reader->seek_to_frame(0);

   vogl_gl_state_snapshot *pSnapshot = NULL;
   bool found_snapshot = false;
   do
   {
      vogl_trace_file_reader::trace_file_reader_status_t read_status = pTrace_reader->read_next_packet();

      if ((read_status != vogl_trace_file_reader::cOK) && (read_status != vogl_trace_file_reader::cEOF))
      {
         vogl_error_printf("%s: Failed reading from keyframe trace file!\n", VOGL_FUNCTION_NAME);
         return NULL;
      }

      if ((read_status == vogl_trace_file_reader::cEOF) || (pTrace_reader->get_packet_type() == cTSPTEOF))
      {
         vogl_error_printf("%s: Failed finding state snapshot in keyframe file!\n", VOGL_FUNCTION_NAME);
         return NULL;
      }

      if (pTrace_reader->get_packet_type() != cTSPTGLEntrypoint)
         continue;

      if (!keyframe_trace_packet.deserialize(pTrace_reader->get_packet_buf().get_ptr(), pTrace_reader->get_packet_buf().size(), false))
      {
         vogl_error_printf("%s: Failed parsing GL entrypoint packet in keyframe file\n", VOGL_FUNCTION_NAME);
         return NULL;
      }

      const vogl_trace_gl_entrypoint_packet *pGL_packet = &pTrace_reader->get_packet<vogl_trace_gl_entrypoint_packet>();
      gl_entrypoint_id_t entrypoint_id = static_cast<gl_entrypoint_id_t>(pGL_packet->m_entrypoint_id);

      if (vogl_is_swap_buffers_entrypoint(entrypoint_id) || vogl_is_draw_entrypoint(entrypoint_id) || vogl_is_make_current_entrypoint(entrypoint_id))
      {
         vogl_error_printf("Failed finding state snapshot in keyframe file!\n");
         return NULL;
      }

      switch (entrypoint_id)
      {
         case VOGL_ENTRYPOINT_glInternalTraceCommandRAD:
         {
            GLuint cmd = keyframe_trace_packet.get_param_value<GLuint>(0);
            GLuint size = keyframe_trace_packet.get_param_value<GLuint>(1); VOGL_NOTE_UNUSED(size);

            if (cmd == cITCRKeyValueMap)
            {
               key_value_map &kvm = keyframe_trace_packet.get_key_value_map();

               dynamic_string cmd_type(kvm.get_string("command_type"));
               if (cmd_type == "state_snapshot")
               {
                  dynamic_string id(kvm.get_string("binary_id"));
                  if (id.is_empty())
                  {
                     vogl_error_printf("%s: Missing binary_id field in glInternalTraceCommandRAD key_valye_map command type: \"%s\"\n", VOGL_FUNCTION_NAME, cmd_type.get_ptr());
                     return NULL;
                  }

                  uint8_vec snapshot_data;
                  {
                     timed_scope ts("get_multi_blob_manager().get");
                     if (!pTrace_reader->get_multi_blob_manager().get(id, snapshot_data) || (snapshot_data.is_empty()))
                     {
                        vogl_error_printf("%s: Failed reading snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                        return NULL;
                     }
                  }

                  vogl_message_printf("%s: Deserializing state snapshot \"%s\", %u bytes\n", VOGL_FUNCTION_NAME, id.get_ptr(), snapshot_data.size());

                  json_document doc;
                  {
                     timed_scope ts("doc.binary_deserialize");
                     if (!doc.binary_deserialize(snapshot_data) || (!doc.get_root()))
                     {
                        vogl_error_printf("%s: Failed deserializing JSON snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                        return NULL;
                     }
                  }

                  pSnapshot = vogl_new(vogl_gl_state_snapshot);

                  timed_scope ts("pSnapshot->deserialize");
                  if (!pSnapshot->deserialize(*doc.get_root(), pTrace_reader->get_multi_blob_manager(), &trace_gl_ctypes))
                  {
                     vogl_delete(pSnapshot);
                     pSnapshot = NULL;

                     vogl_error_printf("%s: Failed deserializing snapshot blob data \"%s\"!\n", VOGL_FUNCTION_NAME, id.get_ptr());
                     return NULL;
                  }

                  found_snapshot = true;
               }
            }

            break;
         }
         default: break;
      }

   } while (!found_snapshot);

   return pSnapshot;
}


bool VoglEditor::open_trace_file(dynamic_string filename)
{
   QCursor origCursor = this->cursor();
   this->setCursor(Qt::WaitCursor);

   vogl_loose_file_blob_manager file_blob_manager;
   dynamic_string keyframe_trace_path(file_utils::get_pathname(filename.get_ptr()));
   file_blob_manager.init(cBMFReadable, keyframe_trace_path.get_ptr());

   dynamic_string actual_keyframe_filename;

   vogl_trace_file_reader* tmpReader = vogl_open_trace_file(filename, actual_keyframe_filename, NULL);

   if (tmpReader == NULL)
   {
      m_statusLabel->setText("Failed to open: ");
      m_statusLabel->setText(m_statusLabel->text().append(filename.c_str()));
      this->setCursor(origCursor);
      return false;
   }
   else
   {
       m_statusLabel->clear();
   }

   // now that we know the new trace file can be opened,
   // close the old one, and update the trace reader
   close_trace_file();
   m_pTraceReader = tmpReader;

   vogl_ctypes trace_ctypes;
   trace_ctypes.init(m_pTraceReader->get_sof_packet().m_pointer_sizes);

   m_pApicallTreeModel = new vogleditor_QApiCallTreeModel(m_pTraceReader);
   ui->treeView->setModel(m_pApicallTreeModel);

   if (ui->treeView->selectionModel() != NULL)
   {
      //connect(ui->treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(on_treeView_selectionChanged(const QItemSelection&, const QItemSelection&)));
      connect(ui->treeView->selectionModel(), SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(on_treeView_currentChanged(const QModelIndex &, const QModelIndex &)));
   }

   if (m_pApicallTreeModel->hasChildren())
   {
      ui->treeView->setExpanded(m_pApicallTreeModel->index(0,0), true);
      ui->treeView->setCurrentIndex(m_pApicallTreeModel->index(0,0));
   }

   int flagsColumnWidth = 30;
   ui->treeView->header()->setMinimumSectionSize(flagsColumnWidth);
   ui->treeView->header()->moveSection(VOGL_ACTC_FLAGS, 0);
   ui->treeView->setColumnWidth(VOGL_ACTC_FLAGS, flagsColumnWidth);

   int width = ui->treeView->width() - flagsColumnWidth - 30; // subtract a little extra for the scrollbar width
   ui->treeView->setColumnWidth(VOGL_ACTC_APICALL, width * 0.7);
   ui->treeView->setColumnWidth(VOGL_ACTC_INDEX, width * 0.15);
   ui->treeView->setColumnWidth(VOGL_ACTC_DURATION, width * 0.15);

   ui->searchTextBox->setEnabled(true);
   ui->searchPrevButton->setEnabled(true);
   ui->searchNextButton->setEnabled(true);

   ui->action_Close->setEnabled(true);
   ui->actionSave_Session->setEnabled(true);
   ui->actionExport_API_Calls->setEnabled(true);

   ui->prevSnapshotButton->setEnabled(true);
   ui->nextSnapshotButton->setEnabled(true);
   ui->prevDrawcallButton->setEnabled(true);
   ui->nextDrawcallButton->setEnabled(true);

   m_backtraceToJsonMap.clear();
   m_backtraceDoc.clear();

    // Extract backtrace map and machine info from trace archive
    if (m_pTraceReader->get_archive_blob_manager().is_initialized())
    {
        // backtrace
        uint8_vec backtrace_data;
        bool bBacktraceVisible = false;
        if (m_pTraceReader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME))
        {
            //$ TODO mikesart: read MAP_SYMS data here when symbols have been resolved.
            if (m_pTraceReader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_BACKTRACE_MAP_ADDRS_FILENAME, backtrace_data))
            {
                json_node* pRoot = m_backtraceDoc.get_root();
                if (m_backtraceDoc.deserialize((const char*)backtrace_data.get_ptr(), backtrace_data.size()))
                {
                    bBacktraceVisible = pRoot->size() > 0;
                    for (uint i = 0; i < pRoot->size(); i++)
                    {
                        json_node* pChild = pRoot->get_child(i);
                        uint32 index = 0;
                        VOGL_ASSERT("Backtrace node does not have an 'index' child" && pChild != NULL && pChild->get_value_as_uint32("index", index));
                        if (pChild != NULL && pChild->get_value_as_uint32("index", index))
                        {
                            m_backtraceToJsonMap.insert(index, pChild);
                        }
                    }
                }
            }
        }

        QList<int> backtraceSplitterSizes = ui->splitter_3->sizes();
        int backtraceSplitterTotalSize = backtraceSplitterSizes[0] + backtraceSplitterSizes[1];
        QList<int> newBacktraceSplitterSizes;
        if (!bBacktraceVisible)
        {
            newBacktraceSplitterSizes.append(backtraceSplitterTotalSize);
            newBacktraceSplitterSizes.append(0);
            ui->splitter_3->setSizes(newBacktraceSplitterSizes);
        }
        else
        {
            newBacktraceSplitterSizes << (backtraceSplitterTotalSize * 0.75)
                                      << (backtraceSplitterTotalSize * 0.25);
            ui->splitter_3->setSizes(newBacktraceSplitterSizes);
        }

        // machine info
        displayMachineInfo();
   }

   m_openFilename = filename.c_str();

   setWindowTitle(m_openFilename + " - " + g_PROJECT_NAME);

   ui->tabWidget->setCurrentWidget(ui->framebufferTab);

   // update toolbar
   m_pPlayButton->setEnabled(true);
   m_pPauseButton->setEnabled(false);
   m_pTrimButton->setEnabled(false);
   m_pStopButton->setEnabled(false);

   // timeline
   m_pTimelineModel = new vogleditor_apiCallTimelineModel(m_pApicallTreeModel->root());
   m_timeline->setModel(m_pTimelineModel);
   m_timeline->repaint();

   this->setCursor(origCursor);
   return true;
}

void VoglEditor::displayMachineInfoHelper(QString prefix, const QString& sectionKeyStr, const vogl::json_value& value, QString& rMachineInfoStr)
{
    if (value.is_array())
    {
        const json_node* pNode = value.get_node_ptr();
        for (uint element = 0; element < pNode->size(); element++)
        {
            dynamic_string elementStr = pNode->get_value(element).as_string();

            elementStr = elementStr.replace("\n", "\n\t");

            rMachineInfoStr += "\t";
            rMachineInfoStr += elementStr.get_ptr();
            rMachineInfoStr += "\n";
        }

        rMachineInfoStr += "\n";
    }
    else if (value.is_node())
    {
        // Check if this is the modoule list.
        bool is_module_list = (sectionKeyStr == "module_list");
        const json_node* pNode = value.get_node_ptr();

        for (uint i = 0; i < pNode->size(); i++)
        {
            dynamic_string key = pNode->get_key(i);
            const json_value &value2 = pNode->get_value(i);

            rMachineInfoStr += prefix;
            // If it's the module list, then the key is the filename and we want to display that last.
            if (!is_module_list)
                rMachineInfoStr += key.c_str();

            if (value2.is_array())
            {
                const json_node* pNode2 = value2.get_node_ptr();

                // If this it module_list, then we get these items: base address, address size, uuid
                // Check in btrace_get_machine_info() to see what's written there.
                for (uint element = 0; element < pNode2->size(); element++)
                {
                    const json_value &json_val = pNode2->get_value(element);

                    if (json_val.is_string())
                    {
                        dynamic_string str = pNode2->get_value(element).as_string();
                        rMachineInfoStr += str.c_str();
                    }
                    else
                    {
                        dynamic_string buf;
                        buf.format("%" PRIx64, json_val.as_uint64());
                        rMachineInfoStr += buf.c_str();
                    }

                    rMachineInfoStr += "\t";
                }
            }
            else
            {
                rMachineInfoStr += ": ";
                rMachineInfoStr += value2.as_string_ptr();
            }

            // Display the filename if this is the module_list.
            if (is_module_list)
                rMachineInfoStr += key.c_str();
            rMachineInfoStr += "\n";
        }

        rMachineInfoStr += "\n";
    }
    else if (value.is_string())
    {
        rMachineInfoStr += value.as_string_ptr();
    }
    else
    {
        rMachineInfoStr += value.as_string_ptr();
    }
}

void VoglEditor::displayMachineInfo()
{
    VOGL_ASSERT(m_pTraceReader != NULL);

    bool bMachineInfoVisible = false;
    if (m_pTraceReader->get_archive_blob_manager().does_exist(VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME))
    {
        uint8_vec machine_info_data;
        if (m_pTraceReader->get_archive_blob_manager().get(VOGL_TRACE_ARCHIVE_MACHINE_INFO_FILENAME, machine_info_data))
        {
            bMachineInfoVisible = true;
            json_document doc;
            json_node *pRoot = doc.get_root();
            if (doc.deserialize((const char*)machine_info_data.get_ptr(), machine_info_data.size()))
            {
                QString text;
                for (uint i = 0; i < pRoot->size(); i++)
                {
                    dynamic_string sectionKeyStr = pRoot->get_key(i);
                    text += pRoot->get_key(i).c_str();
                    text += "\n";

                    QString keyStr = sectionKeyStr.c_str();
                    displayMachineInfoHelper("\t", keyStr, pRoot->get_value(i), text);
                }

                ui->machineInfoText->setText(text);
            }
        }
    }

    if (bMachineInfoVisible)
    {
        if (ui->tabWidget->indexOf(ui->machineInfoTab) == -1)
        {
            // unhide the tab
            ui->tabWidget->insertTab(0, ui->machineInfoTab, "Machine Info");
        }
        else
        {
            VOGLEDITOR_ENABLE_TAB(ui->machineInfoTab);
        }
    }
    else
    {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->machineInfoTab));
    }
}

void VoglEditor::reset_tracefile_ui()
{
    ui->action_Close->setEnabled(false);
    ui->actionExport_API_Calls->setEnabled(false);
    ui->actionSave_Session->setEnabled(false);

    ui->prevSnapshotButton->setEnabled(false);
    ui->nextSnapshotButton->setEnabled(false);
    ui->prevDrawcallButton->setEnabled(false);
    ui->nextDrawcallButton->setEnabled(false);
    ui->searchTextBox->clear();
    ui->searchTextBox->setEnabled(false);
    ui->searchPrevButton->setEnabled(false);
    ui->searchNextButton->setEnabled(false);

    m_statusLabel->clear();
    m_pPlayButton->setEnabled(false);
    m_pPauseButton->setEnabled(false);
    m_pTrimButton->setEnabled(false);
    m_pStopButton->setEnabled(false);

    VOGLEDITOR_DISABLE_TAB(ui->machineInfoTab);

    reset_snapshot_ui();
}

void VoglEditor::reset_snapshot_ui()
{
    m_currentSnapshot = NULL;

    m_framebufferExplorer->clear();
    m_textureExplorer->clear();
    m_renderbufferExplorer->clear();
    m_programExplorer->clear();
    m_shaderExplorer->clear();

    ui->stateTreeView->setModel(NULL);

    QWidget* pCurrentTab = ui->tabWidget->currentWidget();

    VOGLEDITOR_DISABLE_TAB(ui->stateTab);
    VOGLEDITOR_DISABLE_TAB(ui->framebufferTab);
    VOGLEDITOR_DISABLE_TAB(ui->programTab);
    VOGLEDITOR_DISABLE_TAB(ui->shaderTab);
    VOGLEDITOR_DISABLE_TAB(ui->textureTab);
    VOGLEDITOR_DISABLE_TAB(ui->renderbufferTab);

    ui->tabWidget->setCurrentWidget(pCurrentTab);
}

/// This helper will most often return a pointer equal to the pCurSnapshot that is passed in, or NULL if the node does not have a snapshot
/// and also has no children. The pMostRecentSnapshot parameter will be updated to point to the desired snapshot.
/// This function does not follow a traditional DFS search because we need to find the desired snapshot then return the one before it.
/// An alternative approach would be to keep a stack of the found snapshots, or even to build up that stack / list as the user
/// generates new snapshots.
vogleditor_gl_state_snapshot* VoglEditor::findMostRecentSnapshot_helper(vogleditor_apiCallTreeItem* pItem, vogleditor_gl_state_snapshot*& pMostRecentSnapshot, const vogleditor_gl_state_snapshot* pCurSnapshot)
{
    // check if this item has a snapshot shot
    if (pItem->has_snapshot())
    {
        vogleditor_gl_state_snapshot* pTmp = pItem->get_snapshot();
        if (pTmp == pCurSnapshot)
        {
            // if we've reached the item with the current snapshot, we want to return the previous snapshot.
            return pTmp;
        }
        else
        {
            // update most recent snapshot
            pMostRecentSnapshot = pTmp;
        }
    }

    for (int i = 0; i < pItem->childCount(); i++)
    {
        vogleditor_gl_state_snapshot* pTmp = findMostRecentSnapshot_helper(pItem->child(i), pMostRecentSnapshot, pCurSnapshot);
        if (pTmp != NULL)
        {
            if (pTmp == pCurSnapshot)
            {
                // if we've reached the item with the current snapshot, we want to return the previous snapshot.
                return pTmp;
            }
            else
            {
                // update most recent snapshot
                pMostRecentSnapshot = pTmp;
            }
        }
    }

    return NULL;
}

/// This function exists just to simplify the interaction with the helper, so that there no confusion between
/// whether the returned value, or passed in reference parameter should be used as the most recent snapshot.
/// It will either return NULL if there is no recent snapshot (which should only happen for the very first snapshot
/// in a trace), or a pointer to a valid snapshot.
vogleditor_gl_state_snapshot* VoglEditor::findMostRecentSnapshot(vogleditor_apiCallTreeItem* pItem, const vogleditor_gl_state_snapshot* pCurSnapshot)
{
    vogleditor_gl_state_snapshot* pMostRecentSnapshot = NULL;
    findMostRecentSnapshot_helper(pItem, pMostRecentSnapshot, pCurSnapshot);
    return pMostRecentSnapshot;
}

void VoglEditor::update_ui_for_snapshot(vogleditor_gl_state_snapshot* pStateSnapshot)
{
   if (pStateSnapshot == NULL)
   {
      reset_snapshot_ui();
      return;
   }

   if (pStateSnapshot->is_valid() == false)
   {
       reset_snapshot_ui();
       return;
   }

   if (m_currentSnapshot == pStateSnapshot)
   {
       // no need to update if it is the same snapshot
       return;
   }

   m_currentSnapshot = pStateSnapshot;

   if (ui->stateTreeView->model() != NULL)
   {
      if (static_cast<vogleditor_QStateTreeModel*>(ui->stateTreeView->model())->get_snapshot() == m_currentSnapshot)
      {
         // displaying the same snapshot, return
         return;
      }
   }

   QCursor origCursor = this->cursor();
   this->setCursor(Qt::WaitCursor);

   // state viewer
   vogleditor_QStateTreeModel* pStateModel = new vogleditor_QStateTreeModel(NULL);

   vogleditor_QApiCallTreeModel* pTreeModel = static_cast<vogleditor_QApiCallTreeModel*>(ui->treeView->model());
   vogleditor_gl_state_snapshot* pBaseSnapshot = findMostRecentSnapshot(pTreeModel->root(), m_currentSnapshot);
   pStateModel->set_diff_base_snapshot(pBaseSnapshot);

   pStateModel->set_snapshot(pStateSnapshot);

   ui->stateTreeView->setModel(pStateModel);
   ui->stateTreeView->expandToDepth(1);
   ui->stateTreeView->setColumnWidth(0, ui->stateTreeView->width() * 0.5);

   VOGLEDITOR_ENABLE_TAB(ui->stateTab);

   if (pStateSnapshot->get_contexts().size() > 0)
   {
       vogl_trace_ptr_value curContextHandle = pStateSnapshot->get_cur_trace_context();
       if (curContextHandle != 0)
       {
           vogl_context_snapshot* pContext = pStateSnapshot->get_context(curContextHandle);

           // textures
           vogl_gl_object_state_ptr_vec textureObjects;
           pContext->get_all_objects_of_category(cGLSTTexture, textureObjects);
           m_textureExplorer->set_texture_objects(textureObjects);

           GLuint curActiveTextureUnit = pContext->get_general_state().get_value<GLuint>(GL_ACTIVE_TEXTURE);
           if (curActiveTextureUnit >= GL_TEXTURE0 && curActiveTextureUnit < (GL_TEXTURE0 + pContext->get_context_info().get_max_texture_image_units()))
           {
               GLuint cur2DBinding = pContext->get_general_state().get_value<GLuint>(GL_TEXTURE_2D_BINDING_EXT, curActiveTextureUnit - GL_TEXTURE0);
               displayTexture(cur2DBinding, false);
           }

           // renderbuffers
           vogl_gl_object_state_ptr_vec renderbufferObjects;
           pContext->get_all_objects_of_category(cGLSTRenderbuffer, renderbufferObjects);
           m_renderbufferExplorer->set_texture_objects(renderbufferObjects);
           if (renderbufferObjects.size() > 0) { VOGLEDITOR_ENABLE_TAB(ui->renderbufferTab); }

           // framebuffer
           vogl_gl_object_state_ptr_vec framebufferObjects;
           pContext->get_all_objects_of_category(cGLSTFramebuffer, framebufferObjects);
           m_framebufferExplorer->set_framebuffer_objects(framebufferObjects, *pContext, pStateSnapshot->get_default_framebuffer());
           GLuint64 curDrawFramebuffer = pContext->get_general_state().get_value<GLuint64>(GL_DRAW_FRAMEBUFFER_BINDING);
           displayFramebuffer(curDrawFramebuffer, false);

           // programs
           vogl_gl_object_state_ptr_vec programObjects;
           pContext->get_all_objects_of_category(cGLSTProgram, programObjects);
           m_programExplorer->set_program_objects(programObjects);
           GLuint64 curProgram = pContext->get_general_state().get_value<GLuint64>(GL_CURRENT_PROGRAM);
           m_programExplorer->set_active_program(curProgram);
           if (programObjects.size() > 0) { VOGLEDITOR_ENABLE_TAB(ui->programTab); }

           // shaders
           vogl_gl_object_state_ptr_vec shaderObjects;
           pContext->get_all_objects_of_category(cGLSTShader, shaderObjects);
           m_shaderExplorer->set_shader_objects(shaderObjects);
           if (curProgram != 0)
           {
               for (vogl_gl_object_state_ptr_vec::iterator iter = programObjects.begin(); iter != programObjects.end(); iter++)
               {
                   if ((*iter)->get_snapshot_handle() == curProgram)
                   {
                       vogl_program_state* pProgramState = static_cast<vogl_program_state*>(*iter);
                       if (pProgramState->get_attached_shaders().size() > 0)
                       {
                           uint curShader = pProgramState->get_attached_shaders()[0];
                           m_shaderExplorer->set_active_shader(curShader);
                       }
                       break;
                   }
               }
           }
           if (shaderObjects.size() > 0) { VOGLEDITOR_ENABLE_TAB(ui->shaderTab); }
       }
   }

   this->setCursor(origCursor);
}

void VoglEditor::on_stateTreeView_clicked(const QModelIndex &index)
{
   vogleditor_stateTreeItem* pStateItem = static_cast<vogleditor_stateTreeItem*>(index.internalPointer());
   if (pStateItem == NULL)
   {
      return;
   }

   switch(pStateItem->getStateType())
   {
   case vogleditor_stateTreeItem::cTEXTURE:
   {
      vogleditor_stateTreeTextureItem* pTextureItem = static_cast<vogleditor_stateTreeTextureItem*>(pStateItem);
      if (pTextureItem == NULL)
      {
         break;
      }

      displayTexture(pTextureItem->get_texture_state()->get_snapshot_handle(), true);

      break;
   }
   case vogleditor_stateTreeItem::cPROGRAM:
   {
      vogleditor_stateTreeProgramItem* pProgramItem = static_cast<vogleditor_stateTreeProgramItem*>(pStateItem);
      if (pProgramItem == NULL)
      {
         break;
      }

      displayProgram(pProgramItem->get_current_state()->get_snapshot_handle(), true);

      break;
   }
   case vogleditor_stateTreeItem::cSHADER:
   {
      vogleditor_stateTreeShaderItem* pShaderItem = static_cast<vogleditor_stateTreeShaderItem*>(pStateItem);
      if (pShaderItem == NULL)
      {
         break;
      }

      displayShader(pShaderItem->get_current_state()->get_snapshot_handle(), true);

      break;
   }
   case vogleditor_stateTreeItem::cFRAMEBUFFER:
   {
      vogleditor_stateTreeFramebufferItem* pFramebufferItem = static_cast<vogleditor_stateTreeFramebufferItem*>(pStateItem);
      if (pFramebufferItem == NULL)
      {
         break;
      }

      displayFramebuffer(pFramebufferItem->get_framebuffer_state()->get_snapshot_handle(), true);

      break;
   }
   case vogleditor_stateTreeItem::cDEFAULT:
   {
      return;
   }
   }
}

bool VoglEditor::displayShader(GLuint64 shaderHandle, bool bBringTabToFront)
{
    bool bDisplayed = false;
    if (m_shaderExplorer->set_active_shader(shaderHandle))
    {
        if (bBringTabToFront)
        {
            ui->tabWidget->setCurrentWidget(ui->shaderTab);
        }
    }

    return bDisplayed;
}

void VoglEditor::displayProgram(GLuint64 programHandle, bool bBringTabToFront)
{
    if (m_programExplorer->set_active_program(programHandle))
    {
        if (bBringTabToFront)
        {
            ui->tabWidget->setCurrentWidget(ui->programTab);
        }
    }
}

void VoglEditor::displayFramebuffer(GLuint64 framebufferHandle, bool bBringTabToFront)
{
    bool bDisplayedFBO = m_framebufferExplorer->set_active_framebuffer(framebufferHandle);

    if (bDisplayedFBO)
    {
        VOGLEDITOR_ENABLE_TAB(ui->framebufferTab);
        if (bBringTabToFront)
        {
            ui->tabWidget->setCurrentWidget(ui->framebufferTab);
        }
    }
}

bool VoglEditor::displayTexture(GLuint64 textureHandle, bool bBringTabToFront)
{
    bool bDisplayedTexture = m_textureExplorer->set_active_texture(textureHandle);

    if (bDisplayedTexture)
    {
        VOGLEDITOR_ENABLE_TAB(ui->textureTab);
        if (bBringTabToFront)
        {
            ui->tabWidget->setCurrentWidget(ui->textureTab);
        }
    }

    return bDisplayedTexture;
}

void VoglEditor::on_treeView_currentChanged(const QModelIndex & current, const QModelIndex & previous)
{
    VOGL_NOTE_UNUSED(previous);
    onApiCallSelected(current, false);
}

void VoglEditor::on_treeView_clicked(const QModelIndex &index)
{
    onApiCallSelected(index, true);
}

void VoglEditor::onApiCallSelected(const QModelIndex &index, bool bAllowStateSnapshot)
{
    vogleditor_apiCallTreeItem* pCallTreeItem = static_cast<vogleditor_apiCallTreeItem*>(index.internalPointer());
    if (pCallTreeItem == NULL)
    {
       return;
    }

    vogleditor_frameItem* pFrameItem = pCallTreeItem->frameItem();
    vogleditor_apiCallItem* pApiCallItem = pCallTreeItem->apiCallItem();

    if (bAllowStateSnapshot && pCallTreeItem == m_pCurrentCallTreeItem)
    {
        // we can only get snapshots for specific API calls
        if (pApiCallItem != NULL && pApiCallItem->needs_snapshot())
        {
           // get the snapshot after the current api call
           vogleditor_gl_state_snapshot* pNewSnapshot = NULL;
           QCursor origCursor = cursor();
           setCursor(Qt::WaitCursor);
           m_traceReplayer.replay(m_pTraceReader, m_pApicallTreeModel->root(), &pNewSnapshot, pApiCallItem->globalCallIndex(), false);
           setCursor(origCursor);
           pCallTreeItem->set_snapshot(pNewSnapshot);
        }
    }

    update_ui_for_snapshot(pCallTreeItem->get_snapshot());

    if (pApiCallItem != NULL && m_pCurrentCallTreeItem != pCallTreeItem)
    {
        if (m_backtraceToJsonMap.size() > 0)
        {
            QString tmp;
            json_node* pBacktraceNode = m_backtraceToJsonMap[(uint)pApiCallItem->backtraceHashIndex()];
            if (pBacktraceNode != NULL)
            {
                json_node* pAddrs = pBacktraceNode->find_child_array("addrs");
                json_node* pSyms = pBacktraceNode->find_child_array("syms");

                for (uint i = 0; i < pAddrs->size(); i++)
                {
                    tmp += pAddrs->get_value(i).as_string_ptr();
                    if (pSyms)
                    {
                        tmp += "\t";
                        tmp += pSyms->get_value(i).as_string_ptr();
                    }
                    tmp += "\n";
                }
            }
            ui->backtraceText->setText(tmp);
        }
    }

    if (pApiCallItem != NULL)
    {
        m_timeline->setCurrentApiCall(pApiCallItem->globalCallIndex());
    }

    if (pFrameItem != NULL)
    {
       m_timeline->setCurrentFrame(pFrameItem->frameNumber());
    }

    m_timeline->repaint();

    m_pCurrentCallTreeItem = pCallTreeItem;
}

void VoglEditor::selectApicallModelIndex(QModelIndex index, bool scrollTo, bool select)
{
    // make sure the index is visible
    QModelIndex parentIndex = index.parent();
    while (parentIndex.isValid())
    {
        if (ui->treeView->isExpanded(parentIndex) == false)
        {
            ui->treeView->expand(parentIndex);
        }
        parentIndex = parentIndex.parent();
    }

    // scroll to the index
    if (scrollTo)
    {
        ui->treeView->scrollTo(index);
    }

    // select the index
    if (select)
    {
        ui->treeView->setCurrentIndex(index);
    }

    if (m_searchApicallResults.size() > 0 && !ui->searchTextBox->text().isEmpty())
    {
        QItemSelectionModel* pSelection = ui->treeView->selectionModel();
        for (int i = 0; i < m_searchApicallResults.size(); i++)
        {
            pSelection->select(m_searchApicallResults[i], QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        ui->treeView->setSelectionModel(pSelection);
    }
}

void VoglEditor::on_searchTextBox_textChanged(const QString &searchText)
{
    QModelIndex curSearchIndex = ui->treeView->currentIndex();
    if (curSearchIndex.isValid() == false)
    {
        return;
    }

    // store original background color of the search text box so that it can be turned to red and later restored.
    static const QColor sOriginalTextBoxBackground = ui->searchTextBox->palette().base().color();

    // clear previous items
    QItemSelectionModel* pSelection = ui->treeView->selectionModel();
    if (pSelection != NULL)
    {
        for (int i = 0; i < m_searchApicallResults.size(); i++)
        {
            pSelection->select(m_searchApicallResults[i], QItemSelectionModel::Clear | QItemSelectionModel::Rows);
        }
        ui->treeView->setSelectionModel(pSelection);
    }

    // find new matches
    m_searchApicallResults.clear();
    if (m_pApicallTreeModel != NULL)
    {
        m_searchApicallResults = m_pApicallTreeModel->find_search_matches(searchText);
    }

    // if there are matches, restore the textbox background to its original color
    if (m_searchApicallResults.size() > 0)
    {
        QPalette palette(ui->searchTextBox->palette());
        palette.setColor(QPalette::Base, sOriginalTextBoxBackground);
        ui->searchTextBox->setPalette(palette);
    }

    // select new items
    if (!searchText.isEmpty())
    {
        if (m_searchApicallResults.size() > 0)
        {
            // scroll to the first result, but don't select it
            selectApicallModelIndex(m_searchApicallResults[0], true, false);
        }
        else
        {
            // no items were found, so set the textbox background to red
            QPalette palette(ui->searchTextBox->palette());
            palette.setColor(QPalette::Base, Qt::red);
            ui->searchTextBox->setPalette(palette);
        }
    }
}

void VoglEditor::on_searchNextButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        QModelIndex index = m_pApicallTreeModel->find_next_search_result(m_pCurrentCallTreeItem, ui->searchTextBox->text());
        selectApicallModelIndex(index, true, true);
    }
}

void VoglEditor::on_searchPrevButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        QModelIndex index = m_pApicallTreeModel->find_prev_search_result(m_pCurrentCallTreeItem, ui->searchTextBox->text());
        selectApicallModelIndex(index, true, true);
    }
}

void VoglEditor::on_prevSnapshotButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        vogleditor_apiCallTreeItem* pPrevItemWithSnapshot = m_pApicallTreeModel->find_prev_snapshot(m_pCurrentCallTreeItem);
        selectApicallModelIndex(m_pApicallTreeModel->indexOf(pPrevItemWithSnapshot), true, true);
        ui->treeView->setFocus();
    }
}

void VoglEditor::on_nextSnapshotButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        vogleditor_apiCallTreeItem* pNextItemWithSnapshot = m_pApicallTreeModel->find_next_snapshot(m_pCurrentCallTreeItem);
        selectApicallModelIndex(m_pApicallTreeModel->indexOf(pNextItemWithSnapshot), true, true);
        ui->treeView->setFocus();
    }
}

void VoglEditor::on_prevDrawcallButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        vogleditor_apiCallTreeItem* pPrevItem = m_pApicallTreeModel->find_prev_drawcall(m_pCurrentCallTreeItem);
        selectApicallModelIndex(m_pApicallTreeModel->indexOf(pPrevItem), true, true);
        ui->treeView->setFocus();
    }
}

void VoglEditor::on_nextDrawcallButton_clicked()
{
    if (m_pApicallTreeModel != NULL)
    {
        vogleditor_apiCallTreeItem* pNextItem = m_pApicallTreeModel->find_next_drawcall(m_pCurrentCallTreeItem);
        selectApicallModelIndex(m_pApicallTreeModel->indexOf(pNextItem), true, true);
        ui->treeView->setFocus();
    }
}


void VoglEditor::on_program_edited(vogl_program_state* pNewProgramState)
{
    VOGL_NOTE_UNUSED(pNewProgramState);

    m_currentSnapshot->set_edited(true);

    // update all the snapshot flags
    bool bFoundEditedSnapshot = false;
    recursive_update_snapshot_flags(m_pApicallTreeModel->root(), bFoundEditedSnapshot);

    // give the tree view focus so that it redraws. This is something of a hack, we don't really want to be changing around which control has focus,
    // but right now I don't see it being a major issue. It may be an issue later on depending on how we implement more state editing (ie, if arrow
    // keys are used to cycle through options in a drop-down, and the tree view gets focus, the arrow keys would then start changing the selected
    // API call instead of cycling through state options).
    ui->treeView->setFocus();
}

// if an edited snapshot has already been found, mark the node (and all children) as dirty.
void VoglEditor::recursive_update_snapshot_flags(vogleditor_apiCallTreeItem* pItem, bool& bFoundEditedSnapshot)
{
    // check if this item has a snapshot shot
    if (pItem->has_snapshot())
    {
        if (!bFoundEditedSnapshot)
        {
            if (pItem->get_snapshot()->is_edited())
            {
                bFoundEditedSnapshot = true;
            }
            else
            {
                pItem->get_snapshot()->set_outdated(false);
            }
        }
        else
        {
            pItem->get_snapshot()->set_outdated(true);
        }
    }

    for (int i = 0; i < pItem->childCount(); i++)
    {
        recursive_update_snapshot_flags(pItem->child(i), bFoundEditedSnapshot);
    }
}

#undef VOGLEDITOR_DISABLE_TAB
#undef VOGLEDITOR_ENABLE_TAB

void VoglEditor::on_actionSave_Session_triggered()
{
    QString baseName = m_openFilename;

    int lastIndex = baseName.lastIndexOf('.');
    if (lastIndex != -1)
    {
        baseName = baseName.remove(lastIndex, baseName.size() - lastIndex);
    }

    QString suggestedName = baseName + "-vogleditor.json";

    QString sessionFilename = QFileDialog::getSaveFileName(this, tr("Save Debug Session"), suggestedName, tr("JSON (*.json)"));

    if (!save_session_to_disk(sessionFilename))
    {
        m_statusLabel->setText("ERROR: Failed to save session");
    }
}

void VoglEditor::on_actionOpen_Session_triggered()
{
    QString sessionFilename = QFileDialog::getOpenFileName(this, tr("Load Debug Session"), QString(), tr("JSON (*.json)"));

    QCursor origCursor = this->cursor();
    setCursor(Qt::WaitCursor);

    if (!load_session_from_disk(sessionFilename))
    {
        m_statusLabel->setText("ERROR: Failed to load session");
    }

    setCursor(origCursor);
}
