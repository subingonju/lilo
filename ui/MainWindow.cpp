#include "MainWindow.h"
#include "DashboardWidget.h"
#include "AccountWidget.h"
#include "TransactionWidget.h"
#include "BudgetWidget.h"
#include "AuthManager.h"
#include "NotificationManager.h"
#include "AccountModel.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDateTime>

MainWindow::MainWindow(int userId, const QString& username, QWidget* parent)
    : QMainWindow(parent), m_userId(userId), m_username(username)
{
    setWindowTitle("VEDA");
    setMinimumSize(1100, 700);

    NotificationManager::instance().initialize(this);

    setupSidebar();
    setupMenuBar();
    setupStatusBar();

    // 환율: NAM 한 번만 생성, 하루 1회 갱신
    m_nam = new QNetworkAccessManager(this);
    m_rateTimer = new QTimer(this);
    m_rateTimer->setInterval(24 * 60 * 60 * 1000);
    connect(m_rateTimer, &QTimer::timeout, this, &MainWindow::fetchExchangeRates);
    m_rateTimer->start();
    fetchExchangeRates();

    qApp->setStyleSheet(
        "QWidget { background:#FFFFFF; color:#111827; }"
        "QMainWindow { background:#F8FAFC; }"
        "QMenuBar { background:#FFFFFF; color:#111827; border-bottom:1px solid #E5E7EB; }"
        "QMenuBar::item:selected { background:#F3F4F6; }"
        "QMenu { background:#FFFFFF; border:1px solid #E5E7EB; }"
        "QMenu::item:selected { background:#EFF6FF; color:#1D4ED8; }"
        "QScrollBar:vertical { background:#F3F4F6; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:#D1D5DB; border-radius:4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QScrollBar:horizontal { background:#F3F4F6; height:8px; border-radius:4px; }"
        "QScrollBar::handle:horizontal { background:#D1D5DB; border-radius:4px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }"
        "QStatusBar { background:#FFFFFF; border-top:1px solid #E5E7EB; color:#6B7280; }"
    );
}

void MainWindow::setupSidebar() {
    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── 사이드바 ──────────────────────────────────────────
    auto* sidebar = new QWidget(central);
    sidebar->setFixedWidth(210);
    sidebar->setObjectName("Sidebar");
    sidebar->setStyleSheet("QWidget#Sidebar { background: #1a1d2e; }");

    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 3, 14, 5);
    sideLayout->setSpacing(4);

    // 로고 이미지
    auto* logoLbl = new QLabel(sidebar);
    QPixmap logoPix(":/images/logo.png");
    if (!logoPix.isNull()) {
        const int logoW = 160;
        logoLbl->setPixmap(logoPix.scaledToWidth(logoW, Qt::SmoothTransformation));
    }
    logoLbl->setAlignment(Qt::AlignCenter);
    logoLbl->setStyleSheet("padding: 0 0 4px 0; background: transparent;");
    sideLayout->addWidget(logoLbl);

    // 구분선
    auto* divider = new QFrame(sidebar);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("color: #2d3152; margin-bottom: 2px;");
    sideLayout->addWidget(divider);

    // 네비게이션 버튼
    struct NavItem { QString icon; QString label; };
    const QList<NavItem> items = {
        { "◈", "대시보드" },
        { "◈", "계좌관리"     },
        { "◈", "거래내역"     },
        { "◈", "예산"     },
    };

    static const QString kBtnBase =
        "QPushButton {"
        "  background: transparent; color: #94a3b8;"
        "  border: none; border-radius: 8px;"
        "  padding: 11px 14px; text-align: left;"
        "  font-size: 10pt; font-weight: 500;"
        "}"
        "QPushButton:hover { background: #252840; color: #cbd5e1; }";

    static const QString kBtnActive =
        "QPushButton {"
        "  background: #3b4168; color: #ffffff;"
        "  border: none; border-left: 3px solid #6366f1; border-radius: 8px;"
        "  padding: 11px 11px; text-align: left;"
        "  font-size: 10pt; font-weight: 700;"
        "}";

    for (int i = 0; i < items.size(); ++i) {
        auto* btn = new QPushButton(items[i].icon + "   " + items[i].label, sidebar);
        btn->setStyleSheet(i == 0 ? kBtnActive : kBtnBase);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        m_navBtns.append(btn);
        sideLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setActivePage(i);
        });
    }

    sideLayout->addStretch();

    // ── 사이드바 하단: 환율 정보 ──────────────────────────
    auto* rateSep = new QFrame(sidebar);
    rateSep->setFrameShape(QFrame::HLine);
    rateSep->setStyleSheet("color: #2d3152; margin: 6px 0;");
    sideLayout->addWidget(rateSep);

    auto* rateTitleLbl = new QLabel("실시간 환율", sidebar);
    rateTitleLbl->setStyleSheet(
        "color: #CBD5E1; font-size: 7pt; font-weight: 700; "
        "padding: 2px 4px 3px 4px; background: transparent;");
    sideLayout->addWidget(rateTitleLbl);

    auto* rateGrid   = new QWidget(sidebar);
    rateGrid->setStyleSheet(
        "background: rgba(255,255,255,0.06); border-radius: 6px; padding: 4px;");
    auto* rateGridLay = new QGridLayout(rateGrid);
    rateGridLay->setContentsMargins(2, 0, 2, 0);
    rateGridLay->setHorizontalSpacing(8);
    rateGridLay->setVerticalSpacing(2);

    auto addRateRow = [&](int row, const QString& code, QLabel*& valueLabel,
                          const QString& tooltip) {
        auto* codeLbl = new QLabel(code, rateGrid);
        codeLbl->setStyleSheet(
            "color: #94A3B8; font-size: 7pt; font-weight: 700; background: transparent;");
        valueLabel = new QLabel("—", rateGrid);
        valueLabel->setStyleSheet(
            "color: #FACC15; font-size: 7.5pt; font-weight: 600; background: transparent;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setToolTip(tooltip);
        rateGridLay->addWidget(codeLbl,    row, 0);
        rateGridLay->addWidget(valueLabel, row, 1);
    };

    addRateRow(0, "USD", m_sideUsdLabel, "미국 달러 (1 USD → KRW)");
    addRateRow(1, "JPY", m_sideJpyLabel, "일본 엔 (100 JPY → KRW)");
    addRateRow(2, "EUR", m_sideEurLabel, "유로 (1 EUR → KRW)");
    addRateRow(3, "CNY", m_sideCnyLabel, "중국 위안 (1 CNY → KRW)");
    sideLayout->addWidget(rateGrid);

    m_sideRateTimeLbl = new QLabel("", sidebar);
    m_sideRateTimeLbl->setStyleSheet(
        "color: #94A3B8; font-size: 6.5pt; padding: 2px 4px 6px 4px; background: transparent;");
    sideLayout->addWidget(m_sideRateTimeLbl);

    // ── 콘텐츠 영역 ──────────────────────────────────────
    m_stack = new QStackedWidget(central);

    m_dashboard    = new DashboardWidget(m_userId, m_stack);
    m_accounts     = new AccountWidget(m_userId, m_stack);
    m_transactions = new TransactionWidget(m_userId, m_stack);
    m_budgets      = new BudgetWidget(m_userId, m_stack);

    m_stack->addWidget(m_dashboard);    // index 0
    m_stack->addWidget(m_accounts);     // index 1
    m_stack->addWidget(m_transactions); // index 2
    m_stack->addWidget(m_budgets);      // index 3

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(m_stack, 1);

    setCentralWidget(central);
}

void MainWindow::setActivePage(int index) {
    static const QString kBtnBase =
        "QPushButton {"
        "  background: transparent; color: #94a3b8;"
        "  border: none; border-radius: 8px;"
        "  padding: 11px 14px; text-align: left;"
        "  font-size: 10pt; font-weight: 500;"
        "}"
        "QPushButton:hover { background: #252840; color: #cbd5e1; }";

    static const QString kBtnActive =
        "QPushButton {"
        "  background: #3b4168; color: #ffffff;"
        "  border: none; border-left: 3px solid #6366f1; border-radius: 8px;"
        "  padding: 11px 11px; text-align: left;"
        "  font-size: 10pt; font-weight: 700;"
        "}";

    for (int i = 0; i < m_navBtns.size(); ++i)
        m_navBtns[i]->setStyleSheet(i == index ? kBtnActive : kBtnBase);

    m_stack->setCurrentIndex(index);
}

void MainWindow::fetchExchangeRates() {
    if (m_sideRateTimeLbl) m_sideRateTimeLbl->setText("불러오는 중...");
    qDebug() << "[환율] 요청 시작" << QDateTime::currentDateTime().toString("HH:mm:ss");

    auto* reply = m_nam->get(QNetworkRequest(QUrl("https://open.er-api.com/v6/latest/KRW")));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[환율] 네트워크 오류:" << reply->errorString();
            if (m_sideUsdLabel) m_sideUsdLabel->setText("오류");
            if (m_sideJpyLabel) m_sideJpyLabel->setText("오류");
            if (m_sideEurLabel) m_sideEurLabel->setText("오류");
            if (m_sideCnyLabel) m_sideCnyLabel->setText("오류");
            if (m_sideRateTimeLbl)
                m_sideRateTimeLbl->setText("⚠ 네트워크 오류");
            return;
        }

        auto doc   = QJsonDocument::fromJson(reply->readAll());
        auto rates = doc.object()["rates"].toObject();

        if (rates.isEmpty()) {
            qDebug() << "[환율] 응답 파싱 실패 — rates 없음";
            if (m_sideRateTimeLbl) m_sideRateTimeLbl->setText("⚠ 파싱 오류");
            return;
        }

        auto krwPer = [&](const QString& code) -> double {
            double r = rates[code].toDouble();
            return r > 0 ? 1.0 / r : 0.0;
        };
        auto fmt = [](double v) { return AccountModel::formatKRW(qRound(v)); };

        double usd = krwPer("USD");
        double jpy = krwPer("JPY") * 100;
        double eur = krwPer("EUR");
        double cny = krwPer("CNY");

        qDebug() << "[환율] USD=" << usd << "JPY(100)=" << jpy
                 << "EUR=" << eur << "CNY=" << cny;

        if (m_sideUsdLabel) m_sideUsdLabel->setText(fmt(usd));
        if (m_sideJpyLabel) m_sideJpyLabel->setText(fmt(jpy));
        if (m_sideEurLabel) m_sideEurLabel->setText(fmt(eur));
        if (m_sideCnyLabel) m_sideCnyLabel->setText(fmt(cny));

        qint64 unixTs = doc.object()["time_last_update_unix"].toInteger();
        QString dateStr = unixTs > 0
            ? QDateTime::fromSecsSinceEpoch(unixTs).toString("yyyy.MM.dd 기준")
            : QDate::currentDate().toString("yyyy.MM.dd 기준");
        if (m_sideRateTimeLbl) m_sideRateTimeLbl->setText(dateStr);
        qDebug() << "[환율] 업데이트 완료 —" << dateStr;
    });
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("파일(&F)");

    auto* logoutAct = fileMenu->addAction("로그아웃");
    connect(logoutAct, &QAction::triggered, this, &MainWindow::onLogout);



    // 새로 고침 — 파일과 도움말 사이
    auto* refreshAct = new QAction("새로 고침(&R)", this);
    refreshAct->setShortcut(Qt::Key_F5);
    connect(refreshAct, &QAction::triggered, this, [this]() {
        m_dashboard->refresh();
        m_accounts->refresh();
        m_transactions->refresh();
        m_budgets->refresh();
        fetchExchangeRates();
    });
    menuBar()->addAction(refreshAct);

    auto* helpMenu = menuBar()->addMenu("도움말(&H)");
    auto* aboutAct = helpMenu->addAction("정보");
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "정보", "계정 관리자 v1.0\n과제 프로젝트 — Qt6 / C++17");
    });
}

void MainWindow::setupStatusBar() {
    m_statusUser = new QLabel(QString("  로그인: %1  ").arg(m_username));
    statusBar()->addPermanentWidget(m_statusUser);
    statusBar()->showMessage("준비");
}


void MainWindow::onLogout() {
    AuthManager::instance().logout();
    qApp->setStyleSheet("");
    hide();
    QApplication::exit(0);
}
