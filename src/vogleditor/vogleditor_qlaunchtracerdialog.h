#ifndef VOGLEDITOR_QLAUNCHTRACERDIALOG_H
#define VOGLEDITOR_QLAUNCHTRACERDIALOG_H

#include <QDialog>
#include <QProcessEnvironment>

namespace Ui {
class VoglEditor_QLaunchTracerDialog;
}

class vogleditor_QLaunchTracerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit vogleditor_QLaunchTracerDialog(QWidget *parent = 0);
    ~vogleditor_QLaunchTracerDialog();

    QString get_application_to_launch();
    QString get_command_line();
    QProcessEnvironment get_process_environment();
    QString get_trace_file_path();

    bool validate_inputs();

private slots:
    void on_findApplicationButton_clicked();

    void on_findTraceFileButton_clicked();

    void on_steamLauncherCheckBox_clicked(bool checked);

private:
    Ui::VoglEditor_QLaunchTracerDialog *ui;
    QProcessEnvironment m_process_environment;
};

#endif // VOGLEDITOR_QLAUNCHTRACERDIALOG_H
