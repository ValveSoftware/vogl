#ifndef VOGLEDITOR_QTRIMDIALOG_H
#define VOGLEDITOR_QTRIMDIALOG_H

#include <QDialog>
#include <QString>

class QAbstractButton;

namespace Ui
{
    class vogleditor_QTrimDialog;
}

class vogleditor_QTrimDialog : public QDialog
{
    Q_OBJECT

public:
    explicit vogleditor_QTrimDialog(QString parentTraceFile, uint maxFrameIndex, uint maxTrimLength, QWidget *parent = 0);
    ~vogleditor_QTrimDialog();

    QString trim_frame()
    {
        return m_trim_frame;
    }

    QString trim_len()
    {
        return m_trim_len;
    }

    QString trim_file()
    {
        return m_trim_file;
    }

private
slots:
    void on_buttonBox_clicked(QAbstractButton *button);

    void on_buttonBox_rejected();

    void on_pickTrimFileButton_pressed();

    void on_trimLenLineEdit_textChanged(const QString &arg1);

    void on_trimFrameLineEdit_textChanged(const QString &arg1);

private:
    Ui::vogleditor_QTrimDialog *ui;
    uint m_maxFrameIndex;
    uint m_maxTrimLength;
    QString m_trim_frame;
    QString m_trim_len;
    QString m_trim_file;
};

#endif // VOGLEDITOR_QTRIMDIALOG_H
