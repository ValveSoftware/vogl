#ifndef VOGLEDITOR_QVERTEXVISUALIZEREXPLORER_H
#define VOGLEDITOR_QVERTEXVISUALIZEREXPLORER_H

#include <QWidget>
#include "vogl_core.h"
#include "gl_types.h"
#include "vogleditor_output.h"

#include <QVector>
#include <QVector3D>

class QMatrix4x4;

class vogleditor_QVertexVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit vogleditor_QVertexVisualizer(QWidget *parent = 0);
    ~vogleditor_QVertexVisualizer();
    void setLabel(QString label);
    void setVertices(QVector<QVector3D> vertices);
    static void normalizeAngle(int &angle);
protected:
    void paintEvent ( QPaintEvent * event );
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
private:
    QString m_label;
    QVector<QVector3D> m_vertices;
    QMatrix4x4 *m_projection;
    int m_xRot;
    int m_yRot;
    int m_zRot;
    QPoint m_lastPos;
    GLenum m_draw_mode;
    void setXRotation(int angle);
    void setYRotation(int angle);
    void setZRotation(int angle);
    void showContextMenu(const QPoint &pos);
public slots:
    void setDrawMode(GLenum draw_mode);
    void ShowInWindow();
    void ResetRotation();
private slots:
    void DrawModeMenuSelected();
};

#endif // VOGLEDITOR_QVERTEXVISUALIZEREXPLORER_H
