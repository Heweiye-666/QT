#ifndef IMPORTEXPORTWORKER_H
#define IMPORTEXPORTWORKER_H

#include <QObject>
#include <QThread>
#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "database.h"

class ImportExportWorker : public QObject
{
    Q_OBJECT

public:
    enum OperationType {
        ImportOperation,
        ExportOperation,
        ExportSelectedOperation
    };

    explicit ImportExportWorker(QObject *parent = nullptr);

    void setOperationType(OperationType type) { m_operationType = type; }
    void setFilename(const QString &filename) { m_filename = filename; }
    void setSelectedRows(const QList<int> &selectedRows) { m_selectedRows = selectedRows; }
    void setExportEncrypted(bool encrypted) { m_exportEncrypted = encrypted; }

public slots:
    void startOperation();

signals:
    void progressChanged(int percent, const QString &message);
    void operationFinished(bool success, const QString &message);
    void errorOccurred(const QString &error);

private:
    OperationType m_operationType;
    QString m_filename;
    QList<int> m_selectedRows;
    bool m_exportEncrypted;

    bool importFromCSV();
    bool exportToCSV();
    bool exportSelectedToCSV(const QList<int> &selectedRows);
};

#endif // IMPORTEXPORTWORKER_H
