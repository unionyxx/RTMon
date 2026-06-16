#include <QApplication>
#include "mainwindow.h"
#include <QIcon>

int main(int argc, char *argv[]) {
    // Force native Wayland platform
    qputenv("QT_QPA_PLATFORM", "wayland");

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/resources/icon.svg"));
    MainWindow w;
    w.show();
    return a.exec();
}
