#ifndef VOGLEDITOR_STATETREECONTEXTINFOITEM_H
#define VOGLEDITOR_STATETREECONTEXTINFOITEM_H

#include "vogleditor_statetreeitem.h"

class vogleditor_stateTreeContextInfoDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextInfoDiffableItem(QString name, QString value, vogleditor_stateTreeItem *parent)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_context_info *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const = 0;

protected:
    const vogl_context_info *m_pDiffBaseState;
};

class vogleditor_stateTreeContextInfoBoolItem : public vogleditor_stateTreeContextInfoDiffableItem
{
public:
    vogleditor_stateTreeContextInfoBoolItem(QString name, bool (vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeContextInfoBoolItem()
    {
        m_pState = NULL;
    }

    const vogl_context_info *get_context_info() const
    {
        return m_pState;
    }
    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_context_info *m_pState;
    bool (vogl_context_info::*m_pFunc)(void) const;
};

class vogleditor_stateTreeContextInfoUIntItem : public vogleditor_stateTreeContextInfoDiffableItem
{
public:
    vogleditor_stateTreeContextInfoUIntItem(QString name, uint (vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeContextInfoUIntItem()
    {
        m_pState = NULL;
    }

    const vogl_context_info *get_context_info() const
    {
        return m_pState;
    }
    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_context_info *m_pState;
    uint (vogl_context_info::*m_pFunc)(void) const;
};

class vogleditor_stateTreeContextInfoStringItem : public vogleditor_stateTreeContextInfoDiffableItem
{
public:
    vogleditor_stateTreeContextInfoStringItem(QString name, const vogl::dynamic_string &(vogl_context_info::*func)(void) const, vogleditor_stateTreeItem *parent, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeContextInfoStringItem()
    {
        m_pState = NULL;
    }

    const vogl_context_info *get_context_info() const
    {
        return m_pState;
    }
    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    const vogl_context_info *m_pState;
    dynamic_string m_value;
    const vogl::dynamic_string &(vogl_context_info::*m_pFunc)(void) const;
};

class vogleditor_stateTreeContextInfoExtensionItem : public vogleditor_stateTreeContextInfoDiffableItem
{
public:
    vogleditor_stateTreeContextInfoExtensionItem(QString name, const vogl::dynamic_string extensionName, vogleditor_stateTreeItem *parent, const vogl_context_info &info)
        : vogleditor_stateTreeContextInfoDiffableItem(name, QString(extensionName.c_str()), parent),
          m_pState(&info),
          m_extensionName(extensionName)
    {
        setValue(QString(extensionName.c_str()));
    }

    virtual ~vogleditor_stateTreeContextInfoExtensionItem()
    {
        m_pState = NULL;
    }

    const vogl_context_info *get_context_info() const
    {
        return m_pState;
    }
    virtual bool hasChanged() const
    {
        if (m_pDiffBaseState == NULL)
        {
            return false;
        }

        // if the current extension is in the base list, then don't flag this as having changed
        const dynamic_string_array &baseExtensions = m_pDiffBaseState->get_extensions();
        for (const vogl::dynamic_string *iter = baseExtensions.begin(); iter != baseExtensions.end(); iter++)
        {
            if ((*iter) == m_extensionName)
            {
                return false;
            }
        }

        // if we've made it through the entire list, then this extension was not in the base context, so it is new (has changed).
        return true;
    }

    // if an extension shows up as having changed, that means it didn't exist in the previous snapshot
    virtual QString getDiffedValue() const
    {
        return "non-existent";
    }

private:
    const vogl_context_info *m_pState;
    dynamic_string m_extensionName;
};

class vogleditor_stateTreeContextInfoItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeContextInfoItem(QString name, QString value, vogleditor_stateTreeItem *parent, const vogl_context_info &info);
    virtual ~vogleditor_stateTreeContextInfoItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    const vogl_context_info *get_context_info() const
    {
        return m_pState;
    }

    void set_diff_base_state(const vogl_context_info *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeContextInfoDiffableItem **iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }
    virtual bool hasChanged() const;

private:
    const vogl_context_info *m_pState;
    const vogl_context_info *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeContextInfoDiffableItem *> m_diffableItems;
};

#endif // VOGLEDITOR_STATETREECONTEXTINFOITEM_H
