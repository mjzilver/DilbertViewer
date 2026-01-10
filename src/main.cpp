#include <QApplication>

#include "DilbertViewer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    DilbertViewer viewer;
    viewer.show();

    return app.exec();
}
