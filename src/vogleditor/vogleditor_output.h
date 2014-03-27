#ifndef VOGLEDITOR_OUTPUT_H
#define VOGLEDITOR_OUTPUT_H

class QTextEdit;

class vogleditor_output
{
public:
    vogleditor_output();
    ~vogleditor_output();

    void init(QTextEdit* pTextEdit) { m_pTextEdit = pTextEdit; }

    void message(const char* pMessage);
    void warning(const char* pWarning);
    void error(const char* pError);

private:
    QTextEdit* m_pTextEdit;
};

extern vogleditor_output gs_OUTPUT;

static void vogleditor_output_init(QTextEdit* pTextEdit) { gs_OUTPUT.init(pTextEdit); }
static void vogleditor_output_message(const char* pMessage) { gs_OUTPUT.message(pMessage); }
static void vogleditor_output_warning(const char* pWarning) { gs_OUTPUT.warning(pWarning); }
static void vogleditor_output_error(const char* pError) { gs_OUTPUT.error(pError); }
static void vogleditor_output_deinit() { gs_OUTPUT.init(0); }

#endif // VOGLEDITOR_OUTPUT_H
