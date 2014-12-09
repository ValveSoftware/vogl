#include "vogleditor_qlaunchtracerdialog.h"
#include <QDir>
#include <QFileDialog>
#include <QProcessEnvironment>
#include "ui_vogleditor_qlaunchtracerdialog.h"

vogleditor_QLaunchTracerDialog::vogleditor_QLaunchTracerDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::VoglEditor_QLaunchTracerDialog)
{
    ui->setupUi(this);

    QDir appDirectory(QCoreApplication::applicationDirPath());
    QDir vogltraceDir(appDirectory.absoluteFilePath("./"));

// only enable the steam launcher box if the script is available
#if defined(PLATFORM_WINDOWS)
    // vogl64.exe / vogl32.exe does not yet properly support windows, so disable it
    // Eventually, remove this line and enable the code below so that vogl*.exe can be found
    ui->vogltraceCheckBox->setEnabled(false);

//// the '.exe' is necessary on Windows to find the file, but not necessary to execute it.
//ui->vogltraceCheckBox->setEnabled(vogltraceDir.exists((sizeof(void *) > 4) ? "vogl64.exe" : "vogl32.exe"));
#else
    ui->vogltraceCheckBox->setEnabled(vogltraceDir.exists((sizeof(void *) > 4) ? "vogl64" : "vogl32"));
#endif
}

vogleditor_QLaunchTracerDialog::~vogleditor_QLaunchTracerDialog()
{
    delete ui;
}

QString vogleditor_QLaunchTracerDialog::get_command_line()
{
    QDir appDirectory(QCoreApplication::applicationDirPath());

    QString cmdline;
    QString executable;
    if (ui->vogltraceCheckBox->isChecked())
    {
        executable = appDirectory.absoluteFilePath((sizeof(void *) > 4) ? "./vogl64" : "./vogl32");
        executable += " trace ";

        cmdline += get_application_to_launch();

        if (get_trace_file_path().size() > 0)
        {
            cmdline += " --vogl_tracefile " + get_trace_file_path();
        }

        if (ui->forceDebugContextCheckBox->isChecked())
        {
            cmdline += " --vogl_force_debug_context";
        }

        if (ui->disableProgramBinaryCheckBox->isChecked())
        {
            cmdline += " --vogl_disable_gl_program_binary";
        }

        if (ui->gatherCallStacksCheckBox->isChecked())
        {
            cmdline += " --vogl_backtrace_all_calls";
        }

        if (ui->argumentsLineEdit->text().size() > 0)
        {
            cmdline += " -- " + ui->argumentsLineEdit->text();
        }

        // steam launcher must come at the beginning of cmd line
        cmdline = executable + cmdline;
    }
    else
    {
        executable = get_application_to_launch();
        executable += " " + ui->argumentsLineEdit->text();

        // vogl_cmd_line and ld_preload are setup in get_process_environment()

        cmdline = executable;
    }

    return cmdline;
}

QProcessEnvironment vogleditor_QLaunchTracerDialog::get_process_environment()
{
    QDir appDirectory(QCoreApplication::applicationDirPath());

    m_process_environment = QProcessEnvironment::systemEnvironment();

    if (ui->vogltraceCheckBox->isChecked() == false)
    {
        if (get_trace_file_path().size() > 0)
        {
            QString VOGL_CMD_LINE;
            if (get_trace_file_path().size() > 0)
            {
                VOGL_CMD_LINE += "--vogl_tracefile " + get_trace_file_path();
            }

            if (ui->forceDebugContextCheckBox->isChecked())
            {
                VOGL_CMD_LINE += " --vogl_force_debug_context";
            }

            if (ui->disableProgramBinaryCheckBox->isChecked())
            {
                VOGL_CMD_LINE += " --vogl_disable_gl_program_binary";
            }

            if (ui->gatherCallStacksCheckBox->isChecked())
            {
                VOGL_CMD_LINE += " --vogl_backtrace_all_calls";
            }

            m_process_environment.insert("VOGL_CMD_LINE", VOGL_CMD_LINE);
        }

        QString libvogltrace32 = appDirectory.absoluteFilePath("./libvogltrace32.so");
        QString libvogltrace64 = appDirectory.absoluteFilePath("./libvogltrace64.so");
        QString LD_PRELOAD = libvogltrace32 + ":" + libvogltrace64;

        if (getenv("LD_PRELOAD"))
        {
            LD_PRELOAD += ":$LD_PRELOAD";
        }

        LD_PRELOAD += "";

        m_process_environment.insert("LD_PRELOAD", LD_PRELOAD);
    }

    return m_process_environment;
}

QString vogleditor_QLaunchTracerDialog::get_trace_file_path()
{
    // make sure it has the correct extension
    if (!ui->traceFileLineEdit->text().endsWith(".bin"))
    {
        ui->traceFileLineEdit->setText(ui->traceFileLineEdit->text() + ".bin");
    }

    return ui->traceFileLineEdit->text();
}

QString vogleditor_QLaunchTracerDialog::get_application_to_launch()
{
    return ui->applicationLineEdit->text();
}

void vogleditor_QLaunchTracerDialog::on_applicationLineEdit_textChanged(const QString &text)
{
    check_inputs();
}

void vogleditor_QLaunchTracerDialog::on_traceFileLineEdit_textChanged(const QString &text)
{
    check_inputs();
}

void vogleditor_QLaunchTracerDialog::check_inputs()
{
    bool applicationFileEntered = ui->applicationLineEdit->text().size() != 0;
    bool traceFileEntered = ui->traceFileLineEdit->text().size() != 0;

    ui->okButton->setEnabled(applicationFileEntered && traceFileEntered);
}

void vogleditor_QLaunchTracerDialog::on_findApplicationButton_clicked()
{
    // open file dialog
    QString suggestedName = ui->applicationLineEdit->text();
    QString selectedName = QFileDialog::getOpenFileName(this, tr("Find Application to Trace"), suggestedName, "");

    if (!selectedName.isEmpty())
    {
        ui->applicationLineEdit->setText(selectedName);
    }
}

void vogleditor_QLaunchTracerDialog::on_findTraceFileButton_clicked()
{
    // open file dialog
    QString suggestedName = ui->traceFileLineEdit->text();
    QString selectedName = QFileDialog::getSaveFileName(this, tr("Output Trace File"), suggestedName, tr("Trace file (*.bin)"));

    if (!selectedName.isEmpty())
    {
        ui->traceFileLineEdit->setText(selectedName);
    }
}

void vogleditor_QLaunchTracerDialog::on_vogltraceCheckBox_clicked(bool checked)
{
}
