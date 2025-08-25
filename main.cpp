#include <QApplication>
#include "Home.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 创建并显示主窗口
    Home home;
    home.show();

    return app.exec();
}
