#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {}

bool DatabaseManager::initialize(const QString& path) {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = path.isEmpty()
        ? QCoreApplication::applicationDirPath() + "/account_manager.db"
        : path;
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "DB open failed:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode=WAL");
    q.exec("PRAGMA foreign_keys=ON");

    return createTables();
}

bool DatabaseManager::createTables() {
    QSqlQuery q(m_db);
    const QStringList ddl = {
        R"(CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS accounts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            number TEXT NOT NULL UNIQUE,
            balance REAL NOT NULL DEFAULT 0.0,
            type TEXT NOT NULL DEFAULT 'Checking',
            currency TEXT NOT NULL DEFAULT 'KRW',
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
            type TEXT NOT NULL,
            amount REAL NOT NULL,
            category TEXT NOT NULL DEFAULT 'Other',
            description TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS transfers (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_account_id INTEGER NOT NULL REFERENCES accounts(id),
            to_account_id INTEGER NOT NULL REFERENCES accounts(id),
            amount REAL NOT NULL,
            memo TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))",
        R"(CREATE TABLE IF NOT EXISTS budgets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            category TEXT NOT NULL,
            monthly_limit REAL NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),
            UNIQUE(user_id, category)
        ))",
        R"(CREATE TABLE IF NOT EXISTS audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            action TEXT NOT NULL,
            table_name TEXT NOT NULL,
            record_id INTEGER,
            detail TEXT DEFAULT '',
            created_at TEXT NOT NULL DEFAULT (datetime('now','localtime'))
        ))"
    };

    for (const QString& sql : ddl) {
        if (!q.exec(sql)) {
            qCritical() << "Table creation failed:" << q.lastError().text();
            return false;
        }
    }

    // 카테고리 데이터 정합성 보정: 잘못된 유형-카테고리 조합을 '기타'로 교정
    q.exec(R"(
        UPDATE transactions SET category='기타'
        WHERE type='입금'
          AND category NOT IN ('급여','부수입','용돈','금융수익(이자/배당)','이체','기타')
    )");
    q.exec(R"(
        UPDATE transactions SET category='기타'
        WHERE type='출금'
          AND category NOT IN ('식비','교통','쇼핑','주거/통신','의료/건강','여가','교육','이체','기타')
    )");

    return true;
}

void DatabaseManager::logAudit(const QSqlDatabase& db, int userId,
                                const QString& action, const QString& tableName,
                                int recordId, const QString& detail)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO audit_log (user_id, action, table_name, record_id, detail) "
              "VALUES (:uid, :act, :tbl, :rid, :det)");
    q.bindValue(":uid", userId);
    q.bindValue(":act", action);
    q.bindValue(":tbl", tableName);
    q.bindValue(":rid", recordId);
    q.bindValue(":det", detail);
    if (!q.exec())
        qWarning() << "audit_log insert failed:" << q.lastError().text();
}
