#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QWidget>
#include <QList>
#include <QColor>

class NotificationManager : public QObject {
    Q_OBJECT
public:
    static NotificationManager& instance();

    void initialize(QWidget* parent = nullptr);
    void checkBudgets(int userId);

private:
    explicit NotificationManager(QObject* parent = nullptr);
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;

    void notify(const QString& title, const QString& message, const QColor& accentColor);
    void positionToast(QWidget* toast);
    void repositionToasts();

    QSystemTrayIcon*  m_tray        = nullptr;
    QWidget*          m_mainWindow  = nullptr;
    QList<QWidget*>   m_activeToasts;
};
