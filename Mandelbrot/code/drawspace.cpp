#include "drawspace.h"
#include "draw_worker.h"
#include <algorithm>
#include <complex>
#include <QPainter>
#include <QDebug>
#include <QThread>
#include <QThreadPool>
#include "QtConcurrent/QtConcurrent"
#include <QWheelEvent>
#include <QVector>

drawspace::drawspace(QWidget *parent) :
    QWidget(parent)
  , zoom(DEFAULT_ZOOM)
  , pos(0, 0)
  , mouse_anchor(0, 0)
  , colour(DEFAULT_COLOR)
  , iter_num(DEFAULT_ITER_NUM)
  , color_num(DEFAULT_COLOR_NUM)
  , worker(new Draw_worker(this, this))
{
    connect(worker.get(), &Draw_worker::frame_ready, this, &drawspace::queue_frame);
}

const QColor& drawspace::get_colour() const
{
    return colour;
}

void drawspace::set_colour(const QColor &new_colour)
{
    if (new_colour.isValid())
    {
        colour = new_colour;
    }
}

void drawspace::call_repaint()
{
    redraw_field();
}

int drawspace::get_iter_num() const
{
    return iter_num;
}
int drawspace::get_colour_num() const
{
    return color_num;
}

void drawspace::set_iter_num(int new_iter_num)
{
    if (new_iter_num > 0)
        iter_num = new_iter_num;
}
void drawspace::set_colour_num(int new_colour_num)
{
    if (new_colour_num > 0)
        color_num = new_colour_num;
}

void drawspace::reset_nums()
{
    iter_num = DEFAULT_ITER_NUM;
    color_num = DEFAULT_COLOR_NUM;
}

void drawspace::reset()
{
    reset_nums();
    zoom = DEFAULT_ZOOM;
    colour = DEFAULT_COLOR;
    pos = QPointF(0, 0);
    redraw_field();
}

void drawspace::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.drawImage(0, 0, curr_frame);
}

void drawspace::redraw_field()
{
    emit need_new_frame(pos, width(), height(), zoom, colour, iter_num, color_num);
}

void drawspace::queue_frame(QImage frame)
{
    curr_frame = frame;
    update();
}

void drawspace::resizeEvent(QResizeEvent*)
{
    redraw_field();
}

void drawspace::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        mouse_anchor = event->pos();
    }
}

void drawspace::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        pos -= (event->pos() - mouse_anchor) * zoom;
        mouse_anchor = event->pos();
        redraw_field();
    }
}

void drawspace::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (event->pos() == mouse_anchor)
        {
            mouse_anchor = QPointF(0, 0);
            return;
        }
        pos -= (event->pos() - mouse_anchor) * zoom;
        mouse_anchor = QPointF(0, 0);
        redraw_field();
    }
}

void drawspace::wheelEvent(QWheelEvent* event)
{
    int deg = (event->angleDelta() / 8).y();
    const double mod = 0.9;
    double step = std::pow(mod, deg / 15);
    if (event->modifiers() == Qt::ControlModifier)
    {
        step = (step > 1) ? step * 2 : step / 2;
    }
    zoom *= step;

    redraw_field();
}
