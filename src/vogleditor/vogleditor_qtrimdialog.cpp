#include "vogleditor_qtrimdialog.h"
#include "ui_vogleditor_qtrimdialog.h"
#include <QFileDialog>
#include <QMessageBox>

vogleditor_QTrimDialog::vogleditor_QTrimDialog(QString parentTraceFile, uint maxFrameIndex, uint maxTrimLength, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::vogleditor_QTrimDialog),
      m_maxFrameIndex(maxFrameIndex),
      m_maxTrimLength(maxTrimLength),
      m_trim_frame("0"),
      m_trim_len("1")
{
    ui->setupUi(this);
    ui->trimFrameLineEdit->setText(m_trim_frame);
    ui->trimLenLineEdit->setText(m_trim_len);

    QString trimFilename = parentTraceFile;
    trimFilename.insert(trimFilename.lastIndexOf("."), "-trim");
    ui->trimFileLineEdit->setText(trimFilename);
}

vogleditor_QTrimDialog::~vogleditor_QTrimDialog()
{
    delete ui;
}

void vogleditor_QTrimDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (button == ui->buttonBox->button(QDialogButtonBox::Ok))
    {
        // validate the trim start frame
        bool bValidFrame = false;
        uint tmpFrame = ui->trimFrameLineEdit->text().toUInt(&bValidFrame);
        bValidFrame = bValidFrame && (tmpFrame <= m_maxFrameIndex);

        if (!bValidFrame)
        {
            QMessageBox::warning(this, tr("Invalid Trim Frame"), tr("Please enter a valid frame number at which to start the trim."),
                                 QMessageBox::Ok, QMessageBox::Ok);
            ui->trimFrameLineEdit->setFocus();
            return;
        }

        // validate the trim length
        bool bValidLen = false;
        uint tmpLen = ui->trimLenLineEdit->text().toUInt(&bValidLen);
        bValidLen = bValidLen && (tmpLen > 0 && tmpLen <= m_maxTrimLength);

        if (!bValidLen)
        {
            QMessageBox::warning(this, tr("Invalid Trim Count"), tr("Please enter a valid nubmer of frames to trim."),
                                 QMessageBox::Ok, QMessageBox::Ok);
            ui->trimLenLineEdit->setFocus();
            return;
        }

        // validate the filename
        QFile file(ui->trimFileLineEdit->text());
        if (file.exists())
        {
            QString message = ui->trimFileLineEdit->text();
            message += " already exits.\nWould you like to overwrite it?";
            int ret = QMessageBox::warning(this, tr("File Already Exists"), tr(message.toStdString().c_str()),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (ret == QMessageBox::No)
            {
                // return so that the user can update the path
                ui->trimFileLineEdit->setFocus();
                return;
            }
        }

        m_trim_frame = ui->trimFrameLineEdit->text();
        m_trim_len = ui->trimLenLineEdit->text();
        m_trim_file = ui->trimFileLineEdit->text();
        accept();
    }
}

void vogleditor_QTrimDialog::on_buttonBox_rejected()
{
    reject();
}

void vogleditor_QTrimDialog::on_pickTrimFileButton_pressed()
{
    // open file dialog
    QString suggestedName = ui->trimFileLineEdit->text();
    QString selectedName = QFileDialog::getSaveFileName(this, tr("Save Trim File"), suggestedName, tr("Trace file (*.bin)"));

    if (!selectedName.isEmpty())
    {
        ui->trimFileLineEdit->setText(selectedName);
    }
}

void vogleditor_QTrimDialog::on_trimLenLineEdit_textChanged(const QString &arg1)
{
    bool bConverted = false;
    uint trimLen = arg1.toUInt(&bConverted);

    // make sure the length could be converted to a UINT, and that it isn't more than the allowed length
    if (bConverted == false || trimLen > m_maxTrimLength)
    {
        // turn background red
        QPalette palette(ui->trimLenLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::red);
        ui->trimLenLineEdit->setPalette(palette);
    }
    else
    {
        // restore background color
        QPalette palette(ui->trimLenLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::white);
        ui->trimLenLineEdit->setPalette(palette);
    }
}

void vogleditor_QTrimDialog::on_trimFrameLineEdit_textChanged(const QString &arg1)
{
    bool bConverted = false;
    uint trimFrame = arg1.toUInt(&bConverted);

    // make sure frame could be converted to a UINT, and that it isn't greater than the number of frames in the trace
    if (bConverted == false || trimFrame > m_maxFrameIndex)
    {
        // turn background red
        QPalette palette(ui->trimFrameLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::red);
        ui->trimFrameLineEdit->setPalette(palette);
    }
    else
    {
        // restore background color
        QPalette palette(ui->trimFrameLineEdit->palette());
        palette.setColor(QPalette::Base, Qt::white);
        ui->trimFrameLineEdit->setPalette(palette);
    }
}
