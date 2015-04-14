#include <QWidget>
#include <QtDebug>
#include <QMatrix4x4>
#include <QVector2D>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>

#include "vogleditor_qvertexvisualizer.h"

vogleditor_QVertexVisualizer::vogleditor_QVertexVisualizer(QWidget *parent)
    : QWidget(parent),
      m_xRot(0),
      m_yRot(0),
      m_zRot(0),
      m_draw_mode(GL_TRIANGLES)
{
    setMinimumSize(144,81);
    m_projection=new QMatrix4x4;
}

vogleditor_QVertexVisualizer::~vogleditor_QVertexVisualizer()
{

}

void vogleditor_QVertexVisualizer::setLabel(QString label)
{
    m_label=label;
    update();
}

void vogleditor_QVertexVisualizer::setVertices(QVector<QVector3D> vertices)
{
    m_vertices=vertices;
    float maxValue=0;
    foreach (QVector3D vertex, m_vertices) {
        maxValue=qMax(maxValue, qAbs(vertex.x()));
        maxValue=qMax(maxValue, qAbs(vertex.y()));
        maxValue=qMax(maxValue, qAbs(vertex.z()));
    }
    m_projection->setToIdentity();
    m_projection->ortho(-maxValue,maxValue,-maxValue,maxValue,-maxValue,maxValue);
}

void vogleditor_QVertexVisualizer::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter | Qt::AlignBottom , m_label);
    painter.save();
    int textheight;
    if (isWindow())
        textheight=0;
    else
        textheight = painter.fontMetrics().height();
    float viewPortHeight = height()-textheight;
    painter.scale(viewPortHeight/2,-viewPortHeight/2);
    painter.translate(1.0f*((float)width()/viewPortHeight),-1);
    QPen pen = painter.pen();
    pen.setWidth(0);
    painter.setPen(pen);
//    painter.setRenderHint(QPainter::Antialiasing);

    QMatrix4x4 viewMatrix;
    viewMatrix.setToIdentity();
    viewMatrix.rotate(m_xRot / 16.0f, 1, 0, 0);
    viewMatrix.rotate(m_yRot / 16.0f, 0, 1, 0);
    viewMatrix.rotate(m_zRot / 16.0f, 0, 0, 1);

    QMatrix4x4 VPMatrix = viewMatrix*(*m_projection);

    QVector<QPointF> transformedVertices;
    foreach (QVector3D vertex, m_vertices) {
        transformedVertices.push_back((VPMatrix*vertex).toVector2D().toPointF());
    }
    //TODO Support more drawing modes.
    if (m_draw_mode==GL_TRIANGLES){
        for (int i =0; i<transformedVertices.size()/3; i++)
        {
            int j=i*3;
            painter.drawLine(transformedVertices[j],transformedVertices[j+1]);
            painter.drawLine(transformedVertices[j+1],transformedVertices[j+2]);
            painter.drawLine(transformedVertices[j+2],transformedVertices[j]);
        }
        painter.restore();
    }else if (m_draw_mode==GL_TRIANGLE_STRIP){
        for (int i =0; i<transformedVertices.size()-2; i=i+1)
        {
            painter.drawLine(transformedVertices[i],transformedVertices[i+1]);
            painter.drawLine(transformedVertices[i+2],transformedVertices[i]);
        }
        if(transformedVertices.size()>3)
            painter.drawLine(transformedVertices[transformedVertices.size()-2],transformedVertices[transformedVertices.size()-1]);
        painter.restore();
    }else if (m_draw_mode==GL_TRIANGLE_FAN){
        for (int i=1; i<transformedVertices.size()-1; i++)
        {
            painter.drawLine(transformedVertices[0],transformedVertices[i]);
            painter.drawLine(transformedVertices[i],transformedVertices[i+1]);
            painter.drawLine(transformedVertices[i+1],transformedVertices[0]);
        }
        painter.restore();
    }else if (m_draw_mode==GL_LINE_STRIP)
    {
        for (int i =0; i<transformedVertices.size()-1; i++)
            painter.drawLine(transformedVertices[i],transformedVertices[i+1]);
        painter.restore();
    }else if (m_draw_mode==GL_LINES)
    {
        for (int i =0; i<transformedVertices.size()-1; i=i+2)
            painter.drawLine(transformedVertices[i],transformedVertices[i+1]);
        painter.restore();
    }else if (m_draw_mode==GL_LINE_LOOP || m_draw_mode==GL_POLYGON)
    {
        for (int i =0; i<transformedVertices.size()-1; i++)
            painter.drawLine(transformedVertices[i],transformedVertices[i+1]);
        painter.drawLine(transformedVertices[transformedVertices.size()-1],transformedVertices[0]);
        painter.restore();
    }else if (m_draw_mode==GL_POINTS )
    {
        for (int i =0; i<transformedVertices.size(); i++)
            painter.drawPoint(transformedVertices[i]);
        painter.restore();
    }else if (m_draw_mode==GL_QUADS){
        for (int i =0; i<transformedVertices.size()/4; i++)
        {
            int j=i*4;
            painter.drawLine(transformedVertices[j],transformedVertices[j+1]);
            painter.drawLine(transformedVertices[j+1],transformedVertices[j+2]);
            painter.drawLine(transformedVertices[j+2],transformedVertices[j+3]);
            painter.drawLine(transformedVertices[j+3],transformedVertices[j]);
        }
        painter.restore();
    }else
    {
        painter.restore();
        painter.drawText(QRectF(0, 0, width(), height()), Qt::AlignCenter | Qt::AlignVCenter , "Draw mode\nnot supported\nby visualizer.");
    }
    if (!isWindow())
    {
        painter.setPen(QColor(160,160,160));
        painter.drawLine(width()-1, 0, width()-1, height());
    }
}

void vogleditor_QVertexVisualizer::mousePressEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::RightButton)
        showContextMenu(event->pos());
    else if (event->buttons() == Qt::LeftButton)
        m_lastPos = event->pos();
}

void vogleditor_QVertexVisualizer::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int dx = event->x() - m_lastPos.x();
        int dy = event->y() - m_lastPos.y();
        setXRotation(m_xRot - 8 * dy);
        setYRotation(m_yRot - 8 * dx);
        m_lastPos = event->pos();
    }
}

void vogleditor_QVertexVisualizer::showContextMenu(const QPoint &pos)
{
   QMenu contextMenu(tr("Context Menu"), this);

   QAction popup_action("Show In New Window", this);
   connect(&popup_action, SIGNAL(triggered()), this, SLOT(ShowInWindow()));
   contextMenu.addAction(&popup_action);

   QMenu drawModeMenu(tr("Set Draw Mode"), this);
   QList <QPair <GLenum, QString> > modes;
   modes.append(QPair<GLenum, QString>(GL_POINTS,"GL_POINTS"));
   modes.append(QPair<GLenum, QString>(GL_LINE_STRIP,"GL_LINE_STRIP"));
   modes.append(QPair<GLenum, QString>(GL_LINE_LOOP,"GL_LINE_LOOP"));
   modes.append(QPair<GLenum, QString>(GL_LINES,"GL_LINES"));
   modes.append(QPair<GLenum, QString>(GL_LINE_STRIP_ADJACENCY,"GL_LINE_STRIP_ADJACENCY"));
   modes.append(QPair<GLenum, QString>(GL_LINES_ADJACENCY,"GL_LINES_ADJACENCY"));
   modes.append(QPair<GLenum, QString>(GL_TRIANGLE_STRIP,"GL_TRIANGLE_STRIP"));
   modes.append(QPair<GLenum, QString>(GL_TRIANGLE_FAN,"GL_TRIANGLE_FAN"));
   modes.append(QPair<GLenum, QString>(GL_TRIANGLES,"GL_TRIANGLES"));
   modes.append(QPair<GLenum, QString>(GL_TRIANGLE_STRIP_ADJACENCY,"GL_TRIANGLE_STRIP_ADJACENCY"));
   modes.append(QPair<GLenum, QString>(GL_TRIANGLE_STRIP_ADJACENCY,"GL_TRIANGLES_ADJACENCY"));
   modes.append(QPair<GLenum, QString>(GL_PATCHES,"GL_PATCHES"));
   modes.append(QPair<GLenum, QString>(GL_QUADS,"GL_QUADS"));
   modes.append(QPair<GLenum, QString>(GL_POLYGON,"GL_POLYGON"));
   QActionGroup drawModeGroup(&drawModeMenu);
   drawModeGroup.setExclusive(true);
   QPair<GLenum, QString> mode;
   foreach (mode, modes) {
       QAction *action = new QAction(mode.second, &drawModeMenu);
       drawModeGroup.addAction(action);
       action->setData(mode.first);
       connect(action, SIGNAL(triggered()), this, SLOT(DrawModeMenuSelected()));
       drawModeMenu.addAction(action);
       action->setCheckable(true);
       //At the moment we only supports these modes:
       if (!(mode.first==GL_LINES || mode.first==GL_TRIANGLES ||
             mode.first==GL_LINE_STRIP || mode.first==GL_POINTS ||
             mode.first==GL_LINE_LOOP || mode.first==GL_TRIANGLE_STRIP ||
             mode.first==GL_TRIANGLE_FAN || mode.first==GL_QUADS ||
             mode.first==GL_POLYGON))
           action->setEnabled(false);
       if (mode.first==m_draw_mode)
           action->setChecked(true);
   }
   contextMenu.addMenu(&drawModeMenu);

   QAction reset_action("Reset roataion", this);
   connect(&reset_action, SIGNAL(triggered()), this, SLOT(ResetRotation()));
   contextMenu.addAction(&reset_action);

   contextMenu.exec(mapToGlobal(pos));
}

void vogleditor_QVertexVisualizer::setDrawMode(GLenum draw_mode)
{
    m_draw_mode=draw_mode;
    update();
}

void vogleditor_QVertexVisualizer::DrawModeMenuSelected()
{
    QAction* senderAction = (QAction*)(sender());
    int new_mode = senderAction->data().toInt();
    setDrawMode(new_mode);
}

void vogleditor_QVertexVisualizer::ResetRotation()
{
    m_xRot=0;
    m_yRot=0;
    m_zRot=0;
    update();
}

void vogleditor_QVertexVisualizer::ShowInWindow()
{
    vogleditor_QVertexVisualizer *new_window = new vogleditor_QVertexVisualizer();
    new_window->setDrawMode(m_draw_mode);
    new_window->setWindowTitle(m_label);
    new_window->setVertices(m_vertices);
    new_window->show();
}

void vogleditor_QVertexVisualizer::normalizeAngle(int &angle)
{
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;
}

void vogleditor_QVertexVisualizer::setXRotation(int angle)
{
    normalizeAngle(angle);
    if (angle != m_xRot) {
        m_xRot = angle;
        update();
    }
}

void vogleditor_QVertexVisualizer::setYRotation(int angle)
{
    normalizeAngle(angle);
    if (angle != m_yRot) {
        m_yRot = angle;
        update();
    }
}

void vogleditor_QVertexVisualizer::setZRotation(int angle)
{
    normalizeAngle(angle);
    if (angle != m_zRot) {
        m_zRot = angle;
        update();
    }
}
