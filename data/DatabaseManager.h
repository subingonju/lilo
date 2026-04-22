#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();
    bool initialize(const QString& path = QString());
    QSqlDatabase database() const { return m_db; }

    static void logAudit(const QSqlDatabase& db, int userId,
                         const QString& action, const QString& tableName,
                         int recordId, const QString& detail = {});

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool createTables();
    QSqlDatabase m_db;
};
