#include "database.h"
#include "encryption.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QFileInfo>
#include <QStandardPaths>

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
    // 获取默认数据库连接
    db = QSqlDatabase::database(); // 使用main.cpp中已经打开的连接

    if (!db.isOpen()) {
        qDebug() << "数据库未打开，尝试重新连接";
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/passwords.db";
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            qDebug() << "仍然无法打开数据库:" << db.lastError().text();
        } else {
            qDebug() << "数据库重新连接成功";
        }
    }
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
        qDebug() << "数据库未打开，尝试重新连接";
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/passwords.db";
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath);

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

    // 创建表单表
    QString sql = "CREATE TABLE IF NOT EXISTS forms ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "name TEXT NOT NULL, "
                  "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                  "UNIQUE(name))";

    if (!query.exec(sql)) {
        qDebug() << "创建表单表失败:" << query.lastError().text();
        return false;
    }

    // 创建密码表，增加account字段和form_id字段
    sql = "CREATE TABLE IF NOT EXISTS passwords ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT, "
          "form_id INTEGER NOT NULL, "
          "website TEXT NOT NULL, "
          "username TEXT NOT NULL, "
          "account TEXT, "  // 新增：账号字段
          "password TEXT NOT NULL, "
          "notes TEXT, "
          "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
          "FOREIGN KEY(form_id) REFERENCES forms(id) ON DELETE CASCADE, "
          "UNIQUE(form_id, website, username, account))";  // 修改：将form_id加入唯一约束

    if (!query.exec(sql)) {
        qDebug() << "创建密码表失败:" << query.lastError().text();
        return false;
    }

    // 创建索引以提高搜索性能
    query.exec("CREATE INDEX IF NOT EXISTS idx_passwords_form_id ON passwords(form_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_passwords_website ON passwords(website)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_passwords_username ON passwords(username)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_passwords_account ON passwords(account)");

    // 检查是否有表单，如果没有则创建一个默认表单
    query.exec("SELECT COUNT(*) FROM forms");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("INSERT INTO forms (name) VALUES ('默认表单')");
        qDebug() << "创建默认表单";
    }

    // 确保至少有一个表单ID为1的默认表单
    query.exec("SELECT id FROM forms WHERE name = '默认表单'");
    if (!query.next()) {
        query.exec("INSERT INTO forms (name) VALUES ('默认表单')");
    }

    return true;
}

// 表单相关方法
bool Database::addForm(const QString &name)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO forms (name) VALUES (:name)");
    query.bindValue(":name", name);

    if (!query.exec()) {
        qDebug() << "添加表单失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::updateForm(int id, const QString &name)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE forms SET name = :name WHERE id = :id");
    query.bindValue(":name", name);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "更新表单失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::deleteForm(int id)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    // 首先检查是否有其他表单，不能删除最后一个表单
    QSqlQuery checkQuery(db);
    checkQuery.exec("SELECT COUNT(*) FROM forms");
    if (checkQuery.next() && checkQuery.value(0).toInt() <= 1) {
        qDebug() << "不能删除最后一个表单";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM forms WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "删除表单失败:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QList<FormEntry> Database::getAllForms()
{
    QList<FormEntry> forms;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return forms;
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT id, name, created_at FROM forms ORDER BY name")) {
        qDebug() << "查询表单失败:" << query.lastError().text();
        return forms;
    }

    while (query.next()) {
        FormEntry form;
        form.id = query.value(0).toInt();
        form.name = query.value(1).toString();
        form.created_at = query.value(2).toString();
        forms.append(form);
    }

    // 如果没有表单，创建一个默认表单
    if (forms.isEmpty()) {
        addForm("默认表单");
        return getAllForms(); // 递归调用，现在应该有表单了
    }

    return forms;
}

FormEntry Database::getFormById(int id)
{
    FormEntry form;
    form.id = -1;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return form;
    }

    QSqlQuery query(db);
    query.prepare("SELECT id, name, created_at FROM forms WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "查询表单失败:" << query.lastError().text();
        return form;
    }

    if (query.next()) {
        form.id = query.value(0).toInt();
        form.name = query.value(1).toString();
        form.created_at = query.value(2).toString();
    }

    return form;
}

// 密码相关方法
bool Database::addPassword(int form_id, const QString &website, const QString &username,
                           const QString &account, const QString &password,
                           const QString &notes)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    // 如果form_id为-1，使用第一个表单
    if (form_id <= 0) {
        auto forms = getAllForms();
        if (!forms.isEmpty()) {
            form_id = forms.first().id;
        } else {
            qDebug() << "没有可用的表单";
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR IGNORE INTO passwords (form_id, website, username, account, password, notes) "
                  "VALUES (:form_id, :website, :username, :account, :password, :notes)");
    query.bindValue(":form_id", form_id);
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

bool Database::updatePassword(int id, int form_id, const QString &website,
                              const QString &username, const QString &account,
                              const QString &password, const QString &notes)
{
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);

    // 首先检查新的form_id、website、username和account组合是否已存在（排除自身）
    query.prepare("SELECT id FROM passwords WHERE form_id = :form_id AND website = :website AND username = :username AND account = :account AND id != :id");
    query.bindValue(":form_id", form_id);
    query.bindValue(":website", website);
    query.bindValue(":username", username);
    query.bindValue(":account", account);  // 新增：账号条件
    query.bindValue(":id", id);

    if (query.exec() && query.next()) {
        qDebug() << "新的表单、网站、用户名和账号组合已存在";
        return false;
    }

    // 如果新的组合不存在，则更新记录
    query.prepare("UPDATE passwords SET form_id = :form_id, website = :website, username = :username, "
                  "account = :account, password = :password, notes = :notes WHERE id = :id");
    query.bindValue(":form_id", form_id);
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

QList<PasswordEntry> Database::getAllPasswords(int form_id)
{
    QList<PasswordEntry> entries;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return entries;
    }

    QSqlQuery query(db);

    if (form_id >= 0) {
        // 查询指定表单的密码，按照添加顺序（ID递增）排序
        query.prepare("SELECT id, form_id, website, username, account, password, notes FROM passwords WHERE form_id = :form_id ORDER BY id ASC");
        query.bindValue(":form_id", form_id);
    } else {
        // 查询所有表单的密码，按照添加顺序（ID递增）排序
        query.prepare("SELECT id, form_id, website, username, account, password, notes FROM passwords ORDER BY id ASC");
    }

    if (!query.exec()) {
        qDebug() << "查询密码失败:" << query.lastError().text();
        return entries;
    }

    while (query.next()) {
        PasswordEntry entry;
        entry.id = query.value(0).toInt();
        entry.form_id = query.value(1).toInt();
        entry.website = query.value(2).toString();
        entry.username = query.value(3).toString();
        entry.account = query.value(4).toString();  // 新增：获取账号
        entry.password = query.value(5).toString();
        entry.notes = query.value(6).toString();
        entries.append(entry);
    }

    return entries;
}

QList<PasswordEntry> Database::searchPasswords(const QString &keyword, const QList<int> &form_ids)
{
    QList<PasswordEntry> entries;

    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return entries;
    }

    QSqlQuery query(db);

    QString sql = "SELECT id, form_id, website, username, account, password, notes FROM passwords ";
    QString whereClause = "WHERE (website LIKE :keyword OR username LIKE :keyword OR account LIKE :keyword OR notes LIKE :keyword) ";

    if (!form_ids.isEmpty()) {
        // 构建IN子句
        QStringList placeholders;
        for (int i = 0; i < form_ids.size(); ++i) {
            placeholders.append(QString(":form_id_%1").arg(i));
        }
        whereClause += "AND form_id IN (" + placeholders.join(",") + ") ";
    }

    sql += whereClause + "ORDER BY id ASC";  // 修改：按照添加顺序排序

    query.prepare(sql);
    query.bindValue(":keyword", "%" + keyword + "%");

    // 绑定表单ID参数
    for (int i = 0; i < form_ids.size(); ++i) {
        query.bindValue(QString(":form_id_%1").arg(i), form_ids[i]);
    }

    if (!query.exec()) {
        qDebug() << "搜索密码失败:" << query.lastError().text();
        return entries;
    }

    while (query.next()) {
        PasswordEntry entry;
        entry.id = query.value(0).toInt();
        entry.form_id = query.value(1).toInt();
        entry.website = query.value(2).toString();
        entry.username = query.value(3).toString();
        entry.account = query.value(4).toString();  // 新增：获取账号
        entry.password = query.value(5).toString();
        entry.notes = query.value(6).toString();
        entries.append(entry);
    }

    return entries;
}

bool Database::exportToCSV(const QString &filename, int form_id)
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

    auto passwords = getAllPasswords(form_id);
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

bool Database::importFromCSV(const QString &filename, int form_id)
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
                addPassword(form_id, website, username, encryptedAccount, encryptedPassword, notes);
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

