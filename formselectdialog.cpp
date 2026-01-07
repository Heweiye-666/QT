#include "formselectdialog.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QAbstractItemView>

FormSelectDialog::FormSelectDialog(const QList<int> &formIds,
                                   const QList<QString> &formNames,
                                   const QList<int> &selectedIds,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("选择搜索表单");
    resize(300, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 标题
    QLabel *titleLabel = new QLabel("请选择要搜索的表单:");
    mainLayout->addWidget(titleLabel);

    // 列表控件
    listWidget = new QListWidget(this);
    listWidget->setSelectionMode(QAbstractItemView::MultiSelection);

    // 添加表单项目
    for (int i = 0; i < formIds.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(formNames[i], listWidget);
        item->setData(Qt::UserRole, formIds[i]);
        item->setCheckState(Qt::Unchecked);
        idToItem[formIds[i]] = item;

        // 如果该表单在选中列表中，则勾选
        if (selectedIds.contains(formIds[i])) {
            item->setCheckState(Qt::Checked);
        }
    }

    mainLayout->addWidget(listWidget);

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    selectAllButton = new QPushButton("全选", this);
    clearAllButton = new QPushButton("清空", this);
    okButton = new QPushButton("确定", this);
    cancelButton = new QPushButton("取消", this);

    buttonLayout->addWidget(selectAllButton);
    buttonLayout->addWidget(clearAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(selectAllButton, &QPushButton::clicked, this, &FormSelectDialog::onSelectAllClicked);
    connect(clearAllButton, &QPushButton::clicked, this, &FormSelectDialog::onClearAllClicked);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QList<int> FormSelectDialog::selectedFormIds() const
{
    QList<int> selectedIds;
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem *item = listWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            selectedIds.append(item->data(Qt::UserRole).toInt());
        }
    }
    return selectedIds;
}

void FormSelectDialog::onSelectAllClicked()
{
    for (int i = 0; i < listWidget->count(); ++i) {
        listWidget->item(i)->setCheckState(Qt::Checked);
    }
}

void FormSelectDialog::onClearAllClicked()
{
    for (int i = 0; i < listWidget->count(); ++i) {
        listWidget->item(i)->setCheckState(Qt::Unchecked);
    }
}
