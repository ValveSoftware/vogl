#ifndef VOGLEDITOR_QSNAPSHOTOVERLAYWIDGET_H
#define VOGLEDITOR_QSNAPSHOTOVERLAYWIDGET_H

#include <QResizeEvent>
#include <QPushButton>
#include <QGridLayout>
#include <QWidget>
#include <QPainter>

// based on implementation from: http://stackoverflow.com/questions/19383427/blur-effect-over-a-qwidget-in-qt

#ifndef Q_DECL_OVERRIDE
#define Q_DECL_OVERRIDE override
#endif

class vogleditor_OverlayWidget : public QWidget
{
    Q_OBJECT

    void newParent()
    {
        if (parent() == NULL)
        {
            return;
        }
        parent()->installEventFilter(this);
        raise();
    }

public:
    explicit vogleditor_OverlayWidget(QWidget *parent = 0)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        newParent();
    }

protected:
    //! Catches resize and child events from the parent widget
    bool eventFilter(QObject *obj, QEvent *ev) Q_DECL_OVERRIDE
    {
        if (obj == parent())
        {
            if (ev->type() == QEvent::Resize)
            {
                QResizeEvent *rev = static_cast<QResizeEvent *>(ev);
                this->resize(rev->size());
            }
            else if (ev->type() == QEvent::ChildAdded)
            {
                raise();
            }
        }
        return QWidget::eventFilter(obj, ev);
    }

    //! Tracks parent widget changes
    bool event(QEvent *ev) Q_DECL_OVERRIDE
    {
        if (ev->type() == QEvent::ParentAboutToChange)
        {
            if (parent())
                parent()->removeEventFilter(this);
        }
        else if (ev->type() == QEvent::ParentChange)
        {
            newParent();
        }
        return QWidget::event(ev);
    }
};

class vogleditor_QSnapshotOverlayWidget : public vogleditor_OverlayWidget
{
    Q_OBJECT

public:
    vogleditor_QSnapshotOverlayWidget(QWidget *parent = 0);

    ~vogleditor_QSnapshotOverlayWidget()
    {
        if (m_pTakeSnapshotButton != NULL)
        {
            delete m_pTakeSnapshotButton;
            m_pTakeSnapshotButton = NULL;
        }
    }

    void showButton(bool bShow)
    {
        m_pTakeSnapshotButton->setVisible(bShow);
    }

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(100, 100, 100, 128));
    }

private
slots:
    void slot_takeSnapshotButtonClicked()
    {
        emit takeSnapshotButtonClicked();
    }

signals:
    void takeSnapshotButtonClicked();

private:
    QPushButton *m_pTakeSnapshotButton;
};

#endif // VOGLEDITOR_QSNAPSHOTOVERLAYWIDGET_H
