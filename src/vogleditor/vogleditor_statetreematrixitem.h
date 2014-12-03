#ifndef VOGLEDITOR_STATETREEMATRIXITEM_H
#define VOGLEDITOR_STATETREEMATRIXITEM_H

#include "vogleditor_statetreeitem.h"
#include "vogl_matrix_state.h"

class vogleditor_stateTreeMatrixRowItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeMatrixRowItem(QString name, unsigned int rowIndex, vogleditor_stateTreeItem *parent, const matrix44D &matrix);

    virtual ~vogleditor_stateTreeMatrixRowItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const matrix44D *pBaseState)
    {
        m_pDiffBaseState = pBaseState;
    }

    virtual bool hasChanged() const;
    virtual QString getDiffedValue() const;

private:
    unsigned int m_rowIndex;
    const matrix44D *m_pState;
    const matrix44D *m_pDiffBaseState;
};

class vogleditor_stateTreeMatrixItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeMatrixItem(QString name, vogleditor_stateTreeItem *parent, const matrix44D &matrix, unsigned int stackIndex);

    virtual ~vogleditor_stateTreeMatrixItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const matrix44D *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreeMatrixRowItem **iter = m_rowItems.begin(); iter != m_rowItems.end(); iter++)
        {
            (*iter)->set_diff_base_state(pBaseState);
        }
    }

    unsigned int get_stack_index() const
    {
        return m_stackIndex;
    }

private:
    unsigned int m_stackIndex;
    const matrix44D *m_pState;
    const matrix44D *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeMatrixRowItem *> m_rowItems;
};

class vogleditor_stateTreeMatrixStackItem : public vogleditor_stateTreeItem
{
public:
    vogleditor_stateTreeMatrixStackItem(QString name, GLenum target, unsigned int index, vogleditor_stateTreeItem *parent, const vogl_matrix_state &state);

    virtual ~vogleditor_stateTreeMatrixStackItem()
    {
        m_pState = NULL;
        m_pDiffBaseState = NULL;
    }

    void set_diff_base_state(const vogl_matrix_state *pBaseState)
    {
        m_pDiffBaseState = pBaseState;

        for (vogleditor_stateTreeMatrixItem **iter = m_matrixItems.begin(); iter != m_matrixItems.end(); iter++)
        {
            const matrix44D *pMatrix = m_pDiffBaseState->get_matrix(m_target, m_index, (*iter)->get_stack_index());
            (*iter)->set_diff_base_state(pMatrix);
        }
    }

    GLenum get_target() const
    {
        return m_target;
    }

private:
    GLenum m_target;
    unsigned int m_index;
    const vogl_matrix_state *m_pState;
    const vogl_matrix_state *m_pDiffBaseState;
    vogl::vector<vogleditor_stateTreeMatrixItem *> m_matrixItems;
};

#endif // VOGLEDITOR_STATETREEMATRIXITEM_H
