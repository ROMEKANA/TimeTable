#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::setupStudentTab(){
    
    connect(ui->addStudentButton, &QPushButton::clicked, this, &MainWindow::addStudent);
    connect(ui->removeStudentButton, &QPushButton::clicked, this, &MainWindow::removeStudent);
    connect(ui->studentListView, &QListView::, this, &MainWindow::loadStudent(index));
}

