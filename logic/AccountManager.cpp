#include "AccountManager.h"
#include "DatabaseManager.h"
#include "AuthManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

AccountManager& AccountManager::instance() {
    static AccountManager inst;
    return inst;
}

AccountManager::AccountManager(QObject* parent) : QObject(parent) {}

bool AccountManager::createAccount(int userId, const QString& name, const QString& number,
                                   const QString& type, const QString& currency) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("INSERT INTO accounts (user_id, name, number, type, currency) "
              "VALUES (:uid, :name, :num, :type, :cur)");
    q.bindValue(":uid",  userId);
    q.bindValue(":name", name.trimmed());
    q.bindValue(":num",  number.trimmed());
    q.bindValue(":type", type);
    q.bindValue(":cur",  currency);

    if (!q.exec()) {
        qWarning() << "createAccount failed:" << q.lastError().text();
        return false;
    }
    int newId = q.lastInsertId().toInt();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(DatabaseManager::instance().database(), uid,
        "INSERT", "accounts", newId, QString("name=%1, type=%2").arg(name.trimmed(), type));
    emit accountsChanged();
    return true;
}

bool AccountManager::updateAccount(int id, const QString& name, const QString& type,
                                   const QString& currency) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("UPDATE accounts SET name = :name, type = :type, currency = :cur WHERE id = :id");
    q.bindValue(":name", name.trimmed());
    q.bindValue(":type", type);
    q.bindValue(":cur",  currency);
    q.bindValue(":id",   id);

    if (!q.exec()) {
        qWarning() << "updateAccount failed:" << q.lastError().text();
        return false;
    }
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(DatabaseManager::instance().database(), uid,
        "UPDATE", "accounts", id, QString("name=%1, type=%2").arg(name.trimmed(), type));
    emit accountsChanged();
    return true;
}

bool AccountManager::deleteAccount(int id) {
    auto db = DatabaseManager::instance().database();
    db.transaction();

    QSqlQuery q(db);

    // transfers 에는 ON DELETE CASCADE 가 없으므로 먼저 수동 삭제
    q.prepare("DELETE FROM transfers WHERE from_account_id = :id OR to_account_id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "deleteAccount (transfers) failed:" << q.lastError().text();
        db.rollback();
        return false;
    }

    q.prepare("DELETE FROM accounts WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "deleteAccount (accounts) failed:" << q.lastError().text();
        db.rollback();
        return false;
    }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "DELETE", "accounts", id);
    emit accountsChanged();
    return true;
}

Account AccountManager::getAccount(int id) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare("SELECT id, user_id, name, number, balance, type, currency, created_at "
              "FROM accounts WHERE id = :id");
    q.bindValue(":id", id);

    Account a;
    if (q.exec() && q.next()) {
        a.id        = q.value(0).toInt();
        a.userId    = q.value(1).toInt();
        a.name      = q.value(2).toString();
        a.number    = q.value(3).toString();
        a.balance   = q.value(4).toDouble();
        a.type      = q.value(5).toString();
        a.currency  = q.value(6).toString();
        a.createdAt = q.value(7).toString();
    }
    return a;
}
