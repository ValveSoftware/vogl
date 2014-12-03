#include "vogleditor_output.h"
#include <QTextEdit>

vogleditor_output gs_OUTPUT;

vogleditor_output::vogleditor_output()
{
}

vogleditor_output::~vogleditor_output()
{
}

void vogleditor_output::message(const char *pMessage)
{
    if (m_pTextEdit != NULL)
    {
        m_pTextEdit->append(pMessage);
    }
}

void vogleditor_output::warning(const char *pWarning)
{
    if (m_pTextEdit != NULL)
    {
        QString msg = QString("Warning: %1").arg(pWarning);
        m_pTextEdit->append(msg);
    }
}

void vogleditor_output::error(const char *pError)
{
    if (m_pTextEdit != NULL)
    {
        QString msg = QString("ERROR: %1").arg(pError);
        m_pTextEdit->append(msg);
    }
}
