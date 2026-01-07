#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QMessageBox>
#include <QStandardPaths>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Password Manager");
    app.setOrganizationName("QtCourse");

    // 设置应用程序图标（从资源文件加载）
    app.setWindowIcon(QIcon(":/appicon.png"));

    // 输出调试信息
    qDebug() << "当前工作目录:" << QDir::currentPath();
    qDebug() << "可用数据库驱动:" << QSqlDatabase::drivers();

    // 检查SQLite驱动是否可用
    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        qDebug() << "错误: SQLite驱动不可用!";
        QMessageBox::critical(nullptr, "错误", "SQLite驱动不可用，请安装SQLite驱动。");
        return 1;
    }

    // 设置数据库文件路径
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    dbPath += "/passwords.db";
    qDebug() << "数据库路径:" << dbPath;

    // 添加数据库连接
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qDebug() << "无法打开数据库:" << db.lastError().text();
        QMessageBox::critical(nullptr, "数据库错误",
                              QString("无法打开数据库:\n%1\n%2").arg(dbPath).arg(db.lastError().text()));
        return 1;
    }

    qDebug() << "数据库已成功打开";

    MainWindow window;
    window.show();

    return app.exec();
}
