#include "TransactionModel.h"
#include "AccountModel.h"
#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QColor>
#include <QDebug>

const QStringList TransactionModel::s_headers = {
    "ID", "날짜", "계좌", "유형", "카테고리", "금액", "설명"
};

TransactionModel::TransactionModel(QObject* parent) : QAbstractTableModel(parent) {}

int TransactionModel::rowCount(const QModelIndex&) const { return m_transactions.size(); }
int TransactionModel::columnCount(const QModelIndex&) const { return s_headers.size(); }

QVariant TransactionModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_transactions.size())
        return {};

    const Transaction& t = m_transactions[index.row()];

    if (role == Qt::ForegroundRole) {
        if (index.column() == 3 || index.column() == 5) {
            if (t.type == "입금") return QColor(0x05, 0x96, 0x69);
            if (t.type == "출금") return QColor(0xDC, 0x26, 0x26);
        }
        return {};
    }

    if (role == Qt::TextAlignmentRole && index.column() == 5)
        return int(Qt::AlignRight | Qt::AlignVCenter);

    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};

    switch (index.column()) {
    case 0: return t.id;
    case 1: return t.createdAt;
    case 2: return t.accountName;
    case 3: return t.type;
    case 4: return t.category;
    case 5: return AccountModel::formatKRW(t.amount);
    case 6: return t.description;
    }
    return {};
}

QVariant TransactionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return {};
    return s_headers.value(section);
}

static Transaction rowToTransaction(QSqlQuery& q) {
    Transaction t;
    t.id          = q.value(0).toInt();
    t.accountId   = q.value(1).toInt();
    t.type        = q.value(2).toString();
    t.amount      = q.value(3).toDouble();
    t.category    = q.value(4).toString();
    t.description = q.value(5).toString();
    t.createdAt   = q.value(6).toString();
    t.accountName = q.value(7).toString();
    return t;
}

static const char* kSelectSQL =
    "SELECT t.id, t.account_id, t.type, t.amount, t.category, t.description, t.created_at, "
    "a.name as account_name "
    "FROM transactions t JOIN accounts a ON t.account_id = a.id ";

void TransactionModel::loadByAccount(int accountId) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QString(kSelectSQL) + "WHERE t.account_id = :aid ORDER BY t.created_at DESC");
    q.bindValue(":aid", accountId);
    execAndLoad(q);
}

void TransactionModel::loadByUser(int userId) {
    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(QString(kSelectSQL) + "WHERE a.user_id = :uid ORDER BY t.created_at DESC");
    q.bindValue(":uid", userId);
    execAndLoad(q);
}

void TransactionModel::setTransactions(const QList<Transaction>& list) {
    beginResetModel();
    m_transactions = list;
    endResetModel();
}

void TransactionModel::execAndLoad(QSqlQuery& q) {
    beginResetModel();
    m_transactions.clear();
    if (!q.exec()) {
        qWarning() << "TransactionModel load failed:" << q.lastError().text();
    } else {
        while (q.next())
            m_transactions.append(rowToTransaction(q));
    }
    endResetModel();
}

Transaction TransactionModel::transactionAt(int row) const {
    if (row < 0 || row >= m_transactions.size()) return {};
    return m_transactions[row];
}
