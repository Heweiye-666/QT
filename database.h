#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <QString>

struct PasswordEntry {
    int id;  // 新增：主键ID
    QString website;
    QString username;
    QString account;  // 新增：账号字段
    QString password;  // 加密后的
    QString notes;
};

class Database
{
public:
    static Database& instance();

    bool init();
    bool addPassword(const QString &website, const QString &username,
                     const QString &account, const QString &password, const QString &notes);
    bool updatePassword(int id, const QString &website,
                        const QString &username, const QString &account,
                        const QString &password, const QString &notes);
    bool deletePassword(int id);
    bool deletePasswordByWebsite(const QString &website);
    QList<PasswordEntry> getAllPasswords();
    QList<PasswordEntry> searchPasswords(const QString &keyword);
    bool exportToCSV(const QString &filename);
    bool importFromCSV(const QString &filename);

private:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase db;
    bool createTables();
};

#endif // DATABASE_H
