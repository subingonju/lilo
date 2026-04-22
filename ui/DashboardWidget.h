#pragma once
#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGridLayout>

class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(int userId, QWidget* parent = nullptr);
    void refresh();

private:
    void setupUi();
    void setupCharts();
    void updateSummaryCards();
    void updateBarChart();
    void updatePieChart();
    void updateRecentTransactions();
    void updateBudgetProgress();

    int m_userId;

    QLabel* m_totalBalanceLabel;
    QLabel* m_monthIncomeLabel;
    QLabel* m_monthExpenseLabel;

    QWidget* m_barChartView;
    QWidget* m_pieChartView;

    QTableWidget* m_recentTxTable;

    QWidget*     m_budgetContainer;
    QVBoxLayout* m_budgetLayout;

    QLabel*      m_pieCenterTopLbl = nullptr;
    QLabel*      m_pieCenterBotLbl = nullptr;
    QWidget*     m_pieLegendWidget = nullptr;
    QGridLayout* m_pieLegendLayout = nullptr;
};
