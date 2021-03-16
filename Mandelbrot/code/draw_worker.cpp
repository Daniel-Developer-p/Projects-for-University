#include "draw_worker.h"
#include "drawspace.h"
#include <cassert>
#include <complex>
#include <functional>
#include <QThreadPool>

Draw_worker::Draw_worker(QObject* parent) : QThread(parent)
{}

Draw_worker::Draw_worker(drawspace* ptr, QObject* parent)
    : QThread(parent)
    , curr_state()
    , mods(thread_mods::NO_CHANGE)
    , images{QImage(ptr->width(), ptr->height(), QImage::Format_RGB888),
             QImage(ptr->width(), ptr->height(), QImage::Format_RGB888)}
    , curr_image(0)
{
    connect(ptr, &drawspace::need_new_frame, this, &Draw_worker::work_again);
}

Draw_worker::~Draw_worker()
{
    {
        QMutexLocker lock(&m);
        mods = thread_mods::END;
        curr_state.cancel();
        start_cond.wakeOne();
    }

    wait();
}

void Draw_worker::work_again(QPointF frame_center, int w, int h, double z, QColor colour, int iter_num, int color_num)
{
    QMutexLocker lock(&m);
    draw_args.frame_center = frame_center;
    draw_args.w = w;
    draw_args.h = h;
    draw_args.zoom = z;
    draw_args.max_iter_num = iter_num;
    draw_args.max_color_num = color_num;
    draw_args.color = colour;
    if (isRunning())
    {
        mods = thread_mods::RESTART;
        start_cond.wakeOne();
    }
    else
    {
        start();
    }
}

void Draw_worker::run()
{
    forever
    {
        do
        {
            int w, h;
            args frame_args;
            {
                QMutexLocker locker(&m);
                w = draw_args.w;
                h = draw_args.h;
                frame_args = draw_args;
            }
            if (images[curr_image].width() != w || images[curr_image].height() != h)
                images[curr_image] = images[curr_image].scaled(w, h);
            QImage& jackal = images[curr_image];
            curr_image = (curr_image + 1) % 2;

            QVector<v_entry> segments;
            int th_count = QThreadPool::globalInstance()->maxThreadCount() - QThreadPool::globalInstance()->activeThreadCount();
            if (th_count < 1)
                th_count = 1;

            const int segment_h = h / th_count;
            const int per_line = jackal.bytesPerLine();
            for (int i = 0; i < th_count; i++)
            {
                unsigned char* segment = jackal.bits() + i * per_line * segment_h;
                if (i < th_count - 1)
                    segments.append(v_entry{segment, i * segment_h, (i + 1) * segment_h});
                else
                    segments.append(v_entry{segment, i * segment_h, h});
            }
            curr_state = QtConcurrent::map(segments, [this, per_line, frame_args](v_entry& entry)
                                           { process_jackal(entry, per_line, frame_args); });
            curr_state.waitForFinished();
            emit frame_ready(jackal);

            bool restart_flag = false;
            switch (mods)
            {
            case thread_mods::END: return;
            case thread_mods::RESTART: restart_flag = true; break;
            case thread_mods::NO_CHANGE: break;
            default: assert((false) && "No such state expected");
            }
            if (restart_flag)
                break;

            if (images[curr_image].width() != w || images[curr_image].height() != h)
                images[curr_image] = images[curr_image].scaled(w, h);
            QImage& normal = images[curr_image];
            curr_image = (curr_image + 1) % 2;
            for (int i = 0; i < th_count; i++)
            {
                unsigned char* segment = normal.bits() + i * per_line * segment_h;
                segments[i].segment_ptr = segment;
            }
            curr_state = QtConcurrent::map(segments, [this, per_line, frame_args](v_entry& entry)
                                           { process(entry, per_line, frame_args); });

            while(!curr_state.isFinished())
            {
                if (mods != thread_mods::NO_CHANGE)
                {
                    curr_state.cancel();
                    curr_state.waitForFinished();
                    break;
                }
            }

            restart_flag = false;
            switch (mods)
            {
            case thread_mods::END: return;
            case thread_mods::RESTART: restart_flag = true; break;
            case thread_mods::NO_CHANGE: break;
            default: assert((false) && "No such state expected");
            }
            if (restart_flag)
                break;

            emit frame_ready(normal);
        } while(false);

        QMutexLocker lock(&m);
        if (mods != thread_mods::RESTART)
            start_cond.wait(&m);

        switch(mods)
        {
        case thread_mods::RESTART: mods = thread_mods::NO_CHANGE; break;
        case thread_mods::END: return;
        case thread_mods::NO_CHANGE: assert((false) && "Didn't expect NO_CHANGE state here");
        default: assert((false) && "No such state expected");
        }
    }
}

void Draw_worker::process_jackal(v_entry &entry, int bits_per_line, args frame_args)
{
    fill_bit_field(true, entry.segment_ptr, bits_per_line, entry.from_h, entry.to_h, frame_args);
}
void Draw_worker::process(v_entry& entry, int bits_per_line, args frame_args)
{
    fill_bit_field(false, entry.segment_ptr, bits_per_line, entry.from_h, entry.to_h, frame_args);
}

void Draw_worker::fill_bit_field(bool is_jackal, unsigned char* bit_field, const std::size_t per_line, int from_h, int to_h, args frame_args)
{
    int step;
    int w = frame_args.w;
    QColor colour = frame_args.color;
    step = is_jackal ? (w / 40) : 1;

    for (int y = from_h; y < to_h; y++)
    {
        if (!is_jackal && mods != thread_mods::NO_CHANGE)
            return;

        unsigned char* bit_line = bit_field + per_line * (y - from_h);
        for (int x = 0; x < w; x += step)
        {
            double val = count_value(frame_args.frame_center, x, y, w, frame_args.h,
                                     frame_args.zoom, frame_args.max_iter_num, frame_args.max_color_num);
            for (int i = 0; i < step && (x + i < w); i++)
            {
                *bit_line++ = val * colour.red();
                *bit_line++ = val * colour.green();
                *bit_line++ = val * colour.blue();
            }
        }
    }
}

double Draw_worker::count_value(QPointF frame_center, int pos_x, int pos_y, int window_w, int window_h, double zoom, int max_iter_num, int max_color_num)
{
    std::complex<double> c(pos_x - window_w / 2.0, pos_y - window_h / 2.0);
    std::complex<double> offset(frame_center.x(), frame_center.y());

    c *= zoom;
    c += offset;
    std::complex<double> z = 0.0;
    for (int iter = 0; iter < max_iter_num; iter++)
    {
        if (std::norm(z) >= 4.0)
        {
            return static_cast<double>(iter % (max_color_num + 1)) / max_color_num;
        }
        z = z * z + c;
    }
    return 0;
}
