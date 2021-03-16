#ifndef DRAWSPACE_H
#define DRAWSPACE_H

#include <memory>
#include <QWidget>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QtConcurrent/QtConcurrent>

class drawspace;
#include "draw_worker.h"

class drawspace : public QWidget
{
    Q_OBJECT
public:
    explicit drawspace(QWidget *parent = nullptr);
    void call_repaint();
    const QColor& get_colour() const;
    int get_iter_num() const;
    int get_colour_num() const;
    void set_colour(const QColor& colour);
    void set_iter_num(int iter_num);
    void set_colour_num(int colour_num);
    void reset();
    void reset_nums();
private:
    virtual void paintEvent (QPaintEvent* event) override;
    virtual void wheelEvent (QWheelEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

    double count_value(int pos_x, int pos_y, int window_w, int window_h) const;
    void redraw_field();
    constexpr static std::size_t DEFAULT_COLOR_NUM = 50;
    constexpr static std::size_t DEFAULT_ITER_NUM = 100;
    constexpr static double DEFAULT_ZOOM = 0.005;
    constexpr static QColor DEFAULT_COLOR = QColor(127, 127, 255);
    double zoom;
    QPointF pos;
    QPointF mouse_anchor;
    QColor colour;
    int iter_num;
    int color_num;
    QImage curr_frame;
    std::unique_ptr<Draw_worker> worker;

public slots:
    void queue_frame(QImage frame);
signals:
    void need_new_frame(QPointF pos, int w, int h, double zoom, QColor colour, int iter_num, int color_num);
};

#endif // DRAWSPACE_H
