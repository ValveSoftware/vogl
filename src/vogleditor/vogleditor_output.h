#ifndef VOGLEDITOR_OUTPUT_H
#define VOGLEDITOR_OUTPUT_H

class QTextEdit;

class vogleditor_output
{
public:
    vogleditor_output();
    ~vogleditor_output();

    void init(QTextEdit *pTextEdit)
    {
        m_pTextEdit = pTextEdit;
    }

    void message(const char *pMessage);
    void warning(const char *pWarning);
    void error(const char *pError);

private:
    QTextEdit *m_pTextEdit;
};

extern vogleditor_output gs_OUTPUT;

inline void vogleditor_output_init(QTextEdit *pTextEdit)
{
    gs_OUTPUT.init(pTextEdit);
}
inline void vogleditor_output_message(const char *pMessage)
{
    gs_OUTPUT.message(pMessage);
}
inline void vogleditor_output_warning(const char *pWarning)
{
    gs_OUTPUT.warning(pWarning);
}
inline void vogleditor_output_error(const char *pError)
{
    gs_OUTPUT.error(pError);
}
inline void vogleditor_output_deinit()
{
    gs_OUTPUT.init(0);
}

#endif // VOGLEDITOR_OUTPUT_H
