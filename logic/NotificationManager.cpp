#include "NotificationManager.h"
#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>
#include <QStyle>
#include <QDebug>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

NotificationManager& NotificationManager::instance() {
    static NotificationManager inst;
    return inst;
}

NotificationManager::NotificationManager(QObject* parent) : QObject(parent) {}

void NotificationManager::initialize(QWidget* parent) {
    if (m_tray) return;
    m_mainWindow = parent;
    m_tray = new QSystemTrayIcon(
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation),
        parent ? static_cast<QObject*>(parent) : this);
    m_tray->setToolTip("계정 관리자");
    m_tray->show();
}

void NotificationManager::checkBudgets(int userId) {
    if (!m_tray) return;

    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(R"(
        SELECT b.category, b.monthly_limit,
               COALESCE(SUM(t.amount), 0) as spent
        FROM budgets b
        LEFT JOIN accounts a ON a.user_id = b.user_id
        LEFT JOIN transactions t ON t.account_id = a.id
            AND t.type = '출금'
            AND t.category = b.category
            AND strftime('%Y-%m', t.created_at) = strftime('%Y-%m', 'now', 'localtime')
        WHERE b.user_id = :uid
        GROUP BY b.id, b.category, b.monthly_limit
    )");
    q.bindValue(":uid", userId);

    if (!q.exec()) {
        qWarning() << "checkBudgets failed:" << q.lastError().text();
        return;
    }

    while (q.next()) {
        QString cat   = q.value(0).toString();
        double  limit = q.value(1).toDouble();
        double  spent = q.value(2).toDouble();

        if (limit <= 0.0) continue;
        double ratio = spent / limit;

        if (spent > limit) {
            notify("예산 초과",
                   QString("%1 예산을 초과했습니다! (지출: %2 / 한도: %3)")
                       .arg(cat).arg(spent, 0, 'f', 0).arg(limit, 0, 'f', 0),
                   QColor("#e24b4a"));
        } else if (spent == limit) {
            notify("예산 도달",
                   QString("%1 예산에 도달했습니다! (지출: %2 / 한도: %3)")
                       .arg(cat).arg(spent, 0, 'f', 0).arg(limit, 0, 'f', 0),
                   QColor("#FFB800"));
        } else if (ratio >= 0.8) {
            notify("예산 경고",
                   QString("%1 예산이 %2%에 도달했습니다 (%3 / %4)")
                       .arg(cat).arg(int(ratio * 100)).arg(spent, 0, 'f', 0).arg(limit, 0, 'f', 0),
                   QColor("#F59E0B"));
        }
    }
}

void NotificationManager::notify(const QString& title, const QString& message,
                                  const QColor& accentColor) {
    if (!m_mainWindow) return;

    auto* toast = new QFrame(m_mainWindow);
    toast->setAttribute(Qt::WA_StyledBackground, true);
    toast->setAttribute(Qt::WA_DeleteOnClose);
    toast->setFixedWidth(300);
    toast->setStyleSheet(QString(
        "QFrame#toast {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E5E7EB;"
        "  border-left: 4px solid %1;"
        "  border-radius: 10px;"
        "}"
        "QLabel { background: transparent; color: #111827; }").arg(accentColor.name()));
    toast->setObjectName("toast");

    auto* hl = new QHBoxLayout(toast);
    hl->setContentsMargins(12, 12, 14, 12);
    hl->setSpacing(10);

    // 아이콘: 컬러 원형 배지
    QString iconChar = title.contains("초과") ? "✕" : "!";
    auto* iconLbl = new QLabel(iconChar, toast);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setFixedSize(26, 26);
    iconLbl->setStyleSheet(QString(
        "color:#FFFFFF; font-size:11px; font-weight:900;"
        "background:%1; border-radius:13px;").arg(accentColor.name()));

    auto* contentLay = new QVBoxLayout;
    contentLay->setSpacing(2);

    auto* titleLbl = new QLabel(title, toast);
    titleLbl->setStyleSheet(QString(
        "color:%1; font-size:11px; font-weight:700;").arg(accentColor.name()));

    auto* msgLbl = new QLabel(message, toast);
    msgLbl->setStyleSheet("color:#6B7280; font-size:10px;");
    msgLbl->setWordWrap(true);
    msgLbl->setMaximumWidth(220);

    contentLay->addWidget(titleLbl);
    contentLay->addWidget(msgLbl);

    hl->addWidget(iconLbl, 0, Qt::AlignTop);
    hl->addLayout(contentLay);
    hl->addStretch();

    toast->adjustSize();
    positionToast(toast);
    m_activeToasts.append(toast);
    toast->raise();
    toast->show();

    // 3초 후 페이드아웃 (자식 위젯은 QGraphicsOpacityEffect 사용)
    auto* timer = new QTimer(toast);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, toast]() {
        auto* effect = new QGraphicsOpacityEffect(toast);
        toast->setGraphicsEffect(effect);

        auto* anim = new QPropertyAnimation(effect, "opacity", toast);
        anim->setDuration(500);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, [this, toast]() {
            m_activeToasts.removeOne(toast);
            toast->deleteLater();
            repositionToasts();
        });
        anim->start();
    });
    timer->start(3000);
}

void NotificationManager::positionToast(QWidget* toast) {
    if (!m_mainWindow) return;
    const int margin   = 16;
    const int stackGap = 8;

    int yOffset = 0;
    for (QWidget* t : m_activeToasts)
        yOffset += t->height() + stackGap;

    int x = m_mainWindow->width()  - toast->width()  - margin;
    int y = m_mainWindow->height() - toast->height() - margin - yOffset;

    toast->move(x, y);
}

void NotificationManager::repositionToasts() {
    if (!m_mainWindow) return;
    const int margin   = 16;
    const int stackGap = 8;

    int yOffset = 0;
    for (QWidget* t : m_activeToasts) {
        if (!t) continue;
        int x = m_mainWindow->width()  - t->width()  - margin;
        int y = m_mainWindow->height() - t->height() - margin - yOffset;
        t->move(x, y);
        yOffset += t->height() + stackGap;
    }
}
