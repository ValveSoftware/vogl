#include "vogleditor_qsettingsdialog.h"
#include "ui_vogleditor_qsettingsdialog.h"

#include "vogleditor_settings.h"

vogleditor_QSettingsDialog::vogleditor_QSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::vogleditor_QSettingsDialog)
{
    ui->setupUi(this);

    QString strSettings = g_settings.to_string();

    ui->textEdit->setText(strSettings);
}

vogleditor_QSettingsDialog::~vogleditor_QSettingsDialog()
{
    delete ui;
}

void vogleditor_QSettingsDialog::save(const char* settingsFile)
{
    g_settings.from_string(ui->textEdit->toPlainText().toStdString().c_str());
    g_settings.save(settingsFile);
}
