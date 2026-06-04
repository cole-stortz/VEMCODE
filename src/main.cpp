#include <QApplication>
#include "src/ui/mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);           // Create the application
    app.setApplicationName("VirtualEmbeddedProgrammer"); // with a title and version number
    app.setApplicationVersion("0.1");

    MainWindow window;
    window.show();                          // Create and show the main window in the app

    return app.exec();                      // Execute
}