#ifndef WORKER_H
#define WORKER_H

#include <atomic>
#include <QThread>
#include <QImage>
#include <QtConcurrent/QtConcurrent>

class Draw_worker;
#include "drawspace.h"

struct v_entry
{
    unsigned char* segment_ptr;
    int from_h, to_h;
};

class Draw_worker : public QThread
{
    Q_OBJECT

    struct args
    {
        int w, h, max_iter_num, max_color_num;
        double zoom;
        QPointF frame_center;
        QColor color;
    };
public:
    explicit Draw_worker(QObject *parent = nullptr);
    explicit Draw_worker(drawspace* ptr, QObject* parent = nullptr);
    ~Draw_worker();

    void process(v_entry &entry, int bits_per_line, args draw_args);
    void process_jackal(v_entry &entry, int bits_per_line, args draw_args);
    virtual void run() override;
private:
    QFuture<void> curr_state;
    QMutex m;
    QWaitCondition start_cond;
    enum class thread_mods { NO_CHANGE, RESTART, END };
    thread_mods mods;

    args draw_args;
    QImage images[2];
    std::atomic_size_t curr_image;

    void fill_bit_field(bool is_jackal, unsigned char* bit_field, const std::size_t per_line, int from_h, int to_h, args draw_args);
    double count_value(QPointF frame_center, int pos_x, int pos_y, int window_w, int window_h, double zoom, int max_iter_num, int max_color_num);
public slots:
    void work_again(QPointF pos, int w, int h, double zoom, QColor colour, int iter_num, int color_num);

signals:
    void frame_ready(QImage frame);
};

#endif // WORKER_H
