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
#include <QToolButton>

// 前向声明
class ImportExportWorker;
class FormTabWidget;
class FormSelectDialog;
class QProgressDialog;

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

    // 表单相关槽函数
    void onFormAdded(int id, const QString &name);
    void onFormRemoved(int id);
    void onFormRenamed(int id, const QString &newName);
    void onCurrentFormChanged(int id);
    void onFormSelectionChanged(const QList<int> &selectedIds);
    void onSelectFormsClicked();

    // 多线程操作槽函数
    void onImportProgress(int percent, const QString &message);
    void onExportProgress(int percent, const QString &message);
    void onOperationFinished(bool success, const QString &message);
    void onOperationError(const QString &error);
    void cancelOperation();

private:
    void setupUI();
    void loadForms();
    void loadPasswords();
    void setupTable();
    bool exportSelectedPasswords(const QString &filename);
    void updateButtonStates();
    void clearSelection();
    void editSelectedRow(int row);
    bool editField(int row, int column);
    void updateSelectAllButtonText();
    void clearAllCheckboxes();
    void createProgressDialog();

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
    QToolButton *selectFormsButton;  // 新增：选择表单按钮

    // 表单标签栏
    FormTabWidget *formTabWidget;

    QMenuBar *menuBar;
    QStatusBar *statusBar;

    // 多线程相关成员
    QProgressDialog *progressDialog;
    QThread *workerThread;
    ImportExportWorker *worker;
    bool operationInProgress;

    bool multiSelectMode;
    int lastSelectedRow;
    bool isAllSelected;

    // 当前选中的表单ID列表（用于搜索）
    QList<int> selectedFormIdsForSearch;
    int currentFormId;  // 当前激活的表单ID
};

#endif // MAINWINDOW_H
