#ifndef VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H
#define VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_sso_state;

class vogleditor_stateTreeProgramPipelineDiffableItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramPipelineDiffableItem(QString name, QString value, vogleditor_stateTreeItem* parent)
        : vogleditor_stateTreeItem(name, value, parent),
          m_pDiffBaseState(NULL)
    {
    }

    void set_diff_base_state(const vogl_sso_state* pBaseState) {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const = 0;
protected:
    const vogl_sso_state* m_pDiffBaseState;
};

class vogleditor_stateTreeProgramPipelineIntItem : public vogleditor_stateTreeProgramPipelineDiffableItem
{
public:
    vogleditor_stateTreeProgramPipelineIntItem(QString name, GLenum enumName, vogleditor_stateTreeItem* parent, const vogl_sso_state& state);
    virtual ~vogleditor_stateTreeProgramPipelineIntItem() { m_pState = NULL; }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum m_enum;
    const vogl_sso_state* m_pState;
};

class vogleditor_stateTreeProgramPipelineEnumItem : public vogleditor_stateTreeProgramPipelineDiffableItem
{
public:
    vogleditor_stateTreeProgramPipelineEnumItem(QString name, GLenum enumName, vogleditor_stateTreeItem* parent, const vogl_sso_state& state);
    virtual ~vogleditor_stateTreeProgramPipelineEnumItem() { m_pState = NULL; }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    GLenum m_enum;
    const vogl_sso_state* m_pState;
};

class vogleditor_stateTreeProgramPipelineItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeProgramPipelineItem(QString name, GLuint64 handle, vogleditor_stateTreeItem* parent, const vogl_sso_state& state);
    virtual ~vogleditor_stateTreeProgramPipelineItem()
    {
        m_pState = NULL; m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_sso_state* pBaseState)
    {
        m_pDiffBaseState = pBaseState;
        for (vogleditor_stateTreeProgramPipelineDiffableItem** iter = m_diffableItems.begin(); iter != m_diffableItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

    GLuint64 get_handle() const { return m_handle; }

 private:
    GLuint64 m_handle;
    const vogl_sso_state* m_pState;
    const vogl_sso_state* m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeProgramPipelineDiffableItem*> m_diffableItems;
};
#endif // VOGLEDITOR_STATETREEPROGRAMPIPELINEITEM_H
