#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <QString>

              // 表单结构体
              struct FormEntry {
    int id;
    QString name;
    QString created_at;
};

struct PasswordEntry {
    int id;  // 新增：主键ID
    int form_id;  // 新增：表单ID
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

    // 表单相关方法
    bool addForm(const QString &name);
    bool updateForm(int id, const QString &name);
    bool deleteForm(int id);
    QList<FormEntry> getAllForms();
    FormEntry getFormById(int id);

    // 密码相关方法
    bool addPassword(int form_id, const QString &website, const QString &username,
                     const QString &account, const QString &password, const QString &notes);
    bool updatePassword(int id, int form_id, const QString &website,
                        const QString &username, const QString &account,
                        const QString &password, const QString &notes);
    bool deletePassword(int id);
    bool deletePasswordByWebsite(const QString &website);
    QList<PasswordEntry> getAllPasswords(int form_id = -1);  // -1 表示所有表单
    QList<PasswordEntry> searchPasswords(const QString &keyword, const QList<int> &form_ids = QList<int>());
    bool exportToCSV(const QString &filename, int form_id = -1);
    bool importFromCSV(const QString &filename, int form_id = 1);  // 默认导入到第一个表单

private:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase db;
    bool createTables();
};

#endif // DATABASE_H
