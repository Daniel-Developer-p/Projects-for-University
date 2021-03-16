#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QImage>
#include <QMainWindow>
#include <QPainter>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void choose_colour();
    void set_settings();
    void reset();
    void reset_settings();
private:
    std::unique_ptr<Ui::MainWindow> ui;
};
#endif // MAINWINDOW_H
