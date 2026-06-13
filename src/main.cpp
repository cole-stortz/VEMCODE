#include <QApplication>
#include "src/ui/mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("VEMCODE");
    app.setApplicationVersion("0.1");

    MainWindow window;
    window.show();

    return app.exec();
}