#include "DashboardWidget.h"
#include "DatabaseManager.h"
#include "AccountModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSqlQuery>
#include <QDateTime>
#include <QSizePolicy>
#include <QPainter>
#include <QProgressBar>
#include <QScrollArea>
#include <QHeaderView>
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
#include <QtWidgets>

// ─── 요약 카드 ───────────────────────────────────────────────
static QGroupBox* makeSummaryCard(const QString& title, QLabel*& valLabel,
                                  const QString& borderColor, const QString& bgColor,
                                  const QString& valueColor, QWidget* parent) {
    auto* box = new QGroupBox(parent);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    box->setFixedHeight(94);
    box->setStyleSheet(QString(
        "QGroupBox { border:1px solid #E5E7EB; border-left:4px solid %1; "
        "border-radius:10px; background:%2; margin-top:0; }").arg(borderColor, bgColor));

    auto* titleLbl = new QLabel(title, box);
    QFont tf("맑은 고딕", 9, QFont::DemiBold);
    titleLbl->setFont(tf);
    titleLbl->setStyleSheet("color:#374151; background:transparent;");

    valLabel = new QLabel("₩0", box);
    QFont vf("맑은 고딕", 16, QFont::Bold);
    valLabel->setFont(vf);
    valLabel->setStyleSheet(
        QString("color:%1; background:transparent; letter-spacing:-0.5px;").arg(valueColor));
    valLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    valLabel->setFixedHeight(32);

    auto* l = new QVBoxLayout(box);
    l->setContentsMargins(16, 10, 16, 10);
    l->setSpacing(4);
    l->addWidget(titleLbl);
    l->addWidget(valLabel);
    return box;
}

// ════════════════════════════════════════════════════════════
DashboardWidget::DashboardWidget(int userId, QWidget* parent)
    : QWidget(parent), m_userId(userId)
{
    setupUi();
    refresh();
}

// QGroupBox 공통 스타일 — 제목이 박스 안 상단 중앙에 위치
static QString groupBoxStyle(const QString& = "#F8FAFC") {
    return
        "QGroupBox {"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 10px;"
        "  margin-top: 0;"
        "  padding-top: 30px;"
        "  background: #FFFFFF;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top center;"
        "  top: 6px;"
        "  padding: 2px 12px;"
        "  color: #111827;"
        "  font-size: 10.5pt; font-weight: 800;"
        "  background: transparent;"
        "}";
}

// 차트 전용 QGroupBox 스타일 — 박스 안 상단 중앙
static QString chartGroupBoxStyle() {
    return
        "QGroupBox {"
        "  border: 1px solid #E2E8F0;"
        "  border-radius: 14px;"
        "  margin-top: 0;"
        "  padding-top: 34px;"
        "  background: #FFFFFF;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top center;"
        "  top: 6px;"
        "  padding: 3px 14px;"
        "  color: #111827;"
        "  font-size: 10.5pt; font-weight: 800;"
        "  background: transparent;"
        "}";
}

static void addShadow(QWidget* w) {
    auto* eff = new QGraphicsDropShadowEffect(w);
    eff->setBlurRadius(20);
    eff->setColor(QColor(0, 0, 0, 22));
    eff->setOffset(0, 3);
    w->setGraphicsEffect(eff);
}

void DashboardWidget::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    // ── 1. 요약 카드 ─────────────────────────────────────────
    auto* cardsRow = new QHBoxLayout;
    cardsRow->setSpacing(14);
    cardsRow->addWidget(makeSummaryCard(
        "총 자산",      m_totalBalanceLabel, "#1565C0", "#E3F2FD", "#1565C0", this));
    cardsRow->addWidget(makeSummaryCard(
        "이번 달 수입", m_monthIncomeLabel,  "#2E7D32", "#E8F5E9", "#2E7D32", this));
    cardsRow->addWidget(makeSummaryCard(
        "이번 달 지출", m_monthExpenseLabel, "#C62828", "#FFEBEE", "#C62828", this));
    root->addLayout(cardsRow);

    // ── 2. 최근 거래 내역 + 예산 대비 지출 ───────────────────
    auto* recentGroup = new QGroupBox("최근 거래 내역", this);
    recentGroup->setStyleSheet(groupBoxStyle());
    recentGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    recentGroup->setFixedHeight(248);
    auto* recentLay = new QVBoxLayout(recentGroup);
    recentLay->setContentsMargins(10, 10, 10, 10);
    recentLay->setSpacing(0);

    m_recentTxTable = new QTableWidget(0, 4, recentGroup);
    m_recentTxTable->setStyle(QStyleFactory::create("Fusion"));
    m_recentTxTable->setHorizontalHeaderLabels({"날짜", "계좌명", "유형", "금액"});
    m_recentTxTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_recentTxTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_recentTxTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_recentTxTable->setColumnWidth(0, 140);
    m_recentTxTable->setColumnWidth(3, 120);
    m_recentTxTable->horizontalHeader()->setFont(QFont("맑은 고딕", 8, QFont::DemiBold));
    m_recentTxTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_recentTxTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recentTxTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recentTxTable->verticalHeader()->hide();
    m_recentTxTable->setAlternatingRowColors(true);
    m_recentTxTable->setShowGrid(false);
    m_recentTxTable->setFrameShape(QFrame::NoFrame);
    m_recentTxTable->setFont(QFont("맑은 고딕", 9));
    recentLay->addWidget(m_recentTxTable);

    auto* budgetGroup = new QGroupBox("예산 대비 지출", this);
    budgetGroup->setStyleSheet(groupBoxStyle());
    budgetGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    budgetGroup->setFixedHeight(248);
    auto* budgetGroupLay = new QVBoxLayout(budgetGroup);
    budgetGroupLay->setContentsMargins(10, 10, 10, 10);
    budgetGroupLay->setSpacing(0);

    auto* budgetScroll = new QScrollArea(budgetGroup);
    budgetScroll->setWidgetResizable(true);
    budgetScroll->setFrameShape(QFrame::NoFrame);
    m_budgetContainer = new QWidget;
    m_budgetLayout = new QVBoxLayout(m_budgetContainer);
    m_budgetLayout->setContentsMargins(4, 4, 4, 4);
    m_budgetLayout->setSpacing(12);
    budgetScroll->setWidget(m_budgetContainer);
    budgetGroupLay->addWidget(budgetScroll);

    auto* midRow = new QHBoxLayout;
    midRow->setSpacing(14);
    midRow->addWidget(recentGroup, 1);
    midRow->addWidget(budgetGroup, 1);
    root->addLayout(midRow);

    // ── 3. 차트 ──────────────────────────────────────────────
    setupCharts();

    // 막대 차트 박스
    auto* barGroup = new QGroupBox("월별 수입 / 지출", this);
    barGroup->setStyleSheet(chartGroupBoxStyle());
    barGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* barLay = new QVBoxLayout(barGroup);
    barLay->setContentsMargins(12, 8, 12, 12);
    barLay->setSpacing(0);
    barLay->addWidget(m_barChartView);
    addShadow(barGroup);

    // 파이 차트 박스
    auto* pieGroup = new QGroupBox("이번 달 카테고리별 지출", this);
    pieGroup->setStyleSheet(chartGroupBoxStyle());
    pieGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* pieLay = new QVBoxLayout(pieGroup);
    pieLay->setContentsMargins(12, 8, 12, 12);
    pieLay->setSpacing(0);
    addShadow(pieGroup);

    // 도넛 중앙 텍스트 오버레이
    auto* pieWrapper = new QWidget(pieGroup);
    auto* wrapStack  = new QStackedLayout(pieWrapper);
    wrapStack->setStackingMode(QStackedLayout::StackAll);
    wrapStack->addWidget(m_pieChartView);  // 하단 레이어

    auto* centerOverlay = new QWidget(pieWrapper);
    centerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    centerOverlay->setStyleSheet("background:transparent;");
    auto* overLay = new QVBoxLayout(centerOverlay);
    overLay->setAlignment(Qt::AlignCenter);
    overLay->setContentsMargins(0, 0, 0, 0);
    overLay->setSpacing(4);
    m_pieCenterTopLbl = new QLabel("—", centerOverlay);
    m_pieCenterTopLbl->setAlignment(Qt::AlignCenter);
    m_pieCenterTopLbl->setStyleSheet(
        "background:transparent; color:#6B7280; font-size:8pt; font-weight:600;");
    m_pieCenterBotLbl = new QLabel("—", centerOverlay);
    m_pieCenterBotLbl->setAlignment(Qt::AlignCenter);
    m_pieCenterBotLbl->setStyleSheet(
        "background:transparent; color:#111827; font-size:11pt; font-weight:800;");
    overLay->addWidget(m_pieCenterTopLbl);
    overLay->addWidget(m_pieCenterBotLbl);
    wrapStack->addWidget(centerOverlay);  // 상단 레이어

    // 우측 커스텀 범례
    m_pieLegendWidget = new QWidget(pieGroup);
    m_pieLegendWidget->setFixedWidth(100);
    m_pieLegendWidget->setStyleSheet("background:transparent;");
    m_pieLegendLayout = new QVBoxLayout(m_pieLegendWidget);
    m_pieLegendLayout->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_pieLegendLayout->setContentsMargins(4, 0, 0, 0);
    m_pieLegendLayout->setSpacing(8);

    auto* pieInnerLay = new QHBoxLayout;
    pieInnerLay->setContentsMargins(0, 0, 0, 0);
    pieInnerLay->setSpacing(0);
    pieInnerLay->addWidget(pieWrapper, 1);
    pieInnerLay->addWidget(m_pieLegendWidget);
    pieLay->addLayout(pieInnerLay);

    auto* chartRow = new QHBoxLayout;
    chartRow->setSpacing(14);
    chartRow->addWidget(barGroup, 3);
    chartRow->addWidget(pieGroup, 2);
    root->addLayout(chartRow, 1);
}

void DashboardWidget::setupCharts() {
    auto* barView = new QChartView(this);
    auto* pieView = new QChartView(this);
    barView->setRenderHint(QPainter::Antialiasing);
    pieView->setRenderHint(QPainter::Antialiasing);
    barView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    pieView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    barView->setMinimumHeight(180);
    pieView->setMinimumHeight(200);
    barView->setFrameShape(QFrame::NoFrame);
    pieView->setFrameShape(QFrame::NoFrame);
    m_barChartView = barView;
    m_pieChartView = pieView;
}

void DashboardWidget::refresh() {
    updateSummaryCards();
    updateBarChart();
    updatePieChart();
    updateRecentTransactions();
    updateBudgetProgress();
}

void DashboardWidget::updateSummaryCards() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);

    auto fmt = [](double v) { return AccountModel::formatKRW(v); };

    q.prepare("SELECT COALESCE(SUM(balance),0) FROM accounts WHERE user_id=:uid");
    q.bindValue(":uid", m_userId);
    if (q.exec() && q.next()) m_totalBalanceLabel->setText(fmt(q.value(0).toDouble()));

    q.prepare(R"(SELECT COALESCE(SUM(t.amount),0)
        FROM transactions t JOIN accounts a ON t.account_id=a.id
        WHERE a.user_id=:uid AND t.type='입금'
          AND strftime('%Y-%m',t.created_at)=strftime('%Y-%m','now','localtime'))");
    q.bindValue(":uid", m_userId);
    if (q.exec() && q.next()) m_monthIncomeLabel->setText(fmt(q.value(0).toDouble()));

    q.prepare(R"(SELECT COALESCE(SUM(t.amount),0)
        FROM transactions t JOIN accounts a ON t.account_id=a.id
        WHERE a.user_id=:uid AND t.type='출금'
          AND strftime('%Y-%m',t.created_at)=strftime('%Y-%m','now','localtime'))");
    q.bindValue(":uid", m_userId);
    if (q.exec() && q.next()) m_monthExpenseLabel->setText(fmt(q.value(0).toDouble()));
}

void DashboardWidget::updateRecentTransactions() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT t.created_at, a.name, t.type, t.amount
        FROM transactions t
        JOIN accounts a ON t.account_id = a.id
        WHERE a.user_id = :uid
        ORDER BY t.created_at DESC
        LIMIT 10
    )");
    q.bindValue(":uid", m_userId);

    m_recentTxTable->setRowCount(0);
    if (!q.exec()) return;

    int row = 0;
    while (q.next()) {
        m_recentTxTable->insertRow(row);

        QString rawDate = q.value(0).toString();
        QDateTime dt = QDateTime::fromString(rawDate, "yyyy-MM-dd HH:mm:ss");
        if (!dt.isValid()) dt = QDateTime::fromString(rawDate, Qt::ISODate);
        QString fmtDate = dt.isValid() ? dt.toString("MM/dd HH:mm") : rawDate.left(10);

        QString type   = q.value(2).toString();
        double  amount = q.value(3).toDouble();
        QColor  color  = (type == "입금") ? QColor("#059669") : QColor("#EF4444");

        auto* dateItem = new QTableWidgetItem(fmtDate);
        auto* acctItem = new QTableWidgetItem(q.value(1).toString());
        auto* typeItem = new QTableWidgetItem(type);
        typeItem->setForeground(color);
        typeItem->setTextAlignment(Qt::AlignCenter);

        auto* amtItem = new QTableWidgetItem(AccountModel::formatKRW(amount));
        amtItem->setForeground(color);
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_recentTxTable->setItem(row, 0, dateItem);
        m_recentTxTable->setItem(row, 1, acctItem);
        m_recentTxTable->setItem(row, 2, typeItem);
        m_recentTxTable->setItem(row, 3, amtItem);
        ++row;
    }
}

void DashboardWidget::updateBudgetProgress() {
    while (QLayoutItem* item = m_budgetLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT b.category, b.monthly_limit,
               COALESCE(SUM(t.amount), 0) AS spent
        FROM budgets b
        LEFT JOIN accounts a ON a.user_id = b.user_id
        LEFT JOIN transactions t ON t.account_id = a.id
            AND t.type = '출금'
            AND t.category = b.category
            AND strftime('%Y-%m', t.created_at) = strftime('%Y-%m', 'now', 'localtime')
        WHERE b.user_id = :uid
        GROUP BY b.id, b.category, b.monthly_limit
        ORDER BY b.category
    )");
    q.bindValue(":uid", m_userId);

    bool hasData = false;
    if (q.exec()) {
        while (q.next()) {
            hasData = true;
            QString category = q.value(0).toString();
            double  budget   = q.value(1).toDouble();
            double  spent    = q.value(2).toDouble();
            int     pct      = budget > 0 ? qMin((int)(spent * 100.0 / budget), 100) : 0;

            auto* rowWidget = new QWidget(m_budgetContainer);
            rowWidget->setMinimumHeight(52);
            auto* rowLay    = new QVBoxLayout(rowWidget);
            rowLay->setContentsMargins(0, 2, 0, 2);
            rowLay->setSpacing(4);

            auto* hdr    = new QHBoxLayout;
            auto* catLbl = new QLabel(category, rowWidget);
            catLbl->setStyleSheet("color:#374151; font-size:9pt; font-weight:600;");
            auto* amtLbl = new QLabel(
                QString("%1 / %2")
                    .arg(AccountModel::formatKRW(spent))
                    .arg(AccountModel::formatKRW(budget)),
                rowWidget);
            amtLbl->setStyleSheet("color:#6B7280; font-size:8pt;");
            hdr->addWidget(catLbl);
            hdr->addStretch();
            hdr->addWidget(amtLbl);

            auto* bar = new QProgressBar(rowWidget);
            bar->setRange(0, 100);
            bar->setValue(pct);
            bar->setTextVisible(true);
            bar->setFormat(QString::number(pct) + "%");
            bar->setFixedHeight(16);
            QString barColor = pct >= 100 ? "#EF4444" : pct >= 80 ? "#F59E0B" : "#3B82F6";
            bar->setStyleSheet(QString(
                "QProgressBar { background:#F3F4F6; border-radius:8px; border:none; "
                "height:16px; text-align:center; font-size:7pt; color:#374151; }"
                "QProgressBar::chunk { background:%1; border-radius:8px; }")
                .arg(barColor));

            rowLay->addLayout(hdr);
            rowLay->addWidget(bar);
            m_budgetLayout->addWidget(rowWidget);
        }
    }

    if (!hasData) {
        auto* emptyLbl = new QLabel("이번 달 등록된 예산이 없습니다.", m_budgetContainer);
        emptyLbl->setAlignment(Qt::AlignCenter);
        emptyLbl->setStyleSheet("color:#9CA3AF; font-size:9pt; padding:20px;");
        m_budgetLayout->addWidget(emptyLbl);
    }
    m_budgetLayout->addStretch();
}

void DashboardWidget::updateBarChart() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT strftime('%m',t.created_at) as month,
               SUM(CASE WHEN t.type='입금'  THEN t.amount ELSE 0 END) as income,
               SUM(CASE WHEN t.type='출금' THEN t.amount ELSE 0 END) as expense
        FROM transactions t JOIN accounts a ON t.account_id=a.id
        WHERE a.user_id=:uid AND strftime('%Y',t.created_at)=strftime('%Y','now','localtime')
        GROUP BY month ORDER BY month)");
    q.bindValue(":uid", m_userId);

    auto* incomeSet  = new QBarSet("수입");
    auto* expenseSet = new QBarSet("지출");
    incomeSet->setColor(QColor("#6EE7B7"));
    incomeSet->setBorderColor(Qt::transparent);
    expenseSet->setColor(QColor("#FCA5A5"));
    expenseSet->setBorderColor(Qt::transparent);
    QStringList months;

    if (q.exec()) {
        while (q.next()) {
            months << q.value(0).toString() + "월";
            // 만원 단위로 변환
            *incomeSet  << q.value(1).toDouble() / 10000.0;
            *expenseSet << q.value(2).toDouble() / 10000.0;
        }
    }
    if (months.isEmpty()) { months << "—"; *incomeSet << 0; *expenseSet << 0; }

    // 데이터 최대값으로 '깔끔한 정수' 눈금 간격 산출
    // rawStep = max/4 → 1·2·5 계열로 올림 → niceMax = step * ceil(max/step)
    double maxVal = 0;
    for (int i = 0; i < incomeSet->count(); ++i)
        maxVal = qMax(maxVal, qMax(incomeSet->at(i), expenseSet->at(i)));

    double tickInterval = 25.0, niceMax = 100.0;
    if (maxVal > 0) {
        double rawStep = maxVal / 4.0;
        double mag     = qPow(10.0, qFloor(qLn(rawStep) / qLn(10.0)));
        double norm    = rawStep / mag;
        if      (norm <= 1.0) tickInterval = 1.0  * mag;
        else if (norm <= 2.0) tickInterval = 2.0  * mag;
        else if (norm <= 5.0) tickInterval = 5.0  * mag;
        else                  tickInterval = 10.0 * mag;
        niceMax = tickInterval * qCeil(maxVal / tickInterval);
    }

    auto* series = new QBarSeries;
    series->append(incomeSet);
    series->append(expenseSet);
    series->setBarWidth(0.5);

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(false);
    chart->setMargins(QMargins(4, 10, 10, 10));

    auto* legend = chart->legend();
    legend->setAlignment(Qt::AlignTop);
    legend->setFont(QFont("맑은 고딕", 8, QFont::DemiBold));
    legend->setLabelColor(QColor("#374151"));
    legend->setMarkerShape(QLegend::MarkerShapeRectangle);
    legend->setBackgroundVisible(false);

    auto* axisX = new QBarCategoryAxis;
    axisX->append(months);
    axisX->setLabelsFont(QFont("맑은 고딕", 8));
    axisX->setLabelsColor(QColor("#6B7280"));
    axisX->setGridLineVisible(false);
    axisX->setLinePenColor(QColor("#E5E7EB"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setRange(0, niceMax);
    axisY->setTickType(QValueAxis::TicksDynamic);
    axisY->setTickInterval(tickInterval);
    axisY->setLabelFormat("%.0f");
    axisY->setLabelsFont(QFont("맑은 고딕", 8));
    axisY->setLabelsColor(QColor("#6B7280"));
    axisY->setGridLineColor(QColor("#F1F5F9"));
    axisY->setMinorGridLineVisible(false);
    axisY->setLinePenColor(Qt::transparent);
    axisY->setTitleText("(단위: 만원)");
    axisY->setTitleFont(QFont("맑은 고딕", 7));
    axisY->setTitleBrush(QBrush(QColor("#9CA3AF")));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    static_cast<QChartView*>(m_barChartView)->setChart(chart);
}

void DashboardWidget::updatePieChart() {
    static const QString kColors[] = {
        "#60A5FA", "#34D399", "#FBBF24", "#F87171",
        "#A78BFA", "#22D3EE", "#FB923C", "#F472B6"
    };

    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(R"(
        SELECT t.category, SUM(t.amount)
        FROM transactions t JOIN accounts a ON t.account_id=a.id
        WHERE a.user_id=:uid AND t.type='출금'
          AND strftime('%Y-%m',t.created_at)=strftime('%Y-%m','now','localtime')
        GROUP BY t.category ORDER BY 2 DESC)");
    q.bindValue(":uid", m_userId);

    // 데이터 수집
    struct CatRow { QString cat; double amt; };
    QList<CatRow> rows;
    double totalExp = 0;
    if (q.exec()) {
        while (q.next()) {
            double amt = q.value(1).toDouble();
            rows.append({ q.value(0).toString(), amt });
            totalExp += amt;
        }
    }

    // 중앙 레이블 업데이트
    if (rows.isEmpty()) {
        if (m_pieCenterTopLbl) m_pieCenterTopLbl->setText("지출 없음");
        if (m_pieCenterBotLbl) m_pieCenterBotLbl->setText("₩0");
    } else {
        if (m_pieCenterTopLbl) m_pieCenterTopLbl->setText(rows.first().cat);
        if (m_pieCenterBotLbl) m_pieCenterBotLbl->setText(AccountModel::formatKRW(totalExp));
    }

    // 커스텀 범례 재구성
    if (m_pieLegendLayout) {
        while (QLayoutItem* item = m_pieLegendLayout->takeAt(0)) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        int li = 0;
        for (const auto& r : rows) {
            auto* legRow = new QWidget(m_pieLegendWidget);
            legRow->setStyleSheet("background:transparent;");
            auto* legH = new QHBoxLayout(legRow);
            legH->setContentsMargins(0, 0, 0, 0);
            legH->setSpacing(6);

            auto* dot = new QLabel("●", legRow);
            dot->setStyleSheet(QString(
                "color:%1; font-size:9pt; background:transparent;").arg(kColors[li % 8]));
            dot->setFixedWidth(14);

            auto* lbl = new QLabel(r.cat.left(5), legRow);
            lbl->setStyleSheet("color:#374151; font-size:8pt; background:transparent;");
            lbl->setToolTip(r.cat);

            legH->addWidget(dot);
            legH->addWidget(lbl, 1);
            m_pieLegendLayout->addWidget(legRow);
            ++li;
        }
        m_pieLegendLayout->addStretch();
    }

    // 시리즈 구성
    auto* series = new QPieSeries;
    series->setHoleSize(0.52);
    series->setPieSize(0.82);
    if (rows.isEmpty()) {
        series->append("데이터 없음", 1);
    } else {
        for (const auto& r : rows)
            series->append(r.cat, r.amt);
    }

    int i = 0;
    for (auto* slice : series->slices()) {
        slice->setColor(QColor(kColors[i % 8]));
        slice->setBorderColor(QColor("#FFFFFF"));
        slice->setBorderWidth(2);
        slice->setLabelVisible(false);  // 커스텀 범례 사용
        ++i;
    }

    auto* chart = new QChart;
    chart->addSeries(series);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(false);
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->hide();

    static_cast<QChartView*>(m_pieChartView)->setChart(chart);
}
