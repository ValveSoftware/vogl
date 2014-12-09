#include "vogleditor_qdumpdialog.h"
#include "ui_vogleditor_qdumpdialog.h"
#include <QFileDialog>
#include <QMessageBox>

vogleditor_QDumpDialog::vogleditor_QDumpDialog(QString parentTraceFile, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::vogleditor_QDumpDialog),
      m_minGlCall(0),
      m_maxGlCall(1000000),
      m_first_gl_call("0"),
      m_last_gl_call("1000000"),
      m_prefix("")
{
    ui->setupUi(this);
    ui->dumpFirstLineEdit->setText(m_first_gl_call);
    ui->dumpLastLineEdit->setText(m_last_gl_call);
    ui->dumpPrefixLineEdit->setText(m_prefix);
}

vogleditor_QDumpDialog::~vogleditor_QDumpDialog()
{
    delete ui;
}

void vogleditor_QDumpDialog::on_buttonBox1_clicked(QAbstractButton *button)
{
    if (button == (QAbstractButton *)ui->buttonBox1->button(QDialogButtonBox::Ok))
    //    if (button == ui->buttonBox1->button(QDialogButtonBox::Ok))
    {
        // validate the dump start gl call
        bool bValidCall = false;
        uint tmpFirst = ui->dumpFirstLineEdit->text().toUInt(&bValidCall);
        bValidCall = bValidCall && (tmpFirst <= m_maxGlCall);

        if (!bValidCall)
        {
            QMessageBox::warning(this, tr("Invalid Dump First GL Call"), tr("Please enter a valid gl call number at which to start the dump."),
                                 QMessageBox::Ok, QMessageBox::Ok);
            ui->dumpFirstLineEdit->setFocus();
            return;
        }

        // validate the dump end gl call
        bValidCall = false;
        uint tmpLast = ui->dumpLastLineEdit->text().toUInt(&bValidCall);
        bValidCall = bValidCall && (tmpLast > 0 && tmpLast <= m_maxGlCall);

        if (!bValidCall)
        {
            QMessageBox::warning(this, tr("Invalid Dump Last GL Call"), tr("Please enter a valid gl call number at which to stop the dump."),
                                 QMessageBox::Ok, QMessageBox::Ok);
            ui->dumpLastLineEdit->setFocus();
            return;
        }

        // TODO : Need to validate the file prefix
        /*
        QFile file(ui->dumpFileLineEdit->text());
        if (file.exists())
        {
            QString message = ui->dumpFileLineEdit->text();
            message += " already exits.\nWould you like to overwrite it?";
            int ret = QMessageBox::warning(this, tr("File Already Exists"), tr(message.toStdString().c_str()),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (ret == QMessageBox::No)
            {
                // return so that the user can update the path
                ui->dumpFileLineEdit->setFocus();
                return;
            }
        }*/

        m_first_gl_call = ui->dumpFirstLineEdit->text();
        m_last_gl_call = ui->dumpLastLineEdit->text();
        m_prefix = ui->dumpPrefixLineEdit->text();
        accept();
    }
}

void vogleditor_QDumpDialog::on_buttonBox1_rejected()
{
    reject();
}

void vogleditor_QDumpDialog::on_dumpFirstLineEdit_textChanged(const QString &arg1)
{
    bool bConverted = false;
    uint dumpFirst = arg1.toUInt(&bConverted);

    // make sure the gl call could be converted to a UINT, and that it isn't more than the allowed gl call index
    if (bConverted == false || dumpFirst > m_maxGlCall)
    {
        // turn background red
        QPalette palette(ui->dumpFirstLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::red);
        ui->dumpFirstLineEdit->setPalette(palette);
    }
    else
    {
        // restore background color
        QPalette palette(ui->dumpFirstLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::white);
        ui->dumpFirstLineEdit->setPalette(palette);
    }
}

void vogleditor_QDumpDialog::on_dumpLastLineEdit_textChanged(const QString &arg1)
{
    bool bConverted = false;
    uint dumpLast = arg1.toUInt(&bConverted);

    // make sure the gl call could be converted to a UINT, and that it isn't more than the allowed gl call index
    if (bConverted == false || dumpLast > m_maxGlCall)
    {
        // turn background red
        QPalette palette(ui->dumpLastLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::red);
        ui->dumpLastLineEdit->setPalette(palette);
    }
    else
    {
        // restore background color
        QPalette palette(ui->dumpLastLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::white);
        ui->dumpLastLineEdit->setPalette(palette);
    }
}

void vogleditor_QDumpDialog::on_dumpPrefixLineEdit_textChanged(const QString &arg1)
{
    //TODO : Add some validation code here
    //vogleditor_output_message("Prefix line changed.");
}
