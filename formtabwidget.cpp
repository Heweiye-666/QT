#include "formtabwidget.h"
#include <QStyle>
#include <QListWidget>
#include <QAbstractItemView>

FormTabWidget::FormTabWidget(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 创建标签栏
    tabBar = new QTabBar(this);
    tabBar->setTabsClosable(true);
    tabBar->setMovable(true);
    tabBar->setExpanding(false);

    // 创建添加按钮
    addButton = new QPushButton("+", this);
    addButton->setFixedSize(30, tabBar->height());
    addButton->setToolTip("添加新表单");

    layout->addWidget(tabBar, 1);
    layout->addWidget(addButton);

    // 连接信号
    connect(addButton, &QPushButton::clicked, this, &FormTabWidget::onAddFormClicked);
    connect(tabBar, &QTabBar::tabBarDoubleClicked, this, &FormTabWidget::onTabDoubleClicked);
    connect(tabBar, &QTabBar::tabCloseRequested, this, &FormTabWidget::onTabCloseRequested);
    connect(tabBar, &QTabBar::currentChanged, this, &FormTabWidget::onTabCurrentChanged);
}

void FormTabWidget::clear()
{
    // QTabBar没有clear()方法，使用循环移除所有标签页
    while (tabBar->count() > 0) {
        tabBar->removeTab(0);
    }
    formIdToTabIndex.clear();
    tabIndexToFormId.clear();
    selectedForms.clear();
}

void FormTabWidget::addForm(int id, const QString &name, bool setCurrent)
{
    int index = tabBar->addTab(name);
    formIdToTabIndex[id] = index;
    tabIndexToFormId[index] = id;

    if (setCurrent) {
        tabBar->setCurrentIndex(index);
    }
}

void FormTabWidget::removeForm(int id)
{
    if (formIdToTabIndex.contains(id)) {
        int index = formIdToTabIndex[id];
        tabBar->removeTab(index);
        formIdToTabIndex.remove(id);
        tabIndexToFormId.remove(index);

        // 更新映射
        QMap<int, int> newTabIndexToFormId;
        for (auto it = tabIndexToFormId.begin(); it != tabIndexToFormId.end(); ++it) {
            int oldIndex = it.key();
            int formId = it.value();
            int newIndex = oldIndex > index ? oldIndex - 1 : oldIndex;
            formIdToTabIndex[formId] = newIndex;
            newTabIndexToFormId[newIndex] = formId;
        }
        tabIndexToFormId = newTabIndexToFormId;

        // 从选中列表中移除
        selectedForms.removeAll(id);
        emit formSelectionChanged(selectedForms);
    }
}

void FormTabWidget::updateForm(int id, const QString &name)
{
    if (formIdToTabIndex.contains(id)) {
        int index = formIdToTabIndex[id];
        tabBar->setTabText(index, name);
    }
}

void FormTabWidget::setCurrentForm(int id)
{
    if (formIdToTabIndex.contains(id)) {
        tabBar->setCurrentIndex(formIdToTabIndex[id]);
    }
}

int FormTabWidget::currentFormId() const
{
    int index = tabBar->currentIndex();
    if (tabIndexToFormId.contains(index)) {
        return tabIndexToFormId[index];
    }
    return -1;
}

QString FormTabWidget::currentFormName() const
{
    int index = tabBar->currentIndex();
    if (index >= 0) {
        return tabBar->tabText(index);
    }
    return QString();
}

QList<int> FormTabWidget::allFormIds() const
{
    return formIdToTabIndex.keys();
}

QList<int> FormTabWidget::selectedFormIds() const
{
    return selectedForms;
}

void FormTabWidget::setSelectedForms(const QList<int> &formIds)
{
    selectedForms = formIds;
    // 可以在这里更新标签的视觉样式来表示选中状态
    emit formSelectionChanged(selectedForms);
}

void FormTabWidget::clearSelection()
{
    selectedForms.clear();
    emit formSelectionChanged(selectedForms);
}

void FormTabWidget::onAddFormClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "添加表单",
                                         "请输入表单名称:",
                                         QLineEdit::Normal,
                                         "新表单",
                                         &ok);
    if (ok && !name.trimmed().isEmpty()) {
        emit formAdded(-1, name.trimmed());  // -1 表示新表单，需要由上层创建
    }
}

void FormTabWidget::onTabDoubleClicked(int index)
{
    if (tabIndexToFormId.contains(index)) {
        int formId = tabIndexToFormId[index];
        QString oldName = tabBar->tabText(index);

        bool ok;
        QString newName = QInputDialog::getText(this, "重命名表单",
                                                "请输入新名称:",
                                                QLineEdit::Normal,
                                                oldName,
                                                &ok);
        if (ok && !newName.trimmed().isEmpty() && newName != oldName) {
            emit formRenamed(formId, newName.trimmed());
        }
    }
}

void FormTabWidget::onTabCloseRequested(int index)
{
    if (tabIndexToFormId.contains(index)) {
        int formId = tabIndexToFormId[index];
        QString formName = tabBar->tabText(index);

        // 确认对话框
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要删除表单 '%1' 吗？\n此操作将删除该表单中的所有密码记录！").arg(formName),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            emit formRemoved(formId);
        }
    }
}

void FormTabWidget::onTabCurrentChanged(int index)
{
    if (index >= 0 && tabIndexToFormId.contains(index)) {
        emit currentFormChanged(tabIndexToFormId[index]);
    }
}
