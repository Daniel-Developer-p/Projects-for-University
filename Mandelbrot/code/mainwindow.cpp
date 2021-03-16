#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QColorDialog>
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Mandelbrot");
    resize(800, 600);
    ui->colour_box->setValue(ui->space->get_colour_num());
    ui->iter_box->setValue(ui->space->get_iter_num());
}

void MainWindow::choose_colour()
{
    QColor colour = QColorDialog::getColor(ui->space->get_colour(), this);
    if (colour.isValid())
    {
        ui->space->set_colour(colour);
        ui->space->call_repaint();
    }
}

void MainWindow::set_settings()
{
    int new_iter = ui->iter_box->value();
    int new_colour = ui->colour_box->value();

    ui->space->set_iter_num(new_iter);
    ui->space->set_colour_num(new_colour);
    ui->space->call_repaint();
}

void MainWindow::reset_settings()
{
    ui->space->reset_nums();
    ui->colour_box->setValue(ui->space->get_colour_num());
    ui->iter_box->setValue(ui->space->get_iter_num());
    ui->space->call_repaint();
}

void MainWindow::reset()
{
    ui->space->reset();
    ui->colour_box->setValue(ui->space->get_colour_num());
    ui->iter_box->setValue(ui->space->get_iter_num());
}

MainWindow::~MainWindow()
{}

