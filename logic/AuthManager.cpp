#include "AuthManager.h"
#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDebug>

AuthManager& AuthManager::instance() {
    static AuthManager inst;
    return inst;
}

AuthManager::AuthManager(QObject* parent) : QObject(parent) {}

QString AuthManager::hashPassword(const QString& password) {
    return QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();
}

bool AuthManager::login(const QString& username, const QString& password) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("SELECT id FROM users WHERE username = :u AND password_hash = :p");
    q.bindValue(":u", username.trimmed());
    q.bindValue(":p", hashPassword(password));

    if (!q.exec()) {
        qWarning() << "login query failed:" << q.lastError().text();
        return false;
    }
    if (!q.next()) return false;

    m_userId   = q.value(0).toInt();
    m_username = username.trimmed();
    DatabaseManager::logAudit(DatabaseManager::instance().database(), m_userId,
        "LOGIN", "users", m_userId, QString("username=%1").arg(m_username));
    emit loggedIn(m_userId, m_username);
    return true;
}

bool AuthManager::registerUser(const QString& username, const QString& password) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("INSERT INTO users (username, password_hash) VALUES (:u, :p)");
    q.bindValue(":u", username.trimmed());
    q.bindValue(":p", hashPassword(password));

    if (!q.exec()) {
        qWarning() << "registerUser failed:" << q.lastError().text();
        return false;
    }
    int newUserId = q.lastInsertId().toInt();
    DatabaseManager::logAudit(DatabaseManager::instance().database(), newUserId,
        "REGISTER", "users", newUserId, QString("username=%1").arg(username.trimmed()));
    return true;
}

void AuthManager::logout() {
    m_userId   = 0;
    m_username.clear();
    emit loggedOut();
}
