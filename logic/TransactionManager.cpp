#include "TransactionManager.h"
#include "DatabaseManager.h"
#include "AuthManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDebug>

TransactionManager& TransactionManager::instance() {
    static TransactionManager inst;
    return inst;
}

TransactionManager::TransactionManager(QObject* parent) : QObject(parent) {}

bool TransactionManager::deposit(int accountId, double amount, const QString& category,
                                  const QString& description) {
    auto db = DatabaseManager::instance().database();
    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("INSERT INTO transactions (account_id, type, amount, category, description) "
              "VALUES (:aid, '입금', :amt, :cat, :desc)");
    q.bindValue(":aid",  accountId);
    q.bindValue(":amt",  amount);
    q.bindValue(":cat",  category);
    q.bindValue(":desc", description);

    if (!q.exec()) {
        qWarning() << "deposit insert failed:" << q.lastError().text();
        db.rollback();
        return false;
    }

    q.prepare("UPDATE accounts SET balance = balance + :amt WHERE id = :aid");
    q.bindValue(":amt", amount);
    q.bindValue(":aid", accountId);

    if (!q.exec()) {
        qWarning() << "deposit balance update failed:" << q.lastError().text();
        db.rollback();
        return false;
    }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "INSERT", "transactions", q.lastInsertId().toInt(),
        QString("type=입금, amount=%1, category=%2").arg(amount).arg(category));
    emit transactionsChanged(accountId);
    return true;
}

bool TransactionManager::withdraw(int accountId, double amount, const QString& category,
                                   const QString& description) {
    auto db = DatabaseManager::instance().database();

    QSqlQuery check(db);
    check.prepare("SELECT balance FROM accounts WHERE id = :aid");
    check.bindValue(":aid", accountId);
    if (!check.exec() || !check.next() || check.value(0).toDouble() < amount) {
        qWarning() << "Insufficient balance for withdrawal";
        return false;
    }

    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("INSERT INTO transactions (account_id, type, amount, category, description) "
              "VALUES (:aid, '출금', :amt, :cat, :desc)");
    q.bindValue(":aid",  accountId);
    q.bindValue(":amt",  amount);
    q.bindValue(":cat",  category);
    q.bindValue(":desc", description);

    if (!q.exec()) {
        db.rollback();
        return false;
    }

    q.prepare("UPDATE accounts SET balance = balance - :amt WHERE id = :aid");
    q.bindValue(":amt", amount);
    q.bindValue(":aid", accountId);

    if (!q.exec()) {
        db.rollback();
        return false;
    }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "INSERT", "transactions", q.lastInsertId().toInt(),
        QString("type=출금, amount=%1, category=%2").arg(amount).arg(category));
    emit transactionsChanged(accountId);
    return true;
}

bool TransactionManager::updateTransaction(int txId, double newAmount,
                                            const QString& category,
                                            const QString& description) {
    auto db = DatabaseManager::instance().database();

    QSqlQuery fetch(db);
    fetch.prepare("SELECT account_id FROM transactions WHERE id = :id");
    fetch.bindValue(":id", txId);
    if (!fetch.exec() || !fetch.next()) return false;
    int accountId = fetch.value(0).toInt();

    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("UPDATE transactions SET amount = :amt, category = :cat, description = :desc "
              "WHERE id = :id");
    q.bindValue(":amt",  newAmount);
    q.bindValue(":cat",  category);
    q.bindValue(":desc", description);
    q.bindValue(":id",   txId);

    if (!q.exec() || !recalculateBalance(accountId)) {
        db.rollback();
        return false;
    }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "UPDATE", "transactions", txId,
        QString("amount=%1, category=%2").arg(newAmount).arg(category));
    emit transactionsChanged(accountId);
    return true;
}

bool TransactionManager::deleteTransaction(int txId) {
    auto db = DatabaseManager::instance().database();

    QSqlQuery fetch(db);
    fetch.prepare("SELECT account_id FROM transactions WHERE id = :id");
    fetch.bindValue(":id", txId);
    if (!fetch.exec() || !fetch.next()) return false;
    int accountId = fetch.value(0).toInt();

    if (!db.transaction()) return false;

    QSqlQuery q(db);
    q.prepare("DELETE FROM transactions WHERE id = :id");
    q.bindValue(":id", txId);

    if (!q.exec() || !recalculateBalance(accountId)) {
        db.rollback();
        return false;
    }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "DELETE", "transactions", txId);
    emit transactionsChanged(accountId);
    return true;
}

bool TransactionManager::recalculateBalance(int accountId) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(R"(
        UPDATE accounts SET balance = (
            SELECT COALESCE(
                SUM(CASE WHEN type='입금' THEN amount ELSE -amount END), 0)
            FROM transactions WHERE account_id = :aid
        ) WHERE id = :aid2
    )");
    q.bindValue(":aid",  accountId);
    q.bindValue(":aid2", accountId);
    if (!q.exec()) {
        qWarning() << "recalculateBalance failed:" << q.lastError().text();
        return false;
    }
    return true;
}
