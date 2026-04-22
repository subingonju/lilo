#include "TransferManager.h"
#include "DatabaseManager.h"
#include "AuthManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QDebug>

TransferManager& TransferManager::instance() {
    static TransferManager inst;
    return inst;
}

TransferManager::TransferManager(QObject* parent) : QObject(parent) {}

bool TransferManager::transfer(int fromId, int toId, double amount, const QString& memo) {
    if (fromId == toId || amount <= 0.0) return false;

    auto db = DatabaseManager::instance().database();

    QSqlQuery check(db);
    check.prepare("SELECT balance FROM accounts WHERE id = :id");
    check.bindValue(":id", fromId);
    if (!check.exec() || !check.next() || check.value(0).toDouble() < amount) {
        qWarning() << "Insufficient balance for transfer";
        return false;
    }

    if (!db.transaction()) return false;

    QSqlQuery q(db);

    q.prepare("UPDATE accounts SET balance = balance - :amt WHERE id = :id");
    q.bindValue(":amt", amount);
    q.bindValue(":id",  fromId);
    if (!q.exec()) { db.rollback(); return false; }

    q.prepare("UPDATE accounts SET balance = balance + :amt WHERE id = :id");
    q.bindValue(":amt", amount);
    q.bindValue(":id",  toId);
    if (!q.exec()) { db.rollback(); return false; }

    q.prepare("INSERT INTO transfers (from_account_id, to_account_id, amount, memo) "
              "VALUES (:from, :to, :amt, :memo)");
    q.bindValue(":from", fromId);
    q.bindValue(":to",   toId);
    q.bindValue(":amt",  amount);
    q.bindValue(":memo", memo);
    if (!q.exec()) { db.rollback(); return false; }

    q.prepare("INSERT INTO transactions (account_id, type, amount, category, description) "
              "VALUES (:aid, '출금', :amt, '이체', :desc)");
    q.bindValue(":aid",  fromId);
    q.bindValue(":amt",  amount);
    q.bindValue(":desc", memo.isEmpty() ? "이체 출금" : memo);
    if (!q.exec()) { db.rollback(); return false; }

    q.prepare("INSERT INTO transactions (account_id, type, amount, category, description) "
              "VALUES (:aid, '입금', :amt, '이체', :desc)");
    q.bindValue(":aid",  toId);
    q.bindValue(":amt",  amount);
    q.bindValue(":desc", memo.isEmpty() ? "이체 입금" : memo);
    if (!q.exec()) { db.rollback(); return false; }

    db.commit();
    int uid = AuthManager::instance().currentUserId();
    DatabaseManager::logAudit(db, uid, "INSERT", "transfers", q.lastInsertId().toInt(),
        QString("from=%1, to=%2, amount=%3").arg(fromId).arg(toId).arg(amount));
    emit transferCompleted(fromId, toId);
    return true;
}
