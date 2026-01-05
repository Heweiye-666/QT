#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QSqlDatabase>  // 添加这行
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Password Manager");
    app.setOrganizationName("QtCourse");

    // 输出调试信息
    qDebug() << "当前工作目录:" << QDir::currentPath();
    qDebug() << "可用数据库驱动:" << QSqlDatabase::drivers();

    MainWindow window;
    window.show();

    return app.exec();
}
