#include "mainwindow.h"
#include "database.h"
#include "encryption.h"
#include "importexportworker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QInputDialog>
#include <QStatusBar>
#include <QTextStream>
#include <QDebug>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QAbstractItemView>
#include <QFileInfo>
#include <QProgressDialog>
#include <QApplication>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), model(new QStandardItemModel(this)),
    progressDialog(nullptr), workerThread(nullptr), worker(nullptr),
    operationInProgress(false), multiSelectMode(false),
    lastSelectedRow(-1), isAllSelected(false)
{
    // 先初始化数据库
    if (!Database::instance().init()) {
        QMessageBox::critical(this, "数据库错误",
                              "无法初始化数据库，程序可能无法正常工作。\n请检查SQLite驱动是否可用。");
    }

    setupUI();
    loadPasswords();
}

MainWindow::~MainWindow()
{
    // 清理多线程资源
    if (workerThread && workerThread->isRunning()) {
        workerThread->quit();
        workerThread->wait();
    }

    delete worker;
    delete workerThread;
    delete progressDialog;
}

void MainWindow::setupUI()
{
    setWindowTitle("个人密码管理器（多线程版）");
    resize(900, 600);

    // 中央部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 搜索栏
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("搜索网站、用户名...");
    searchButton = new QPushButton("搜索");

    // 连接搜索按钮的点击事件
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::searchPasswords);

    // 连接回车键事件
    connect(searchEdit, &QLineEdit::returnPressed, this, &MainWindow::searchPasswords);

    searchLayout->addWidget(searchEdit);
    searchLayout->addWidget(searchButton);
    searchLayout->addStretch();

    // 表格
    tableView = new QTableView();
    setupTable();

    // 按钮栏
    addButton = new QPushButton("添加密码");
    editButton = new QPushButton("编辑");
    deleteButton = new QPushButton("删除");
    exportButton = new QPushButton("导出");
    importButton = new QPushButton("导入");
    testButton = new QPushButton("测试数据库");

    // 多选和全选按钮
    multiSelectButton = new QPushButton("多选");
    selectAllButton = new QPushButton("全选");
    selectAllButton->setEnabled(false);

    connect(multiSelectButton, &QPushButton::clicked, this, &MainWindow::toggleMultiSelectMode);
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAll);

    // 显示/隐藏密码按钮
    showPasswordButton = new QPushButton("显示密码");
    connect(showPasswordButton, &QPushButton::clicked, [this]() {
        bool isHidden = tableView->isColumnHidden(4);  // 第4列是密码列
        tableView->setColumnHidden(4, !isHidden);
        if (isHidden) {
            this->showPasswordButton->setText("隐藏密码");
        } else {
            this->showPasswordButton->setText("显示密码");
        }
    });

    connect(addButton, &QPushButton::clicked, this, &MainWindow::addPassword);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editPassword);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deletePassword);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportPasswords);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importPasswords);
    connect(testButton, &QPushButton::clicked, this, &MainWindow::testDatabase);

    // 第一行按钮
    QHBoxLayout *buttonLayout1 = new QHBoxLayout();
    buttonLayout1->addWidget(addButton);
    buttonLayout1->addWidget(editButton);
    buttonLayout1->addWidget(deleteButton);
    buttonLayout1->addStretch();
    buttonLayout1->addWidget(multiSelectButton);
    buttonLayout1->addWidget(selectAllButton);
    buttonLayout1->addStretch();
    buttonLayout1->addWidget(showPasswordButton);

    // 第二行按钮
    QHBoxLayout *buttonLayout2 = new QHBoxLayout();
    buttonLayout2->addStretch();
    buttonLayout2->addWidget(exportButton);
    buttonLayout2->addWidget(importButton);
    buttonLayout2->addWidget(testButton);
    buttonLayout2->addStretch();

    // 添加到主布局
    mainLayout->addLayout(searchLayout);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(buttonLayout1);
    mainLayout->addLayout(buttonLayout2);

    // 菜单栏
    menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    QMenu *fileMenu = menuBar->addMenu("文件");
    QAction *exportAction = fileMenu->addAction("导出密码");
    QAction *importAction = fileMenu->addAction("导入密码");
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("退出");

    connect(exportAction, &QAction::triggered, this, &MainWindow::exportPasswords);
    connect(importAction, &QAction::triggered, this, &MainWindow::importPasswords);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    QMenu *helpMenu = menuBar->addMenu("帮助");
    QAction *aboutAction = helpMenu->addAction("关于");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    // 状态栏
    statusBar = new QStatusBar();
    setStatusBar(statusBar);
    statusBar->showMessage("就绪");
}

void MainWindow::createProgressDialog()
{
    // 如果已经存在，先删除
    if (progressDialog) {
        delete progressDialog;
        progressDialog = nullptr;
    }

    // 创建新的进度对话框
    progressDialog = new QProgressDialog(this);
    progressDialog->setWindowTitle("操作进度");
    progressDialog->setLabelText("正在处理...");
    progressDialog->setCancelButtonText("取消");
    progressDialog->setRange(0, 100);
    progressDialog->setValue(0);

    // 修改为无模态，不阻塞主窗口
    progressDialog->setModal(false);

    // 设置为非置顶窗口
    progressDialog->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    // 不自动关闭和重置，由我们手动控制
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);

    // 设置最小化时不显示进度对话框
    progressDialog->setAttribute(Qt::WA_ShowWithoutActivating, false);

    connect(progressDialog, &QProgressDialog::canceled, this, &MainWindow::cancelOperation);
}

void MainWindow::setupTable()
{
    model->setColumnCount(8);  // 修改：增加一列，现在是8列
    model->setHeaderData(0, Qt::Horizontal, "ID");
    model->setHeaderData(1, Qt::Horizontal, "网站/应用");
    model->setHeaderData(2, Qt::Horizontal, "用户名");
    model->setHeaderData(3, Qt::Horizontal, "账号");  // 新增：账号列
    model->setHeaderData(4, Qt::Horizontal, "密码（明文）");
    model->setHeaderData(5, Qt::Horizontal, "备注");
    model->setHeaderData(6, Qt::Horizontal, "加密账号密码");  // 合并加密的账号和密码
    model->setHeaderData(7, Qt::Horizontal, "选择");

    tableView->setModel(model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->setAlternatingRowColors(true);

    // 设置列宽
    tableView->setColumnWidth(1, 150);
    tableView->setColumnWidth(2, 120);
    tableView->setColumnWidth(3, 120);  // 新增：账号列宽
    tableView->setColumnWidth(4, 100);
    tableView->setColumnWidth(5, 200);
    tableView->setColumnWidth(7, 60);

    // 隐藏列
    tableView->setColumnHidden(0, true);
    tableView->setColumnHidden(6, true);
    tableView->setColumnHidden(7, !multiSelectMode);

    // 连接信号
    connect(tableView, &QTableView::clicked, this, &MainWindow::onTableViewClicked);
    connect(tableView, &QTableView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
}

void MainWindow::onTableViewClicked(const QModelIndex &index)
{
    // 获取当前选中模型
    QItemSelectionModel *selectionModel = tableView->selectionModel();

    if (!selectionModel) {
        return;
    }

    // 获取当前点击的行
    int clickedRow = index.row();

    // 检查是否点击了表格的空白区域（无效行）
    if (clickedRow < 0 || clickedRow >= model->rowCount()) {
        // 点击了表格的空白区域，清除选中
        if (multiSelectMode) {
            // 在多选模式下，清除所有复选框的选中状态
            clearAllCheckboxes();
            statusBar->showMessage("已清除所有选择");
        } else {
            // 在普通模式下，清除行选择
            selectionModel->clearSelection();
            lastSelectedRow = -1;
            statusBar->showMessage("已清除选择");
        }
        return;
    }

    // 如果点击的是有效的行
    if (multiSelectMode) {
        // 在多选模式下，不处理点击选中逻辑，由复选框控制
        // 只是更新状态栏显示信息
        QString website = model->item(clickedRow, 1)->text();
        QString username = model->item(clickedRow, 2)->text();
        statusBar->showMessage(QString("已选择行: %1 - %2").arg(website).arg(username));
    } else {
        // 在普通模式下
        // 如果点击的是上次选中的同一行，取消选中
        if (clickedRow == lastSelectedRow) {
            selectionModel->clearSelection();
            lastSelectedRow = -1;
            statusBar->showMessage("已取消选择");
        } else {
            // 否则，选中新的一行
            tableView->selectRow(clickedRow);
            lastSelectedRow = clickedRow;

            QString website = model->item(clickedRow, 1)->text();
            QString username = model->item(clickedRow, 2)->text();
            statusBar->showMessage(QString("已选择: %1 - %2").arg(website).arg(username));
        }
    }
}

void MainWindow::clearAllCheckboxes()
{
    // 清除所有复选框的选中状态
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* checkItem = model->item(i, 7);
        if (checkItem) {
            checkItem->setCheckState(Qt::Unchecked);
        }
    }

    // 更新全选状态和按钮文字
    isAllSelected = false;
    updateSelectAllButtonText();
}

void MainWindow::onItemDoubleClicked(const QModelIndex &index)
{
    if (multiSelectMode) {
        // 多选模式下，不允许直接编辑单元格
        QMessageBox::information(this, "提示", "多选模式下请使用编辑按钮进行编辑");
        return;
    }

    int row = index.row();
    int column = index.column();

    // 检查是否点击了有效单元格
    if (row < 0 || row >= model->rowCount()) {
        return;
    }

    // 检查是否点击了可编辑的列（网站、用户名、账号、密码、备注）
    if (column >= 1 && column <= 5) {
        editField(row, column);
    } else {
        // 如果点击了ID列或加密密码列，提示不可编辑
        if (column == 0 || column == 6) {
            QMessageBox::information(this, "提示", "此列不可编辑");
        }
    }
}

bool MainWindow::editField(int row, int column)
{
    // 从第0列获取ID
    QStandardItem* idItem = model->item(row, 0);
    if (!idItem) {
        statusBar->showMessage("获取记录ID失败");
        return false;
    }

    int id = idItem->data(Qt::UserRole + 1).toInt();

    // 获取当前单元格的值
    QString currentValue = model->item(row, column)->text();

    // 根据列设置对话框标题和标签
    QString title, label;
    switch(column) {
    case 1: // 网站
        title = "编辑网站";
        label = "网站/应用:";
        break;
    case 2: // 用户名
        title = "编辑用户名";
        label = "用户名:";
        break;
    case 3: // 账号
        title = "编辑账号";
        label = "账号(6-20位):";
        break;
    case 4: // 密码
        title = "编辑密码";
        label = "密码(6-20位):";
        break;
    case 5: // 备注
        title = "编辑备注";
        label = "备注:";
        break;
    default:
        title = "编辑";
        label = "值:";
        break;
    }

    bool ok;
    QString newValue;

    // 对于账号和密码字段，需要循环验证直到输入正确或用户取消
    if (column == 3 || column == 4) { // 账号或密码列
        while (true) {
            newValue = QInputDialog::getText(this, title, label,
                                             QLineEdit::Normal, currentValue, &ok);

            if (!ok) {
                statusBar->showMessage("已取消编辑");
                return false;
            }

            // 验证长度
            if (newValue.length() >= 6 && newValue.length() <= 20) {
                break; // 验证通过，退出循环
            } else {
                // 验证失败，显示错误提示，然后重新输入
                QMessageBox::warning(this, "输入错误", "请输入正确格式(6-20位)!");
                currentValue = newValue; // 使用上次输入的值作为新对话框的初始值
            }
        }
    } else {
        // 其他字段直接输入
        newValue = QInputDialog::getText(this, title, label,
                                         QLineEdit::Normal, currentValue, &ok);

        if (!ok) {
            statusBar->showMessage("已取消编辑");
            return false;
        }
    }

    // 如果值没有变化，直接返回
    if (newValue == currentValue) {
        statusBar->showMessage("值未变化");
        return true;
    }

    // 获取该行的所有数据
    QString website = model->item(row, 1)->text();
    QString username = model->item(row, 2)->text();
    QString account = model->item(row, 3)->text(); // 明文账号
    QString password = model->item(row, 4)->text(); // 明文密码
    QString notes = model->item(row, 5)->text();

    // 根据编辑的列更新对应的值
    switch(column) {
    case 1: // 网站
        website = newValue;
        break;
    case 2: // 用户名
        username = newValue;
        break;
    case 3: // 账号
        account = newValue;
        break;
    case 4: // 密码
        password = newValue;
        break;
    case 5: // 备注
        notes = newValue;
        break;
    }

    // 加密账号和密码
    QString encryptedAccount = Encryption::encrypt(account);
    QString encryptedPassword = Encryption::encrypt(password);

    // 更新数据库
    if (Database::instance().updatePassword(id, website, username, encryptedAccount, encryptedPassword, notes)) {
        // 更新模型中的数据
        model->item(row, column)->setText(newValue);

        // 如果是账号或密码列，还需要更新加密列
        if (column == 3 || column == 4) {
            QString encryptedData = encryptedAccount + "|" + encryptedPassword;
            model->item(row, 6)->setText(encryptedData);
        }

        statusBar->showMessage("编辑成功");
        return true;
    } else {
        statusBar->showMessage("编辑失败，可能是新的网站、用户名和账号组合已存在");
        return false;
    }
}

void MainWindow::clearSelection()
{
    QItemSelectionModel *selectionModel = tableView->selectionModel();
    if (selectionModel) {
        selectionModel->clearSelection();
    }
    lastSelectedRow = -1;

    // 在多选模式下，还需要清除所有复选框的选择
    if (multiSelectMode) {
        clearAllCheckboxes();
    }
}

void MainWindow::loadPasswords()
{
    // 断开之前的连接，避免重复连接
    disconnect(model, &QStandardItemModel::itemChanged, this, &MainWindow::onCheckboxStateChanged);

    model->removeRows(0, model->rowCount());

    auto passwords = Database::instance().getAllPasswords();
    for (const auto &pwd : passwords) {
        QList<QStandardItem*> row;

        // 第0列：隐藏的ID
        QStandardItem* idItem = new QStandardItem();
        idItem->setData(pwd.id, Qt::UserRole + 1);  // 将ID存储在UserRole+1中
        row << idItem;

        // 第1列：网站
        QStandardItem* websiteItem = new QStandardItem(pwd.website);
        websiteItem->setEditable(false);
        row << websiteItem;

        // 第2列：用户名
        QStandardItem* usernameItem = new QStandardItem(pwd.username);
        usernameItem->setEditable(false);
        row << usernameItem;

        // 第3列：账号（解密后）
        QStandardItem* accountItem = new QStandardItem(Encryption::decrypt(pwd.account));
        accountItem->setEditable(false);
        row << accountItem;

        // 第4列：密码（解密后）
        QStandardItem* passwordItem = new QStandardItem(Encryption::decrypt(pwd.password));
        passwordItem->setEditable(false);
        row << passwordItem;

        // 第5列：备注
        QStandardItem* notesItem = new QStandardItem(pwd.notes);
        notesItem->setEditable(false);
        row << notesItem;

        // 第6列：加密账号和密码
        QStandardItem* encryptedItem = new QStandardItem(pwd.account + "|" + pwd.password);
        encryptedItem->setEditable(false);
        row << encryptedItem;

        // 第7列：选择框
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setEditable(false);
        checkItem->setCheckState(Qt::Unchecked);
        row << checkItem;

        model->appendRow(row);
    }

    statusBar->showMessage(QString("加载了 %1 条记录").arg(passwords.size()));

    // 重置全选状态
    isAllSelected = false;
    updateSelectAllButtonText();

    // 清除选中状态
    clearSelection();

    // 连接复选框状态改变信号
    connect(model, &QStandardItemModel::itemChanged, this, &MainWindow::onCheckboxStateChanged);
}

void MainWindow::updateButtonStates()
{
    // 更新按钮状态
    if (multiSelectMode) {
        selectAllButton->setEnabled(true);
        deleteButton->setText("批量删除");
        exportButton->setText("批量导出");
        tableView->setSelectionMode(QAbstractItemView::MultiSelection);
    } else {
        selectAllButton->setEnabled(false);
        deleteButton->setText("删除");
        exportButton->setText("导出");
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    }
}

void MainWindow::toggleMultiSelectMode()
{
    multiSelectMode = !multiSelectMode;

    if (multiSelectMode) {
        multiSelectButton->setText("取消多选");
        tableView->setColumnHidden(7, false);  // 显示选择框列
    } else {
        multiSelectButton->setText("多选");
        tableView->setColumnHidden(7, true);   // 隐藏选择框列

        // 清除所有复选框的选择状态
        clearAllCheckboxes();
    }

    // 清除当前选中状态
    clearSelection();

    updateButtonStates();
}

void MainWindow::updateSelectAllButtonText()
{
    if (isAllSelected) {
        selectAllButton->setText("取消全选");
    } else {
        selectAllButton->setText("全选");
    }
}

void MainWindow::selectAll()
{
    if (!multiSelectMode) {
        return;
    }

    // 切换全选状态
    isAllSelected = !isAllSelected;

    // 根据全选状态设置所有复选框
    Qt::CheckState newState = isAllSelected ? Qt::Checked : Qt::Unchecked;

    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* checkItem = model->item(i, 7);
        if (checkItem) {
            checkItem->setCheckState(newState);
        }
    }

    // 更新按钮文字
    updateSelectAllButtonText();

    // 更新状态栏
    if (isAllSelected) {
        statusBar->showMessage(QString("已全选 %1 条记录").arg(model->rowCount()));
    } else {
        statusBar->showMessage("已取消全选");
    }
}

void MainWindow::onCheckboxStateChanged()
{
    if (!multiSelectMode) {
        return;
    }

    // 计算当前选中数量
    int selectedCount = 0;
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* checkItem = model->item(i, 7);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            selectedCount++;
        }
    }

    // 更新全选状态
    isAllSelected = (selectedCount == model->rowCount() && model->rowCount() > 0);
    updateSelectAllButtonText();

    if (selectedCount > 0) {
        statusBar->showMessage(QString("已选中 %1 条记录").arg(selectedCount));
    } else {
        statusBar->showMessage("就绪");
    }
}

void MainWindow::addPassword()
{
    // 清除当前选中状态
    clearSelection();

    // 简单对话框添加
    bool ok;
    QString website = QInputDialog::getText(this, "添加密码", "网站/应用:", QLineEdit::Normal, "", &ok);
    if (!ok || website.isEmpty()) {
        statusBar->showMessage("已取消添加");
        return;
    }

    QString username = QInputDialog::getText(this, "添加密码", "用户名:", QLineEdit::Normal, "", &ok);
    if (!ok) {
        statusBar->showMessage("已取消添加");
        return;
    }

    // 账号输入 - 循环验证
    QString account;
    QString accountTemp = "";
    while (true) {
        account = QInputDialog::getText(this, "添加密码", "账号(6-20位):", QLineEdit::Normal, accountTemp, &ok);
        if (!ok) {
            statusBar->showMessage("已取消添加");
            return;
        }

        // 验证账号格式
        if (account.length() >= 6 && account.length() <= 20) {
            break; // 验证通过，退出循环
        } else {
            // 验证失败，显示错误提示，然后重新输入
            QMessageBox::warning(this, "输入错误", "请输入正确格式的账号(6-20位)!");
            accountTemp = account; // 使用上次输入的值作为新对话框的初始值
        }
    }

    // 密码输入 - 循环验证
    QString password;
    QString passwordTemp = "";
    while (true) {
        password = QInputDialog::getText(this, "添加密码", "密码(6-20位):", QLineEdit::Normal, passwordTemp, &ok);
        if (!ok) {
            statusBar->showMessage("已取消添加");
            return;
        }

        // 验证密码格式
        if (password.length() >= 6 && password.length() <= 20) {
            break; // 验证通过，退出循环
        } else {
            // 验证失败，显示错误提示，然后重新输入
            QMessageBox::warning(this, "输入错误", "请输入正确格式的密码(6-20位)!");
            passwordTemp = password; // 使用上次输入的值作为新对话框的初始值
        }
    }

    QString notes = QInputDialog::getText(this, "添加密码", "备注:", QLineEdit::Normal, "", &ok);

    // 加密账号和密码
    QString encryptedAccount = Encryption::encrypt(account);
    QString encryptedPassword = Encryption::encrypt(password);

    if (Database::instance().addPassword(website, username, encryptedAccount, encryptedPassword, notes)) {
        loadPasswords();
        statusBar->showMessage("密码添加成功");
    } else {
        statusBar->showMessage("密码添加失败，可能已存在相同记录");
    }
}

void MainWindow::editSelectedRow(int row)
{
    // 从第0列获取ID
    QStandardItem* idItem = model->item(row, 0);
    if (!idItem) {
        statusBar->showMessage("获取记录ID失败");
        return;
    }

    int id = idItem->data(Qt::UserRole + 1).toInt();

    bool ok;
    QString website = QInputDialog::getText(this, "编辑密码", "网站/应用:",
                                            QLineEdit::Normal, model->item(row, 1)->text(), &ok);
    if (!ok) {
        statusBar->showMessage("已取消编辑");
        return;
    }

    QString username = QInputDialog::getText(this, "编辑密码", "用户名:",
                                             QLineEdit::Normal, model->item(row, 2)->text(), &ok);
    if (!ok) {
        statusBar->showMessage("已取消编辑");
        return;
    }

    // 账号输入 - 循环验证
    QString account;
    QString accountTemp = Encryption::decrypt(model->item(row, 3)->text());
    while (true) {
        account = QInputDialog::getText(this, "编辑密码", "账号(6-20位):",
                                        QLineEdit::Normal, accountTemp, &ok);
        if (!ok) {
            statusBar->showMessage("已取消编辑");
            return;
        }

        // 验证账号格式
        if (account.length() >= 6 && account.length() <= 20) {
            break; // 验证通过，退出循环
        } else {
            // 验证失败，显示错误提示，然后重新输入
            QMessageBox::warning(this, "输入错误", "请输入正确格式的账号(6-20位)!");
            accountTemp = account; // 使用上次输入的值作为新对话框的初始值
        }
    }

    // 密码输入 - 循环验证
    QString password;
    QString passwordTemp = model->item(row, 4)->text();
    while (true) {
        password = QInputDialog::getText(this, "编辑密码", "密码(6-20位):",
                                         QLineEdit::Normal, passwordTemp, &ok);
        if (!ok) {
            statusBar->showMessage("已取消编辑");
            return;
        }

        // 验证密码格式
        if (password.length() >= 6 && password.length() <= 20) {
            break; // 验证通过，退出循环
        } else {
            // 验证失败，显示错误提示，然后重新输入
            QMessageBox::warning(this, "输入错误", "请输入正确格式的密码(6-20位)!");
            passwordTemp = password; // 使用上次输入的值作为新对话框的初始值
        }
    }

    QString notes = QInputDialog::getText(this, "编辑密码", "备注:",
                                          QLineEdit::Normal, model->item(row, 5)->text(), &ok);

    QString newEncryptedAccount = Encryption::encrypt(account);
    QString newEncryptedPassword = Encryption::encrypt(password);
    if (Database::instance().updatePassword(id, website, username, newEncryptedAccount, newEncryptedPassword, notes)) {
        loadPasswords();
        statusBar->showMessage("密码更新成功");
    } else {
        statusBar->showMessage("密码更新失败，可能是新的网站、用户名和账号组合已存在");
    }
}

void MainWindow::editPassword()
{
    // 根据当前模式判断如何获取选中的行
    if (multiSelectMode) {
        // 在多选模式下，检查是否有选中的行
        QList<int> selectedRows;
        for (int i = 0; i < model->rowCount(); ++i) {
            QStandardItem* checkItem = model->item(i, 7);
            if (checkItem && checkItem->checkState() == Qt::Checked) {
                selectedRows.append(i);
            }
        }

        if (selectedRows.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择一条记录");
            return;
        }

        if (selectedRows.size() > 1) {
            QMessageBox::warning(this, "警告", "编辑操作只能选择一条记录");
            return;
        }

        // 编辑选中的单条记录
        int row = selectedRows.first();
        editSelectedRow(row);
    } else {
        // 在普通模式下，使用当前选中的行
        QItemSelectionModel *selectionModel = tableView->selectionModel();
        if (!selectionModel || !selectionModel->hasSelection()) {
            QMessageBox::warning(this, "警告", "请先选择一条记录");
            return;
        }

        QModelIndexList selectedIndexes = selectionModel->selectedRows();
        if (selectedIndexes.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择一条记录");
            return;
        }

        int row = selectedIndexes.first().row();
        editSelectedRow(row);
    }
}

void MainWindow::deletePassword()
{
    if (multiSelectMode) {
        // 批量删除模式
        QList<int> rowsToDelete;
        QList<int> idsToDelete;

        // 收集要删除的行和ID
        for (int i = 0; i < model->rowCount(); ++i) {
            QStandardItem* checkItem = model->item(i, 7);  // 第7列是选择框
            if (checkItem && checkItem->checkState() == Qt::Checked) {
                rowsToDelete.append(i);
                int id = model->item(i, 0)->data(Qt::UserRole + 1).toInt();  // 获取ID
                idsToDelete.append(id);
            }
        }

        if (rowsToDelete.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择要删除的记录");
            return;
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认批量删除",
                                      QString("确定要删除选中的 %1 条记录吗?").arg(rowsToDelete.size()),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            bool allDeleted = true;
            for (int i = rowsToDelete.size() - 1; i >= 0; --i) {
                int row = rowsToDelete[i];
                int id = idsToDelete[i];

                if (!Database::instance().deletePassword(id)) {
                    allDeleted = false;
                } else {
                    model->removeRow(row);
                }
            }

            if (allDeleted) {
                statusBar->showMessage(QString("成功删除 %1 条记录").arg(rowsToDelete.size()));
            } else {
                statusBar->showMessage("部分记录删除失败");
            }

            // 重置全选状态
            isAllSelected = false;
            updateSelectAllButtonText();

            // 清除选中状态
            clearSelection();
        } else {
            statusBar->showMessage("已取消删除");
        }
    } else {
        // 单个删除模式
        QItemSelectionModel *selectionModel = tableView->selectionModel();
        if (!selectionModel || !selectionModel->hasSelection()) {
            QMessageBox::warning(this, "警告", "请先选择一条记录");
            return;
        }

        QModelIndexList selectedIndexes = selectionModel->selectedRows();
        if (selectedIndexes.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择一条记录");
            return;
        }

        int row = selectedIndexes.first().row();

        // 从第0列获取ID
        QStandardItem* idItem = model->item(row, 0);
        if (!idItem) {
            statusBar->showMessage("获取记录ID失败");
            return;
        }

        int id = idItem->data(Qt::UserRole + 1).toInt();
        QString website = model->item(row, 1)->text();
        QString username = model->item(row, 2)->text();

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要删除 '%1' 用户 '%2' 的密码记录吗?").arg(website).arg(username),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            if (Database::instance().deletePassword(id)) {
                loadPasswords();  // 重新加载数据
                statusBar->showMessage("密码删除成功");
            } else {
                statusBar->showMessage("密码删除失败");
            }
        } else {
            statusBar->showMessage("已取消删除");
        }
    }
}

void MainWindow::searchPasswords()
{
    QString keyword = searchEdit->text().trimmed();
    if (keyword.isEmpty()) {
        loadPasswords();
        return;
    }

    auto results = Database::instance().searchPasswords(keyword);

    // 断开之前的连接，避免重复连接
    disconnect(model, &QStandardItemModel::itemChanged, this, &MainWindow::onCheckboxStateChanged);

    model->removeRows(0, model->rowCount());

    for (const auto &pwd : results) {
        QList<QStandardItem*> row;

        // 第0列：隐藏的ID
        QStandardItem* idItem = new QStandardItem();
        idItem->setData(pwd.id, Qt::UserRole + 1);
        row << idItem;

        // 第1列：网站
        QStandardItem* websiteItem = new QStandardItem(pwd.website);
        websiteItem->setEditable(false);
        row << websiteItem;

        // 第2列：用户名
        QStandardItem* usernameItem = new QStandardItem(pwd.username);
        usernameItem->setEditable(false);
        row << usernameItem;

        // 第3列：账号（解密后）
        QStandardItem* accountItem = new QStandardItem(Encryption::decrypt(pwd.account));
        accountItem->setEditable(false);
        row << accountItem;

        // 第4列：密码（解密后）
        QStandardItem* passwordItem = new QStandardItem(Encryption::decrypt(pwd.password));
        passwordItem->setEditable(false);
        row << passwordItem;

        // 第5列：备注
        QStandardItem* notesItem = new QStandardItem(pwd.notes);
        notesItem->setEditable(false);
        row << notesItem;

        // 第6列：加密账号和密码
        QStandardItem* encryptedItem = new QStandardItem(pwd.account + "|" + pwd.password);
        encryptedItem->setEditable(false);
        row << encryptedItem;

        // 第7列：选择框
        QStandardItem* checkItem = new QStandardItem();
        checkItem->setCheckable(true);
        checkItem->setEditable(false);
        checkItem->setCheckState(Qt::Unchecked);
        row << checkItem;

        model->appendRow(row);
    }

    // 重置全选状态
    isAllSelected = false;
    updateSelectAllButtonText();

    // 重新连接复选框状态改变信号
    connect(model, &QStandardItemModel::itemChanged, this, &MainWindow::onCheckboxStateChanged);

    // 清除选中状态
    clearSelection();

    statusBar->showMessage(QString("找到 %1 条匹配记录").arg(results.size()));
}

bool MainWindow::exportSelectedPasswords(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件:" << filename;
        return false;
    }

    QTextStream out(&file);

    // 写入UTF-8 BOM以确保正确识别编码（Windows系统需要）
    out << "\xEF\xBB\xBF";

    // 写入表头
    out << "Website,Username,Account,Password,Notes\n";

    int exportedCount = 0;

    // 按顺序导出选中的行
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* checkItem = model->item(i, 7);
        if (checkItem && checkItem->checkState() == Qt::Checked) {
            QString website = model->item(i, 1)->text();
            QString username = model->item(i, 2)->text();
            QString account = model->item(i, 3)->text();  // 账号
            QString password = model->item(i, 4)->text();  // 明文密码
            QString notes = model->item(i, 5)->text();

            // CSV转义函数
            auto escapeCSV = [](const QString &field) -> QString {
                QString escaped = field;
                bool needsQuotes = escaped.contains(',') ||
                                   escaped.contains('"') ||
                                   escaped.contains('\n') ||
                                   escaped.contains('\r');
                escaped.replace("\"", "\"\"");
                if (needsQuotes || escaped.startsWith(' ') || escaped.endsWith(' ')) {
                    escaped = "\"" + escaped + "\"";
                }
                return escaped;
            };

            out << escapeCSV(website) << ","
                << escapeCSV(username) << ","
                << escapeCSV(account) << ","
                << escapeCSV(password) << ","
                << escapeCSV(notes) << "\n";

            exportedCount++;
        }
    }

    file.close();
    qDebug() << "导出成功，共导出" << exportedCount << "条记录";

    if (exportedCount == 0) {
        statusBar->showMessage("没有选中任何记录");
        return false;
    }

    return true;
}

void MainWindow::exportPasswords()
{
    if (operationInProgress) {
        QMessageBox::warning(this, "警告", "当前有操作正在进行，请等待完成");
        return;
    }

    // 第一步：弹出对话框让用户选择导出类型
    QDialog exportTypeDialog(this);
    exportTypeDialog.setWindowTitle("选择导出类型");
    exportTypeDialog.resize(300, 150);

    QVBoxLayout *dialogLayout = new QVBoxLayout(&exportTypeDialog);

    QGroupBox *groupBox = new QGroupBox("请选择导出版本:");
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

    QRadioButton *unencryptedButton = new QRadioButton("未保密版（密码以明文显示）");
    QRadioButton *encryptedButton = new QRadioButton("保密版（密码以加密形式显示）");

    // 默认选中未保密版
    unencryptedButton->setChecked(true);

    groupLayout->addWidget(unencryptedButton);
    groupLayout->addWidget(encryptedButton);
    groupBox->setLayout(groupLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    dialogLayout->addWidget(groupBox);
    dialogLayout->addWidget(buttonBox);
    exportTypeDialog.setLayout(dialogLayout);

    connect(buttonBox, &QDialogButtonBox::accepted, &exportTypeDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &exportTypeDialog, &QDialog::reject);

    if (exportTypeDialog.exec() != QDialog::Accepted) {
        statusBar->showMessage("已取消导出");
        return;
    }

    // 确定用户选择的导出类型
    bool exportEncrypted = encryptedButton->isChecked();

    // 第二步：选择保存文件位置
    QString fileName = QFileDialog::getSaveFileName(this, "导出密码",
                                                    "passwords_backup.csv",
                                                    "CSV文件 (*.csv)");
    if (fileName.isEmpty()) {
        statusBar->showMessage("已取消导出");
        return;
    }

    if (multiSelectMode) {
        // 获取选中的行
        QList<int> selectedRows;
        for (int i = 0; i < model->rowCount(); ++i) {
            QStandardItem* checkItem = model->item(i, 7);
            if (checkItem && checkItem->checkState() == Qt::Checked) {
                selectedRows.append(i);
            }
        }

        if (selectedRows.isEmpty()) {
            QMessageBox::warning(this, "警告", "没有选中任何记录，将导出全部记录");
            startExportOperation(fileName, exportEncrypted);
        } else {
            startExportSelectedOperation(fileName, selectedRows, exportEncrypted);
        }
    } else {
        startExportOperation(fileName, exportEncrypted);
    }
}

void MainWindow::importPasswords()
{
    if (operationInProgress) {
        QMessageBox::warning(this, "警告", "当前有操作正在进行，请等待完成");
        return;
    }

    QString fileName = QFileDialog::getOpenFileName(this, "导入密码",
                                                    "", "CSV文件 (*.csv)");
    if (fileName.isEmpty()) {
        statusBar->showMessage("已取消导入");
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认导入",
                                  "导入将添加新记录，重复记录不会覆盖。确定要继续吗?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        startImportOperation(fileName);
    } else {
        statusBar->showMessage("已取消导入");
    }
}

void MainWindow::startImportOperation(const QString &filename)
{
    operationInProgress = true;

    // 禁用相关按钮
    importButton->setEnabled(false);
    exportButton->setEnabled(false);

    // 创建进度对话框（如果需要）
    if (!progressDialog) {
        createProgressDialog();
    }

    // 创建线程和工作器
    workerThread = new QThread(this);
    worker = new ImportExportWorker();
    worker->setOperationType(ImportExportWorker::ImportOperation);
    worker->setFilename(filename);
    worker->moveToThread(workerThread);

    // 连接信号
    connect(workerThread, &QThread::started, worker, &ImportExportWorker::startOperation);
    connect(worker, &ImportExportWorker::progressChanged, this, &MainWindow::onImportProgress);
    connect(worker, &ImportExportWorker::operationFinished, this, &MainWindow::onOperationFinished);
    connect(worker, &ImportExportWorker::errorOccurred, this, &MainWindow::onOperationError);
    connect(workerThread, &QThread::finished, worker, &ImportExportWorker::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 启动线程
    workerThread->start();

    // 显示进度对话框
    progressDialog->setWindowTitle("导入数据");
    progressDialog->setLabelText("正在导入数据，请稍候...");
    progressDialog->show();
}

void MainWindow::startExportOperation(const QString &filename, bool exportEncrypted)
{
    operationInProgress = true;

    // 禁用相关按钮
    importButton->setEnabled(false);
    exportButton->setEnabled(false);

    // 创建进度对话框（如果需要）
    if (!progressDialog) {
        createProgressDialog();
    }

    // 创建线程和工作器
    workerThread = new QThread(this);
    worker = new ImportExportWorker();
    worker->setOperationType(ImportExportWorker::ExportOperation);
    worker->setFilename(filename);
    worker->setExportEncrypted(exportEncrypted);  // 设置导出类型
    worker->moveToThread(workerThread);

    // 连接信号
    connect(workerThread, &QThread::started, worker, &ImportExportWorker::startOperation);
    connect(worker, &ImportExportWorker::progressChanged, this, &MainWindow::onExportProgress);
    connect(worker, &ImportExportWorker::operationFinished, this, &MainWindow::onOperationFinished);
    connect(worker, &ImportExportWorker::errorOccurred, this, &MainWindow::onOperationError);
    connect(workerThread, &QThread::finished, worker, &ImportExportWorker::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 启动线程
    workerThread->start();

    // 显示进度对话框
    QString exportType = exportEncrypted ? "保密版" : "未保密版";
    progressDialog->setWindowTitle(QString("导出数据 (%1)").arg(exportType));
    progressDialog->setLabelText(QString("正在导出数据(%1)，请稍候...").arg(exportType));
    progressDialog->show();
}

void MainWindow::startExportSelectedOperation(const QString &filename, const QList<int> &selectedRows, bool exportEncrypted)
{
    operationInProgress = true;

    // 禁用相关按钮
    importButton->setEnabled(false);
    exportButton->setEnabled(false);

    // 创建进度对话框（如果需要）
    if (!progressDialog) {
        createProgressDialog();
    }

    // 创建线程和工作器
    workerThread = new QThread(this);
    worker = new ImportExportWorker();
    worker->setOperationType(ImportExportWorker::ExportSelectedOperation);
    worker->setFilename(filename);
    worker->setSelectedRows(selectedRows);
    worker->setExportEncrypted(exportEncrypted);  // 设置导出类型
    worker->moveToThread(workerThread);

    // 连接信号
    connect(workerThread, &QThread::started, worker, &ImportExportWorker::startOperation);
    connect(worker, &ImportExportWorker::progressChanged, this, &MainWindow::onExportProgress);
    connect(worker, &ImportExportWorker::operationFinished, this, &MainWindow::onOperationFinished);
    connect(worker, &ImportExportWorker::errorOccurred, this, &MainWindow::onOperationError);
    connect(workerThread, &QThread::finished, worker, &ImportExportWorker::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 启动线程
    workerThread->start();

    // 显示进度对话框
    QString exportType = exportEncrypted ? "保密版" : "未保密版";
    progressDialog->setWindowTitle(QString("导出选中的数据 (%1)").arg(exportType));
    progressDialog->setLabelText(QString("正在导出选中的数据(%1)，请稍候...").arg(exportType));
    progressDialog->show();
}

void MainWindow::onImportProgress(int percent, const QString &message)
{
    if (progressDialog) {
        progressDialog->setValue(percent);
        progressDialog->setLabelText(message);
    }
    statusBar->showMessage(message);
}

void MainWindow::onExportProgress(int percent, const QString &message)
{
    if (progressDialog) {
        progressDialog->setValue(percent);
        progressDialog->setLabelText(message);
    }
    statusBar->showMessage(message);
}

void MainWindow::onOperationFinished(bool success, const QString &message)
{
    operationInProgress = false;

    // 启用按钮
    importButton->setEnabled(true);
    exportButton->setEnabled(true);

    // 关闭进度对话框
    if (progressDialog) {
        progressDialog->reset();
        progressDialog->hide();
    }

    if (success) {
        // 重新加载数据
        loadPasswords();

        QMessageBox::information(this, "成功", message);
        statusBar->showMessage(message);
    } else {
        QMessageBox::warning(this, "失败", message);
        statusBar->showMessage("操作失败");
    }

    // 清理线程
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
        worker = nullptr;
    }
}

void MainWindow::onOperationError(const QString &error)
{
    operationInProgress = false;

    // 启用按钮
    importButton->setEnabled(true);
    exportButton->setEnabled(true);

    // 关闭进度对话框
    if (progressDialog) {
        progressDialog->reset();
        progressDialog->hide();
    }

    QMessageBox::critical(this, "错误", error);
    statusBar->showMessage("操作出错");

    // 清理线程
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
        worker = nullptr;
    }
}

void MainWindow::cancelOperation()
{
    if (operationInProgress && workerThread && workerThread->isRunning()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认取消",
                                      "确定要取消当前操作吗？",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // 终止线程
            workerThread->terminate();
            workerThread->wait();

            operationInProgress = false;

            // 启用按钮
            importButton->setEnabled(true);
            exportButton->setEnabled(true);

            // 隐藏进度对话框
            if (progressDialog) {
                progressDialog->reset();
                progressDialog->hide();
            }

            statusBar->showMessage("操作已取消");
            QMessageBox::information(this, "提示", "操作已取消");
        } else {
            // 重新显示进度对话框
            if (progressDialog) {
                progressDialog->show();
            }
        }
    }
}

void MainWindow::testDatabase()
{
    if (Database::instance().init()) {
        QMessageBox::information(this, "数据库测试", "数据库连接成功！");
        statusBar->showMessage("数据库连接正常");
    } else {
        QMessageBox::warning(this, "数据库测试", "数据库连接失败！");
        statusBar->showMessage("数据库连接失败");
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "关于密码管理器",
                       "个人密码管理器 v1.4（账号增强版）\n\n"
                       "基于Qt开发的密码管理工具\n"
                       "新增功能：\n"
                       "  • 增加账号字段（6-20位）\n"
                       "  • 账号和密码格式验证\n"
                       "  • 账号加密存储\n"
                       "  • 多线程导入导出\n"
                       "  • 操作进度显示\n"
                       "  • 支持取消长时间操作\n"
                       "  • 批量操作优化\n"
                       "  • 导出保密版/未保密版选择\n"
                       "  • 密码加密存储与导出\n\n"
                       "课程设计项目 - Qt应用程序开发");
}
