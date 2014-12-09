#include "vogleditor_qsnapshotoverlaywidget.h"

vogleditor_QSnapshotOverlayWidget::vogleditor_QSnapshotOverlayWidget(QWidget *parent)
    : vogleditor_OverlayWidget(parent),
      m_pTakeSnapshotButton()
{
    setAttribute(Qt::WA_TranslucentBackground);

    m_pTakeSnapshotButton = new QPushButton(this);
    m_pTakeSnapshotButton->setText("Click here\nto snapshot state");
    m_pTakeSnapshotButton->setMaximumHeight(200);
    m_pTakeSnapshotButton->setMaximumWidth(200);

    QGridLayout *pGridLayout = new QGridLayout(this);
    pGridLayout->addWidget(m_pTakeSnapshotButton, 1, 1);
    this->setLayout(pGridLayout);

    connect(m_pTakeSnapshotButton, SIGNAL(clicked()), this, SLOT(slot_takeSnapshotButtonClicked()));
}
