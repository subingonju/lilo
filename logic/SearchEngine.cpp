#include "SearchEngine.h"
#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

SearchEngine& SearchEngine::instance() {
    static SearchEngine inst;
    return inst;
}

SearchEngine::SearchEngine(QObject* parent) : QObject(parent) {}

QList<Transaction> SearchEngine::search(const SearchCriteria& c) {
    QString sql =
        "SELECT t.id, t.account_id, t.type, t.amount, t.category, t.description, "
        "t.created_at, a.name "
        "FROM transactions t JOIN accounts a ON t.account_id = a.id WHERE 1=1 ";

    if (c.userId > 0)       sql += " AND a.user_id = :uid";
    if (c.accountId > 0)    sql += " AND t.account_id = :aid";
    if (!c.keyword.isEmpty()) sql += " AND (t.description LIKE :kw OR t.category LIKE :kw2)";
    if (!c.startDate.isEmpty()) sql += " AND t.created_at >= :sd";
    if (!c.endDate.isEmpty())   sql += " AND t.created_at <= :ed";
    if (c.minAmount >= 0)   sql += " AND t.amount >= :mina";
    if (c.maxAmount >= 0)   sql += " AND t.amount <= :maxa";
    if (!c.category.isEmpty() && c.category != "All") sql += " AND t.category = :cat";
    if (!c.transactionType.isEmpty()) sql += " AND t.type = :ttype";
    sql += " ORDER BY t.created_at DESC";

    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(sql);

    if (c.userId > 0)         q.bindValue(":uid",  c.userId);
    if (c.accountId > 0)      q.bindValue(":aid",  c.accountId);
    if (!c.keyword.isEmpty()) {
        q.bindValue(":kw",  "%" + c.keyword + "%");
        q.bindValue(":kw2", "%" + c.keyword + "%");
    }
    if (!c.startDate.isEmpty()) q.bindValue(":sd",   c.startDate);
    if (!c.endDate.isEmpty())   q.bindValue(":ed",   c.endDate + " 23:59:59");
    if (c.minAmount >= 0)     q.bindValue(":mina", c.minAmount);
    if (c.maxAmount >= 0)     q.bindValue(":maxa", c.maxAmount);
    if (!c.category.isEmpty() && c.category != "All") q.bindValue(":cat", c.category);
    if (!c.transactionType.isEmpty()) q.bindValue(":ttype", c.transactionType);

    QList<Transaction> results;
    if (!q.exec()) {
        qWarning() << "SearchEngine::search failed:" << q.lastError().text();
        return results;
    }

    while (q.next()) {
        Transaction t;
        t.id          = q.value(0).toInt();
        t.accountId   = q.value(1).toInt();
        t.type        = q.value(2).toString();
        t.amount      = q.value(3).toDouble();
        t.category    = q.value(4).toString();
        t.description = q.value(5).toString();
        t.createdAt   = q.value(6).toString();
        t.accountName = q.value(7).toString();
        results.append(t);
    }
    return results;
}
