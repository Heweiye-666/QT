#ifndef FORMTABWIDGET_H
#define FORMTABWIDGET_H

#include <QWidget>
#include <QTabBar>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QList>
#include <QInputDialog>
#include <QMessageBox>
#include <QMap>

class FormTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FormTabWidget(QWidget *parent = nullptr);

    void addForm(int id, const QString &name, bool setCurrent = false);
    void removeForm(int id);
    void updateForm(int id, const QString &name);
    void setCurrentForm(int id);
    int currentFormId() const;
    QString currentFormName() const;
    QList<int> allFormIds() const;
    QList<int> selectedFormIds() const;
    void setSelectedForms(const QList<int> &formIds);
    void clearSelection();
    void clear();  // 新增：清空所有标签

signals:
    void formAdded(int id, const QString &name);
    void formRemoved(int id);
    void formRenamed(int id, const QString &newName);
    void currentFormChanged(int id);
    void formSelectionChanged(const QList<int> &selectedIds);

public slots:
    void onAddFormClicked();
    void onTabDoubleClicked(int index);
    void onTabCloseRequested(int index);
    void onTabCurrentChanged(int index);

private:
    QTabBar *tabBar;
    QPushButton *addButton;
    QMap<int, int> formIdToTabIndex;  // 表单ID到标签页索引的映射
    QMap<int, int> tabIndexToFormId;  // 标签页索引到表单ID的映射
    QList<int> selectedForms;  // 选中的表单ID列表
};

#endif // FORMTABWIDGET_H
