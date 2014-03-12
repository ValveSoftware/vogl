#ifndef VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H
#define VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H

#include "vogleditor_statetreeitem.h"

class vogl_arb_program_environment_state;

class vogleditor_stateTreeArbProgramEnvItem : public vogleditor_stateTreeItem
{
public:
   vogleditor_stateTreeArbProgramEnvItem(QString name, unsigned int index, vogleditor_stateTreeItem* parent, vogl_arb_program_environment_state& state);
   virtual ~vogleditor_stateTreeArbProgramEnvItem() { m_pState = NULL; m_pDiffBaseState = NULL; }

   vogl_arb_program_environment_state* get_current_state() const { return m_pState; }
   const vogl_arb_program_environment_state* get_base_state() const { return m_pDiffBaseState; }

   virtual void set_diff_base_state(const vogl_arb_program_environment_state* pBaseState)
   {
       m_pDiffBaseState = pBaseState;
   }

   virtual bool hasChanged() const;

private:
   unsigned int m_index;
   vogl_arb_program_environment_state* m_pState;
   const vogl_arb_program_environment_state* m_pDiffBaseState;
};

#endif // VOGLEDITOR_STATETREEARBPROGRAMENVITEM_H
