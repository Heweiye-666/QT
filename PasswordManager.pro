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

# ============ 使用RC文件方式设置Windows可执行文件图标 ============
# 方法1：创建专门的.rc文件（推荐）
RC_FILE = app.rc

# 或者方法2：直接指定图标文件（二选一）
# win32:RC_ICONS = appicon.ico

# 同时使用Qt资源文件用于程序内部图标
RESOURCES += icons.qrc

# =================================================

# 源文件
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    database.cpp \
    encryption.cpp \
    importexportworker.cpp \
    formtabwidget.cpp \
    formselectdialog.cpp

HEADERS += \
    mainwindow.h \
    database.h \
    encryption.h \
    importexportworker.h \
    formtabwidget.h \
    formselectdialog.h

# 添加包含路径
INCLUDEPATH += .

# 确保.rc文件被正确包含
win32: INCLUDEPATH += .
