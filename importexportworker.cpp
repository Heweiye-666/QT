#include "importexportworker.h"
#include "encryption.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QStandardItemModel>
#include <QSqlDatabase>
#include <QSqlQuery>

              // CSV字段转义函数（复制自database.cpp，稍作修改）
              static QString escapeCSVField(const QString &field)
{
    QString escaped = field;

    // 检查是否需要引号（包含逗号、双引号或换行符）
    bool needsQuotes = escaped.contains(',') ||
                       escaped.contains('"') ||
                       escaped.contains('\n') ||
                       escaped.contains('\r');

    // 转义双引号
    escaped.replace("\"", "\"\"");

    // 如果需要引号，则添加引号
    if (needsQuotes || escaped.startsWith(' ') || escaped.endsWith(' ')) {
        escaped = "\"" + escaped + "\"";
    }

    return escaped;
}

// CSV行解析函数（复制自database.cpp）
static QStringList parseCSVLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool inQuotes = false;

    for (int i = 0; i < line.length(); ++i) {
        QChar ch = line[i];

        if (ch == '"') {
            if (i + 1 < line.length() && line[i + 1] == '"') {
                // 双引号转义
                field.append('"');
                i++; // 跳过下一个引号
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.append(field);
            field.clear();
        } else {
            field.append(ch);
        }
    }

    // 添加最后一个字段
    fields.append(field);

    return fields;
}

ImportExportWorker::ImportExportWorker(QObject *parent)
    : QObject(parent)
    , m_operationType(ImportOperation)
    , m_exportEncrypted(false)  // 默认导出未保密版
    , m_formId(-1)  // 默认-1表示所有表单
{
}

void ImportExportWorker::startOperation()
{
    qDebug() << "Worker thread started:" << QThread::currentThread()->objectName();

    bool success = false;
    QString message;

    try {
        switch (m_operationType) {
        case ImportOperation:
            success = importFromCSV();
            message = success ? "导入完成" : "导入失败";
            break;
        case ExportOperation:
            success = exportToCSV();
            message = success ? "导出完成" : "导出失败";
            break;
        case ExportSelectedOperation:
            success = exportSelectedToCSV(m_selectedRows);
            message = success ? "导出完成" : "导出失败";
            break;
        }
    } catch (const std::exception &e) {
        QString error = QString("操作异常: %1").arg(e.what());
        emit errorOccurred(error);
        return;
    } catch (...) {
        emit errorOccurred("未知异常");
        return;
    }

    emit operationFinished(success, message);
}

bool ImportExportWorker::importFromCSV()
{
    emit progressChanged(0, "开始导入...");

    QFile file(m_filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法打开文件: %1").arg(m_filename));
        return false;
    }

    QTextStream in(&file);
    QString firstLine = in.readLine();

    // 检查并跳过UTF-8 BOM
    if (!firstLine.startsWith("Website,Username,Account,Password,Notes")) {
        in.seek(0);
        in.read(3); // 跳过BOM
        firstLine = in.readLine(); // 读取标题行
    }

    // 获取文件大小用于计算进度
    file.seek(0);
    qint64 fileSize = file.size();
    qint64 processedSize = 0;

    // 统计导入数量
    int importedCount = 0;
    int lineNumber = 0;

    // 批量插入，每10条提交一次
    const int BATCH_SIZE = 10;
    QSqlDatabase::database().transaction();

    while (!in.atEnd()) {
        lineNumber++;
        QString line = in.readLine().trimmed();
        processedSize += line.length() + 2; // +2 for CRLF

        if (line.isEmpty()) continue;

        // 计算进度
        int progress = fileSize > 0 ? static_cast<int>((processedSize * 100) / fileSize) : 0;
        emit progressChanged(progress, QString("正在导入第 %1 行...").arg(lineNumber));

        // 处理CSV行
        QStringList fields = parseCSVLine(line);

        if (fields.size() >= 5) {
            QString website = fields[0].trimmed();
            QString username = fields[1].trimmed();
            QString account = fields[2].trimmed();  // 账号字段
            QString password = fields[3].trimmed();  // CSV中的明文密码
            QString notes = fields[4].trimmed();

            if (!website.isEmpty() && !username.isEmpty()) {
                // 对账号和密码进行加密后再存储
                QString encryptedAccount = Encryption::encrypt(account);
                QString encryptedPassword = Encryption::encrypt(password);

                // 使用指定的表单ID（如果为-1则使用默认表单）
                int targetFormId = m_formId;
                if (targetFormId <= 0) {
                    // 获取第一个表单作为默认
                    auto forms = Database::instance().getAllForms();
                    if (!forms.isEmpty()) {
                        targetFormId = forms.first().id;
                    } else {
                        // 如果没有表单，创建一个默认表单
                        Database::instance().addForm("默认表单");
                        forms = Database::instance().getAllForms();
                        if (!forms.isEmpty()) {
                            targetFormId = forms.first().id;
                        }
                    }
                }

                Database::instance().addPassword(targetFormId, website, username,
                                                 encryptedAccount, encryptedPassword, notes);
                importedCount++;

                // 每批提交一次
                if (importedCount % BATCH_SIZE == 0) {
                    QSqlDatabase::database().commit();
                    QSqlDatabase::database().transaction();
                }
            }
        }

        // 处理事件循环，避免界面卡死
        QCoreApplication::processEvents();
    }

    // 提交最后一批
    QSqlDatabase::database().commit();

    file.close();

    emit progressChanged(100, QString("导入完成，共导入 %1 条记录").arg(importedCount));
    return importedCount > 0;
}

bool ImportExportWorker::exportToCSV()
{
    emit progressChanged(0, "开始导出...");

    QFile file(m_filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法创建文件: %1").arg(m_filename));
        return false;
    }

    QTextStream out(&file);

    // 写入UTF-8 BOM
    out << "\xEF\xBB\xBF";

    // 写入表头，增加Account列
    out << "Website,Username,Account,Password,Notes\n";

    // 获取指定表单的密码（如果m_formId为-1则获取所有）
    auto passwords = Database::instance().getAllPasswords(m_formId);
    int totalCount = passwords.size();
    int exportedCount = 0;

    for (const auto &pwd : passwords) {
        exportedCount++;

        // 更新进度
        int progress = totalCount > 0 ? static_cast<int>((exportedCount * 100) / totalCount) : 0;
        QString exportType = m_exportEncrypted ? "保密版" : "未保密版";
        emit progressChanged(progress, QString("正在导出(%1)第 %2/%3 条...").arg(exportType).arg(exportedCount).arg(totalCount));

        // 对CSV特殊字符进行转义
        QString escapedWebsite = escapeCSVField(pwd.website);
        QString escapedUsername = escapeCSVField(pwd.username);

        // 根据导出类型决定账号和密码字段
        QString accountField;
        QString passwordField;

        if (m_exportEncrypted) {
            // 保密版：直接使用数据库中的加密账号和密码
            accountField = pwd.account;
            passwordField = pwd.password;
        } else {
            // 未保密版：解密账号和密码
            accountField = Encryption::decrypt(pwd.account);
            passwordField = Encryption::decrypt(pwd.password);
        }

        QString escapedAccount = escapeCSVField(accountField);
        QString escapedPassword = escapeCSVField(passwordField);
        QString escapedNotes = escapeCSVField(pwd.notes);

        out << escapedWebsite << ","
            << escapedUsername << ","
            << escapedAccount << ","   // 增加账号列
            << escapedPassword << ","
            << escapedNotes << "\n";

        // 处理事件循环，避免界面卡死
        if (exportedCount % 10 == 0) {
            QCoreApplication::processEvents();
        }
    }

    file.close();

    QString exportType = m_exportEncrypted ? "保密版" : "未保密版";
    QString formInfo = (m_formId >= 0) ?
                           QString("表单ID:%1").arg(m_formId) :
                           "所有表单";
    emit progressChanged(100, QString("导出(%1)完成，共导出 %2 条记录 (%3)").arg(exportType).arg(exportedCount).arg(formInfo));
    return exportedCount > 0;
}

bool ImportExportWorker::exportSelectedToCSV(const QList<int> &selectedRows)
{
    emit progressChanged(0, "开始导出选中的记录...");

    QFile file(m_filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("无法创建文件: %1").arg(m_filename));
        return false;
    }

    QTextStream out(&file);

    // 写入UTF-8 BOM
    out << "\xEF\xBB\xBF";

    // 写入表头，增加Account列
    out << "Website,Username,Account,Password,Notes\n";

    // 获取指定表单的密码（如果m_formId为-1则获取所有）
    auto passwords = Database::instance().getAllPasswords(m_formId);
    int totalCount = selectedRows.size();
    int exportedCount = 0;

    for (int row : selectedRows) {
        if (row < 0 || row >= passwords.size()) {
            continue;
        }

        const auto &pwd = passwords[row];
        exportedCount++;

        // 更新进度
        int progress = totalCount > 0 ? static_cast<int>((exportedCount * 100) / totalCount) : 0;
        QString exportType = m_exportEncrypted ? "保密版" : "未保密版";
        emit progressChanged(progress, QString("正在导出(%1)第 %2/%3 条...").arg(exportType).arg(exportedCount).arg(totalCount));

        // 对CSV特殊字符进行转义
        QString escapedWebsite = escapeCSVField(pwd.website);
        QString escapedUsername = escapeCSVField(pwd.username);

        // 根据导出类型决定账号和密码字段
        QString accountField;
        QString passwordField;

        if (m_exportEncrypted) {
            // 保密版：直接使用数据库中的加密账号和密码
            accountField = pwd.account;
            passwordField = pwd.password;
        } else {
            // 未保密版：解密账号和密码
            accountField = Encryption::decrypt(pwd.account);
            passwordField = Encryption::decrypt(pwd.password);
        }

        QString escapedAccount = escapeCSVField(accountField);
        QString escapedPassword = escapeCSVField(passwordField);
        QString escapedNotes = escapeCSVField(pwd.notes);

        out << escapedWebsite << ","
            << escapedUsername << ","
            << escapedAccount << ","   // 增加账号列
            << escapedPassword << ","
            << escapedNotes << "\n";

        // 处理事件循环，避免界面卡死
        if (exportedCount % 10 == 0) {
            QCoreApplication::processEvents();
        }
    }

    file.close();

    QString exportType = m_exportEncrypted ? "保密版" : "未保密版";
    QString formInfo = (m_formId >= 0) ?
                           QString("表单ID:%1").arg(m_formId) :
                           "所有表单";
    emit progressChanged(100, QString("导出(%1)完成，共导出 %2 条记录 (%3)").arg(exportType).arg(exportedCount).arg(formInfo));
    return exportedCount > 0;
}
