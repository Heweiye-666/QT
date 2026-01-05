QT += core gui sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# 编译选项
CONFIG += debug_and_release
debug {
    DESTDIR = debug
    OBJECTS_DIR = debug/.obj
    MOC_DIR = debug/.moc
    RCC_DIR = debug/.qrc
    UI_DIR = debug/.ui
}
release {
    DESTDIR = release
    OBJECTS_DIR = release/.obj
    MOC_DIR = release/.moc
    RCC_DIR = release/.qrc
    UI_DIR = release/.ui
}

# 输出文件名
TARGET = PasswordManager
TEMPLATE = app

# 源文件
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    database.cpp \
    encryption.cpp \
    importexportworker.cpp

HEADERS += \
    mainwindow.h \
    database.h \
    encryption.h \
    importexportworker.h

# 资源文件
RESOURCES += resources.qrc
