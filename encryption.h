#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <QString>

class Encryption
{
public:
    static QString encrypt(const QString &plainText);
    static QString decrypt(const QString &encryptedText);
};

#endif // ENCRYPTION_H
