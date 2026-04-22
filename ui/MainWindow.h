#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QList>
#include <QTimer>
#include <QNetworkAccessManager>

class DashboardWidget;
class AccountWidget;
class TransactionWidget;
class TransferDialog;
class BudgetWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(int userId, const QString& username, QWidget* parent = nullptr);

private slots:
    void onLogout();
    void setActivePage(int index);
    void fetchExchangeRates();

private:
    void setupMenuBar();
    void setupStatusBar();
    void setupSidebar();

    int     m_userId;
    QString m_username;

    QStackedWidget*     m_stack;
    QList<QPushButton*> m_navBtns;

    DashboardWidget*   m_dashboard;
    AccountWidget*     m_accounts;
    TransactionWidget* m_transactions;
    BudgetWidget*      m_budgets;

    // 사이드바 환율 레이블
    QLabel* m_sideUsdLabel    = nullptr;
    QLabel* m_sideJpyLabel    = nullptr;
    QLabel* m_sideEurLabel    = nullptr;
    QLabel* m_sideCnyLabel    = nullptr;
    QLabel* m_sideRateTimeLbl = nullptr;

    QTimer*                m_rateTimer = nullptr;
    QNetworkAccessManager* m_nam       = nullptr;

    QLabel* m_statusUser;
};
