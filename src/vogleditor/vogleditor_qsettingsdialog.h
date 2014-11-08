#ifndef VOGLEDITOR_QSETTINGSDIALOG_H
#define VOGLEDITOR_QSETTINGSDIALOG_H

#include <QDialog>

namespace Ui
{
    class vogleditor_QSettingsDialog;
}

class vogleditor_QSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit vogleditor_QSettingsDialog(QWidget *parent = 0);
    ~vogleditor_QSettingsDialog();

    void save(const char *settingsFile);

private:
    Ui::vogleditor_QSettingsDialog *ui;
};

#endif // VOGLEDITOR_QSETTINGSDIALOG_H
