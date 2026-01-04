#include "database.h"
#include "encryption.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QFileInfo>

// CSV字段转义函数
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

// CSV行解析函数
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

Database::Database()
{
    // 设置数据库连接名，避免冲突
    static int connectionCount = 0;
    QString connectionName = QString("password_db_%1").arg(connectionCount++);

    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    QString dbPath = QDir::currentPath() + "/passwords.db";
    db.setDatabaseName(dbPath);
    qDebug() << "数据库路径:" << dbPath;
}

Database::~Database()
{
    if (db.isOpen()) {
        db.close();
    }
    // 移除数据库连接
    QSqlDatabase::removeDatabase(db.connectionName());
}

Database& Database::instance()
{
    static Database instance;
    return instance;
}

bool Database::init()
{
    // 检查SQLite驱动是否可用
    if (!QSqlDatabase::isDriverAvailable("QSQLITE")) {
        qDebug() << "SQLite驱动不可用";
        return false;
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "无法打开数据库:" << db.lastError().text();
            return false;
        }
    }

    qDebug() << "数据库已成功打开";
    return createTables();
}

bool Database::createTables()
{
    QSqlQuery query(db);

    // 创建密码表，增加account字段
    QString sql = "CREATE TABLE IF NOT EXISTS passwords ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "website TEXT NOT NULL, "
                  "username TEXT NOT NULL, "
                  "account TEXT, "  // 新增：账号字段
                  "password TEXT NOT NULL, "
                  "notes TEXT, "
                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                  "UNIQUE(website, username, account))";  // 修改：将account加入唯一约束

    if (!query.exec(sql)) {
        qDebug() << "创建表失败:" << query.lastError().text();
        return false;
    }

    // 创建索引以提高搜索性能，增加账号索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_website ON passwords(website)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_username ON passwords(username)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_account ON passwords(account)");  // 新增：账号索引

    return true;
}

bool Database::addPassword(const QString &website, const QString &username,
                           const QString &account, const QString &password,
                           const QString &notes)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO passwords (website, username, account, password, notes) "
                  "VALUES (:website, :username, :account, :password, :notes)");
    query.bindValue(":website", website);
    query.bindValue(":username", username);
    query.bindValue(":account", account);  // 新增：绑定账号
    query.bindValue(":password", password);
    query.bindValue(":notes", notes);

    if (!query.exec()) {
        qDebug() << "添加密码失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::updatePassword(int id, const QString &website,
                              const QString &username, const QString &account,
                              const QString &password, const QString &notes)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);

    // 首先检查新的website、username和account组合是否已存在（排除自身）
    query.prepare("SELECT id FROM passwords WHERE website = :website AND username = :username AND account = :account AND id != :id");
    query.bindValue(":website", website);
    query.bindValue(":username", username);
    query.bindValue(":account", account);  // 新增：账号条件
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        qDebug() << "新的网站、用户名和账号组合已存在";
        return false;
    }

    // 如果新的组合不存在，则更新记录
    query.prepare("UPDATE passwords SET website = :website, username = :username, "
                  "account = :account, password = :password, notes = :notes WHERE id = :id");
    query.bindValue(":website", website);
    query.bindValue(":username", username);
    query.bindValue(":account", account);  // 新增：更新账号
    query.bindValue(":password", password);
    query.bindValue(":notes", notes);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "更新密码失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::deletePassword(int id)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM passwords WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "删除密码失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::deletePasswordByWebsite(const QString &website)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM passwords WHERE website = :website");
    query.bindValue(":website", website);

    if (!query.exec()) {
        qDebug() << "删除密码失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QList<PasswordEntry> Database::getAllPasswords()
{
    QList<PasswordEntry> entries;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return entries;
    }

    QSqlQuery query(db);
    // 修改：包含account字段
    if (!query.exec("SELECT id, website, username, account, password, notes FROM passwords ORDER BY website")) {
        qDebug() << "查询密码失败:" << query.lastError().text();
        return entries;
    }

    while (query.next()) {
        PasswordEntry entry;
        entry.id = query.value(0).toInt();
        entry.website = query.value(1).toString();
        entry.username = query.value(2).toString();
        entry.account = query.value(3).toString();  // 新增：获取账号
        entry.password = query.value(4).toString();
        entry.notes = query.value(5).toString();
        entries.append(entry);
    }

    return entries;
}

QList<PasswordEntry> Database::searchPasswords(const QString &keyword)
{
    QList<PasswordEntry> entries;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return entries;
    }

    QSqlQuery query(db);
    // 修改：包含account字段，并增加账号搜索
    query.prepare("SELECT id, website, username, account, password, notes FROM passwords "
                  "WHERE website LIKE :keyword OR username LIKE :keyword OR account LIKE :keyword OR notes LIKE :keyword "
                  "ORDER BY website");
    query.bindValue(":keyword", "%" + keyword + "%");

    if (!query.exec()) {
        qDebug() << "搜索密码失败:" << query.lastError().text();
        return entries;
    }

    while (query.next()) {
        PasswordEntry entry;
        entry.id = query.value(0).toInt();
        entry.website = query.value(1).toString();
        entry.username = query.value(2).toString();
        entry.account = query.value(3).toString();  // 新增：获取账号
        entry.password = query.value(4).toString();
        entry.notes = query.value(5).toString();
        entries.append(entry);
    }

    return entries;
}

bool Database::exportToCSV(const QString &filename)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件:" << filename;
        return false;
    }

    QTextStream out(&file);

    // 写入UTF-8 BOM以确保正确识别编码（Windows系统需要）
    out << "\xEF\xBB\xBF";

    // 写入表头，增加Account列
    out << "Website,Username,Account,Password,Notes\n";

    auto passwords = getAllPasswords();
    for (const auto &pwd : passwords) {
        // 对CSV特殊字符进行转义
        QString escapedWebsite = escapeCSVField(pwd.website);
        QString escapedUsername = escapeCSVField(pwd.username);

        // 解密账号和密码后再进行转义
        QString decryptedAccount = Encryption::decrypt(pwd.account);
        QString escapedDecryptedAccount = escapeCSVField(decryptedAccount);

        QString decryptedPassword = Encryption::decrypt(pwd.password);
        QString escapedPassword = escapeCSVField(decryptedPassword);

        QString escapedNotes = escapeCSVField(pwd.notes);

        out << escapedWebsite << ","
            << escapedUsername << ","
            << escapedDecryptedAccount << ","  // 使用解密后的账号
            << escapedPassword << ","
            << escapedNotes << "\n";
    }

    file.close();
    qDebug() << "导出成功，共导出" << passwords.size() << "条记录";
    return true;
}

bool Database::importFromCSV(const QString &filename)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件:" << filename;
        return false;
    }

    QTextStream in(&file);
    // 检查并跳过UTF-8 BOM
    QString firstLine = in.readLine();
    if (!firstLine.startsWith("Website,Username,Account,Password,Notes")) {
        // 可能是第一行包含BOM，尝试读取下一行
        in.seek(0);
        in.read(3); // 跳过BOM
        firstLine = in.readLine(); // 读取标题行
    }

    int importedCount = 0;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // 使用改进的CSV解析
        QStringList fields = parseCSVLine(line);

        if (fields.size() >= 5) {
            QString website = fields[0].trimmed();
            QString username = fields[1].trimmed();
            QString account = fields[2].trimmed();  // 新增：获取账号
            QString password = fields[3].trimmed();  // CSV中的明文密码
            QString notes = fields[4].trimmed();

            if (!website.isEmpty() && !username.isEmpty()) {
                // 对账号和密码进行加密后再存储
                QString encryptedAccount = Encryption::encrypt(account);
                QString encryptedPassword = Encryption::encrypt(password);
                addPassword(website, username, encryptedAccount, encryptedPassword, notes);
                importedCount++;
            }
        } else {
            qDebug() << "CSV格式错误，行:" << line;
            qDebug() << "解析出的字段数:" << fields.size();
        }
    }

    file.close();
    qDebug() << "成功导入" << importedCount << "条记录";
    return importedCount > 0;
}
