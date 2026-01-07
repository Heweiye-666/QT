#ifndef FORMSELECTDIALOG_H
#define FORMSELECTDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMap>

class FormSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormSelectDialog(const QList<int> &formIds,
                              const QList<QString> &formNames,
                              const QList<int> &selectedIds = QList<int>(),
                              QWidget *parent = nullptr);

    QList<int> selectedFormIds() const;

private slots:
    void onSelectAllClicked();
    void onClearAllClicked();

private:
    QListWidget *listWidget;
    QPushButton *selectAllButton;
    QPushButton *clearAllButton;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QMap<int, QListWidgetItem*> idToItem;
};

#endif // FORMSELECTDIALOG_H
