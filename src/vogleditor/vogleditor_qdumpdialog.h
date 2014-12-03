#ifndef VOGLEDITOR_QDUMPDIALOG_H
#define VOGLEDITOR_QDUMPDIALOG_H

#include <QDialog>
#include <QString>

class QAbstractButton;

namespace Ui
{
    class vogleditor_QDumpDialog;
}

class vogleditor_QDumpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit vogleditor_QDumpDialog(QString parentTraceFile, QWidget *parent = 0);
    ~vogleditor_QDumpDialog();

    QString dump_first_gl_call()
    {
        return m_first_gl_call;
    }

    QString dump_last_gl_call()
    {
        return m_last_gl_call;
    }

    QString dump_prefix()
    {
        return m_prefix;
    }

private
slots:
    void on_buttonBox1_clicked(QAbstractButton *button);

    void on_buttonBox1_rejected();

    void on_dumpFirstLineEdit_textChanged(const QString &arg1);

    void on_dumpLastLineEdit_textChanged(const QString &arg1);

    void on_dumpPrefixLineEdit_textChanged(const QString &arg1);

private:
    Ui::vogleditor_QDumpDialog *ui;
    uint m_minGlCall;
    uint m_maxGlCall;
    QString m_first_gl_call;
    QString m_last_gl_call;
    QString m_prefix;
};

#endif // VOGLEDITOR_QDUMPDIALOG_H
