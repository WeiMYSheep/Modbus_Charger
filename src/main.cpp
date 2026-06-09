#include "ui/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Ui::MainWindow window;
    window.show();
    return app.exec();
}

