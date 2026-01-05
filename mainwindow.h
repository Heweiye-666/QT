#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTableView>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QMenuBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QThread>

// 前向声明
class ImportExportWorker;
class QProgressDialog;  // 前向声明，不包含头文件

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void addPassword();
    void editPassword();
    void deletePassword();
    void searchPasswords();
    void exportPasswords();
    void importPasswords();
    void showAbout();
    void testDatabase();
    void toggleMultiSelectMode();
    void selectAll();
    void onCheckboxStateChanged();
    void onTableViewClicked(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);

    // 多线程操作槽函数
    void onImportProgress(int percent, const QString &message);
    void onExportProgress(int percent, const QString &message);
    void onOperationFinished(bool success, const QString &message);
    void onOperationError(const QString &error);
    void cancelOperation();

private:
    void setupUI();
    void loadPasswords();
    void setupTable();
    bool exportSelectedPasswords(const QString &filename);
    void updateButtonStates();
    void clearSelection();
    void editSelectedRow(int row);
    bool editField(int row, int column);
    void updateSelectAllButtonText();
    void clearAllCheckboxes();
    void createProgressDialog();  // 改为创建函数，而不是在构造函数中创建

    // 多线程操作方法
    void startImportOperation(const QString &filename);
    void startExportOperation(const QString &filename, bool exportEncrypted);
    void startExportSelectedOperation(const QString &filename, const QList<int> &selectedRows, bool exportEncrypted);

    QStandardItemModel *model;
    QTableView *tableView;
    QLineEdit *searchEdit;
    QPushButton *addButton, *editButton, *deleteButton, *searchButton;
    QPushButton *exportButton, *importButton, *testButton;
    QPushButton *showPasswordButton;
    QPushButton *multiSelectButton;
    QPushButton *selectAllButton;
    QMenuBar *menuBar;
    QStatusBar *statusBar;

    // 多线程相关成员
    QProgressDialog *progressDialog;  // 仅在需要时创建
    QThread *workerThread;
    ImportExportWorker *worker;
    bool operationInProgress;

    bool multiSelectMode;
    int lastSelectedRow;
    bool isAllSelected;
};

#endif // MAINWINDOW_H
