#include "encryption.h"
#include <QByteArray>
#include <QDebug>

QString Encryption::encrypt(const QString &plainText)
{
    if (plainText.isEmpty()) return QString();

    // 简单的Base64编码
    QByteArray data = plainText.toUtf8();
    QString result = QString::fromLatin1(data.toBase64());

    qDebug() << "加密: '" << plainText << "' -> Base64: '" << result << "'";
    return result;
}

QString Encryption::decrypt(const QString &encryptedText)
{
    if (encryptedText.isEmpty()) return QString();

    qDebug() << "解密: Base64字符串 '" << encryptedText << "'";

    try {
        QByteArray data = QByteArray::fromBase64(encryptedText.toLatin1());
        if (data.isEmpty()) {
            qDebug() << "Base64解码失败，可能不是有效的Base64字符串";
            // 如果不是Base64字符串，直接返回原字符串
            // 这有助于处理以前未加密的数据
            return encryptedText;
        }

        QString result = QString::fromUtf8(data);
        qDebug() << "解码结果: '" << result << "'";
        return result;
    } catch (...) {
        qDebug() << "解密异常";
        return encryptedText;
    }
}
