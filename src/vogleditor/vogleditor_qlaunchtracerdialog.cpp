#include "vogleditor_qlaunchtracerdialog.h"
#include <QDir>
#include <QFileDialog>
#include <QProcessEnvironment>
#include "ui_vogleditor_qlaunchtracerdialog.h"

vogleditor_QLaunchTracerDialog::vogleditor_QLaunchTracerDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VoglEditor_QLaunchTracerDialog)
{
    ui->setupUi(this);

    QDir appDirectory(QCoreApplication::applicationDirPath());
    QDir steamLauncherDir(appDirectory.absoluteFilePath("../../bin/"));

    // only enable the steam launcher box if the script is available
    ui->steamLauncherCheckBox->setEnabled(steamLauncherDir.exists("steamlauncher.sh"));
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
    if (ui->steamLauncherCheckBox->isChecked())
    {
        executable = appDirectory.absoluteFilePath("../../bin/steamlauncher.sh");

        if (ui->amd64CheckBox->isChecked())
        {
            cmdline += " --amd64";
        }

        cmdline += " --gameid " + get_application_to_launch();

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

        cmdline += " " + ui->argumentsLineEdit->text();

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

    if (ui->steamLauncherCheckBox->isChecked() == false)
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

        QString LD_PRELOAD = "";
        if (ui->amd64CheckBox->isChecked())
        {
            LD_PRELOAD += appDirectory.absoluteFilePath("../../vogl_build/bin/libvogltrace64.so");
        }
        else
        {
            LD_PRELOAD += appDirectory.absoluteFilePath("../../vogl_build/bin/libvogltrace32.so");
        }

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
    return ui->traceFileLineEdit->text();
}

QString vogleditor_QLaunchTracerDialog::get_application_to_launch()
{
    return ui->applicationLineEdit->text();
}

bool vogleditor_QLaunchTracerDialog::validate_inputs()
{
    if (ui->applicationLineEdit->text().size() == 0)
    {
        ui->applicationLineEdit->setFocus();
        return false;
    }

    if (ui->traceFileLineEdit->text().size() == 0)
    {
        ui->traceFileLineEdit->setFocus();
        return false;
    }
    else
    {
        // make sure it has the correct extension
        if (!ui->traceFileLineEdit->text().endsWith(".bin"))
        {
            ui->traceFileLineEdit->setText(ui->traceFileLineEdit->text() + ".bin");
        }
    }

    return true;
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
