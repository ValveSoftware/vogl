#include "vogleditor_output.h"
#include <QTextEdit>

vogleditor_output::vogleditor_output()
{
}

vogleditor_output::~vogleditor_output()
{
}

void vogleditor_output::message(const char* pMessage)
{
    if (m_pTextEdit != NULL)
    {
        m_pTextEdit->append(pMessage);
    }
}

void vogleditor_output::warning(const char* pWarning)
{
    if (m_pTextEdit != NULL)
    {
        m_pTextEdit->append("Warning: ");
        m_pTextEdit->append(pWarning);
    }
}


void vogleditor_output::error(const char* pError)
{
    if (m_pTextEdit != NULL)
    {
        m_pTextEdit->append("ERROR: ");
        m_pTextEdit->append(pError);
    }
}
