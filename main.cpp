#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QTextStream>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#include "Backend.h"
#include "Home.h"
#include"TtsTest.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    

        Home backend;
        backend.show();
        
        qDebug() << "Backend窗口已启动";
        
        return app.exec();
}
