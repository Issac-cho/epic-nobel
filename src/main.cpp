#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    app.setApplicationName("LightCell");
    app.setOrganizationName("LightCell");
    app.setWindowIcon(QIcon(":/icons/lightcell.png"));
    
    // Set global app style
    app.setStyle("Fusion");

    MainWindow w;
    w.show();
    
    return app.exec();
}
