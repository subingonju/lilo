#include "AccountWidget.h"
#include "AccountManager.h"
#include "TransactionManager.h"
#include "TransferDialog.h"
#include "ValidationHelper.h"
#include "AccountModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QRegularExpressionValidator>
#include <QEvent>
#include <QMouseEvent>

// ── 유형 헬퍼 ──────────────────────────────────────────────

static QString typeLabel(const QString& t) {
    static const QMap<QString, QString> m = {
        {"Checking","입출금"}, {"Savings","저축"},
        {"Investment","투자"}, {"Cash","현금"},
        {"입출금","입출금"},   {"저축","저축"},
        {"투자","투자"},       {"현금","현금"}
    };
    return m.value(t, t);
}

static QColor typeColor(const QString& type) {
    const QString t = typeLabel(type);
    if (t == "입출금") return QColor(0x3B, 0x82, 0xF6);
    if (t == "저축")   return QColor(0x05, 0x96, 0x69);
    if (t == "투자")   return QColor(0x8B, 0x5C, 0xF6);
    return QColor(0xF5, 0x9E, 0x0B);
}

static QColor typeLightColor(const QString& type) {
    const QString t = typeLabel(type);
    if (t == "입출금") return QColor("#DBEAFE");
    if (t == "저축")   return QColor("#D1FAE5");
    if (t == "투자")   return QColor("#EDE9FE");
    return QColor("#FEF3C7");
}

static QString maskNumber(const QString& num) {
    const QStringList parts = num.split('-');
    if (parts.size() >= 3) {
        QString r = parts.first() + "-";
        for (int i = 1; i < parts.size() - 1; ++i) r += "***-";
        return r + parts.last();
    }
    if (num.length() > 7) return num.left(3) + "-****-" + num.right(4);
    return num;
}

// ─────────────────────────────────────────────────────────

AccountWidget::AccountWidget(int userId, QWidget* parent)
    : QWidget(parent), m_userId(userId)
{
    setupUi();
    refresh();

    connect(&AccountManager::instance(), &AccountManager::accountsChanged,
            this, &AccountWidget::refresh);
    connect(&TransactionManager::instance(), &TransactionManager::transactionsChanged,
            this, [this](int) { refresh(); });
}

void AccountWidget::setupUi() {
    m_model = new AccountModel(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(14);

    // ── 상단 바 ──────────────────────────────────────────
    auto* topBar = new QHBoxLayout;
    topBar->setSpacing(10);

    auto* titleLbl = new QLabel("계좌 관리", this);
    titleLbl->setFont(QFont("맑은 고딕", 14, QFont::Bold));
    titleLbl->setStyleSheet("color:#111827;");

    auto* totalCap = new QLabel("총 자산", this);
    totalCap->setFont(QFont("맑은 고딕", 8));
    totalCap->setStyleSheet("color:#6B7280;");

    m_totalLabel = new QLabel("₩0", this);
    m_totalLabel->setFont(QFont("맑은 고딕", 12, QFont::DemiBold));
    m_totalLabel->setStyleSheet("color:#1D4ED8;");

    auto* addBtn = new QPushButton("+ 계좌 추가", this);
    addBtn->setFixedHeight(34);
    addBtn->setStyleSheet(
        "QPushButton{background:#3B82F6;color:#fff;border:none;border-radius:8px;"
        "font-size:9pt;font-weight:600;padding:0 16px;}"
        "QPushButton:hover{background:#2563EB;}");

    // 뷰 전환 토글
    auto* toggleFr = new QFrame(this);
    toggleFr->setStyleSheet("QFrame{background:#F3F4F6;border-radius:10px;border:none;}");
    toggleFr->setFixedHeight(42);
    auto* tgl = new QHBoxLayout(toggleFr);
    tgl->setContentsMargins(4, 4, 4, 4);
    tgl->setSpacing(2);

    m_cardModeBtn = new QPushButton("⊞", toggleFr);
    m_listModeBtn = new QPushButton("≡", toggleFr);
    m_cardModeBtn->setFixedSize(34, 34);
    m_listModeBtn->setFixedSize(34, 34);
    m_cardModeBtn->setFont(QFont("Segoe UI", 15));
    m_listModeBtn->setFont(QFont("Segoe UI", 15));
    tgl->addWidget(m_cardModeBtn);
    tgl->addWidget(m_listModeBtn);

    topBar->addWidget(titleLbl);
    topBar->addStretch();
    topBar->addWidget(totalCap);
    topBar->addWidget(m_totalLabel);
    topBar->addSpacing(12);
    topBar->addWidget(addBtn);
    topBar->addWidget(toggleFr);
    root->addLayout(topBar);

    // ── 스택 (카드 / 리스트) ──────────────────────────────
    m_stack = new QStackedWidget(this);

    m_cardScroll = new QScrollArea(this);
    m_cardScroll->setWidgetResizable(true);
    m_cardScroll->setFrameShape(QFrame::NoFrame);
    m_cardScroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");

    m_listScroll = new QScrollArea(this);
    m_listScroll->setWidgetResizable(true);
    m_listScroll->setFrameShape(QFrame::NoFrame);
    m_listScroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");

    m_stack->addWidget(m_cardScroll);   // 0 = 카드
    m_stack->addWidget(m_listScroll);   // 1 = 리스트
    root->addWidget(m_stack, 1);

    // ── 시그널 연결 ───────────────────────────────────────
    connect(addBtn, &QPushButton::clicked, this, &AccountWidget::onAdd);

    auto updateToggle = [this]() {
        const char* on  =
            "QPushButton{background:#fff;color:#1D4ED8;border:none;"
            "border-radius:6px;padding:0 10px;}"
            "QPushButton:hover{background:#F0F9FF;}";
        const char* off =
            "QPushButton{background:transparent;color:#6B7280;border:none;"
            "border-radius:6px;padding:0 10px;}"
            "QPushButton:hover{background:#E5E7EB;}";
        m_cardModeBtn->setStyleSheet(m_cardMode ? on : off);
        m_listModeBtn->setStyleSheet(m_cardMode ? off : on);
    };
    updateToggle();

    connect(m_cardModeBtn, &QPushButton::clicked, this, [this, updateToggle]() {
        m_cardMode = true;
        m_stack->setCurrentIndex(0);
        updateToggle();
    });
    connect(m_listModeBtn, &QPushButton::clicked, this, [this, updateToggle]() {
        m_cardMode = false;
        m_stack->setCurrentIndex(1);
        updateToggle();
    });
}

// ── 카드 뷰 ───────────────────────────────────────────────

void AccountWidget::buildCards() {
    delete m_cardScroll->takeWidget();
    m_cardFrames.clear();

    auto* container = new QWidget;
    container->setStyleSheet("background:transparent;");
    auto* grid = new QGridLayout(container);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(16);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    const int count = m_model->rowCount();

    if (count == 0) {
        auto* empty = new QLabel("계좌가 없습니다.\n\n+ 계좌 추가 버튼으로 시작해보세요.", container);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color:#9CA3AF;font-size:11pt;");
        empty->setMinimumHeight(200);
        grid->addWidget(empty, 0, 0, 1, 3);
    }

    for (int i = 0; i < count; ++i) {
        const Account a   = m_model->accountAt(i);
        const QColor  tc  = typeColor(a.type);
        const bool    sel = (a.id == m_selectedId);

        auto* card = new QFrame(container);
        card->setObjectName("acctCard");
        card->setProperty("accountId", a.id);
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(210);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        card->setStyleSheet(sel
            ? "QFrame#acctCard{border:2px solid #3B82F6;border-radius:12px;background:#FFFFFF;}"
            : "QFrame#acctCard{border:1px solid #E5E7EB;border-radius:12px;background:#FFFFFF;}");
        card->installEventFilter(this);
        m_cardFrames.append(card);

        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(20, 16, 18, 14);
        vl->setSpacing(0);

        // 헤더: 유형 배지(ghost) + 계좌명
        auto* hdr = new QHBoxLayout;
        hdr->setSpacing(10);

        auto* badge = new QLabel(typeLabel(a.type), card);
        badge->setFixedHeight(22);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(QString(
            "color:%1;background:%2;"
            "border:none;border-radius:4px;"
            "font-size:8pt;font-weight:700;padding:0 8px;"
        ).arg(tc.name(), typeLightColor(a.type).name()));

        auto* nameLbl = new QLabel(a.name, card);
        nameLbl->setFont(QFont("맑은 고딕", 13, QFont::Bold));
        nameLbl->setStyleSheet("color:#111827;background:transparent;");
        nameLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        hdr->addWidget(badge);
        hdr->addWidget(nameLbl, 1);
        vl->addLayout(hdr);
        vl->addSpacing(8);

        // 계좌번호
        auto* numLbl = new QLabel(maskNumber(a.number), card);
        numLbl->setFont(QFont("맑은 고딕", 9));
        numLbl->setStyleSheet("color:#9CA3AF;background:transparent;");
        vl->addWidget(numLbl);

        vl->addStretch();

        // 잔액 캡션
        auto* balCapLbl = new QLabel("잔액", card);
        balCapLbl->setFont(QFont("맑은 고딕", 8));
        balCapLbl->setStyleSheet("color:#9CA3AF;background:transparent;");
        balCapLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vl->addWidget(balCapLbl);

        // 잔액
        auto* balLbl = new QLabel(AccountModel::formatKRW(a.balance), card);
        balLbl->setFont(QFont("맑은 고딕", 20, QFont::Bold));
        balLbl->setStyleSheet(QString(
            "color:%1;background:transparent;letter-spacing:-0.5px;"
        ).arg(a.balance >= 0 ? "#111827" : "#DC2626"));
        balLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vl->addWidget(balLbl);
        vl->addSpacing(14);

        // 구분선
        auto* divLine = new QFrame(card);
        divLine->setFrameShape(QFrame::HLine);
        divLine->setStyleSheet("border:none;border-top:1px solid #F3F4F6;");
        vl->addWidget(divLine);
        vl->addSpacing(10);

        // 액션 버튼
        auto* act = new QHBoxLayout;
        act->setSpacing(7);

        auto mkBtn = [&](const QString& text, const QString& bg,
                         const QString& fg, const QString& border) {
            auto* b = new QPushButton(text, card);
            b->setFixedHeight(34);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(QString(
                "QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:8px;"
                "font-size:9pt;font-weight:700;padding:0 6px;}"
                "QPushButton:hover{background:%2;color:#FFFFFF;border-color:%2;}"
            ).arg(bg, fg, border));
            return b;
        };

        const int aid = a.id;
        auto* depBtn  = mkBtn("입금", "#ECFDF5", "#059669", "#A7F3D0");
        auto* withBtn = mkBtn("출금", "#FEF2F2", "#DC2626", "#FECACA");
        auto* trfBtn  = mkBtn("이체", "#EFF6FF", "#1D4ED8", "#BFDBFE");
        auto* moreBtn = mkBtn("···",  "#F3F4F6", "#6B7280", "#E5E7EB");
        moreBtn->setFixedWidth(40);

        connect(depBtn,  &QPushButton::clicked, this, [this, aid]() { doDeposit(aid);  });
        connect(withBtn, &QPushButton::clicked, this, [this, aid]() { doWithdraw(aid); });
        connect(trfBtn,  &QPushButton::clicked, this, [this, aid]() { doTransfer(aid); });
        connect(moreBtn, &QPushButton::clicked, this, [this, moreBtn, aid]() {
            QMenu m(this);
            m.addAction("수정", [this, aid]() { doEdit(aid); });
            m.addSeparator();
            m.addAction("삭제", [this, aid]() { doDelete(aid); });
            m.exec(moreBtn->mapToGlobal(moreBtn->rect().bottomLeft()));
        });

        act->addWidget(depBtn, 1);
        act->addWidget(withBtn, 1);
        act->addWidget(trfBtn, 1);
        act->addWidget(moreBtn);
        vl->addLayout(act);

        grid->addWidget(card, i / 3, i % 3);
    }

    if (count > 0)
        grid->setRowStretch((count - 1) / 3 + 1, 1);

    m_cardScroll->setWidget(container);
}

// ── 리스트 뷰 ─────────────────────────────────────────────

void AccountWidget::buildList() {
    delete m_listScroll->takeWidget();
    m_listFrames.clear();

    auto* container = new QWidget;
    container->setStyleSheet("background:transparent;");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(8);

    const int count = m_model->rowCount();

    if (count == 0) {
        auto* empty = new QLabel("계좌가 없습니다.", container);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color:#9CA3AF;font-size:11pt;");
        empty->setFixedHeight(100);
        vl->addWidget(empty);
    }

    for (int i = 0; i < count; ++i) {
        const Account a   = m_model->accountAt(i);
        const QColor  tc  = typeColor(a.type);
        const QColor  tlc = typeLightColor(a.type);
        const bool    sel = (a.id == m_selectedId);

        auto* row = new QFrame(container);
        row->setObjectName("acctRow");
        row->setProperty("accountId", a.id);
        row->setCursor(Qt::PointingHandCursor);
        row->setFixedHeight(82);
        row->setStyleSheet(sel
            ? "QFrame#acctRow{border:2px solid #3B82F6;border-radius:12px;background:#FFFFFF;}"
            : "QFrame#acctRow{border:1px solid #E5E7EB;border-radius:12px;background:#FFFFFF;}");
        row->installEventFilter(this);
        m_listFrames.append(row);

        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(16, 0, 14, 0);
        hl->setSpacing(10);

        // 유형 아이콘
        auto* iconLbl = new QLabel(typeLabel(a.type).left(2), row);
        iconLbl->setFixedSize(48, 48);
        iconLbl->setAlignment(Qt::AlignCenter);
        iconLbl->setFont(QFont("맑은 고딕", 10, QFont::Bold));
        iconLbl->setStyleSheet(QString(
            "color:%1;background:%2;border-radius:12px;"
        ).arg(tc.name(), tlc.name()));
        hl->addWidget(iconLbl);

        // 계좌명 + 번호
        auto* infoW = new QWidget(row);
        infoW->setStyleSheet("background:transparent;");
        auto* infoV = new QVBoxLayout(infoW);
        infoV->setContentsMargins(0, 0, 0, 0);
        infoV->setSpacing(3);

        auto* nameLbl = new QLabel(a.name, infoW);
        nameLbl->setFont(QFont("맑은 고딕", 11, QFont::Bold));
        nameLbl->setStyleSheet("color:#111827;background:transparent;");

        auto* numLbl = new QLabel(maskNumber(a.number), infoW);
        numLbl->setFont(QFont("맑은 고딕", 9));
        numLbl->setStyleSheet("color:#9CA3AF;background:transparent;");

        infoV->addWidget(nameLbl);
        infoV->addWidget(numLbl);
        hl->addWidget(infoW, 1);

        // 잔액
        auto* balLbl = new QLabel(AccountModel::formatKRW(a.balance), row);
        balLbl->setFont(QFont("맑은 고딕", 13, QFont::Bold));
        balLbl->setStyleSheet(QString(
            "color:%1;background:transparent;letter-spacing:-0.5px;"
        ).arg(a.balance >= 0 ? "#111827" : "#DC2626"));
        balLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        balLbl->setMinimumWidth(140);
        hl->addWidget(balLbl);

        hl->addSpacing(6);

        // 액션 버튼
        auto mkRowBtn = [&](const QString& text, const QString& bg,
                            const QString& fg, const QString& border) {
            auto* b = new QPushButton(text, row);
            b->setMinimumWidth(54);
            b->setFixedHeight(34);
            b->setCursor(Qt::PointingHandCursor);
            b->setStyleSheet(QString(
                "QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:8px;"
                "font-size:9pt;font-weight:700;padding:0 6px;}"
                "QPushButton:hover{background:%2;color:#FFFFFF;border-color:%2;}"
            ).arg(bg, fg, border));
            return b;
        };

        const int aid = a.id;
        auto* depBtn  = mkRowBtn("입금",  "#ECFDF5", "#059669", "#A7F3D0");
        auto* withBtn = mkRowBtn("출금",  "#FEF2F2", "#DC2626", "#FECACA");
        auto* trfBtn  = mkRowBtn("이체",  "#EFF6FF", "#1D4ED8", "#BFDBFE");
        auto* moreBtn = mkRowBtn("···",   "#F3F4F6", "#6B7280", "#E5E7EB");
        moreBtn->setMinimumWidth(36);

        connect(depBtn,  &QPushButton::clicked, this, [this, aid]() { doDeposit(aid);  });
        connect(withBtn, &QPushButton::clicked, this, [this, aid]() { doWithdraw(aid); });
        connect(trfBtn,  &QPushButton::clicked, this, [this, aid]() { doTransfer(aid); });
        connect(moreBtn, &QPushButton::clicked, this, [this, moreBtn, aid]() {
            QMenu m(this);
            m.addAction("수정", [this, aid]() { doEdit(aid); });
            m.addSeparator();
            m.addAction("삭제", [this, aid]() { doDelete(aid); });
            m.exec(moreBtn->mapToGlobal(moreBtn->rect().bottomLeft()));
        });

        hl->addWidget(depBtn);
        hl->addWidget(withBtn);
        hl->addWidget(trfBtn);
        hl->addWidget(moreBtn);
        vl->addWidget(row);
    }

    vl->addStretch();
    m_listScroll->setWidget(container);
}

// ── 선택 상태 ─────────────────────────────────────────────

void AccountWidget::setSelectedAccount(int id) {
    m_selectedId = id;

    for (auto* f : m_cardFrames) {
        const bool sel = (f->property("accountId").toInt() == id);
        f->setStyleSheet(sel
            ? "QFrame#acctCard{border:2px solid #3B82F6;border-radius:12px;background:#FFFFFF;}"
            : "QFrame#acctCard{border:1px solid #E5E7EB;border-radius:12px;background:#FFFFFF;}");
    }
    for (auto* f : m_listFrames) {
        const bool sel = (f->property("accountId").toInt() == id);
        f->setStyleSheet(sel
            ? "QFrame#acctRow{border:2px solid #3B82F6;border-radius:12px;background:#FFFFFF;}"
            : "QFrame#acctRow{border:1px solid #E5E7EB;border-radius:12px;background:#FFFFFF;}");
    }
}

bool AccountWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        const QVariant prop = obj->property("accountId");
        if (prop.isValid()) setSelectedAccount(prop.toInt());
    }
    return QWidget::eventFilter(obj, event);
}

// ── refresh ───────────────────────────────────────────────

void AccountWidget::refresh() {
    m_model->loadAccounts(m_userId);
    m_totalLabel->setText(AccountModel::formatKRW(m_model->totalBalance()));
    buildCards();
    buildList();
}

// ── 공개 슬롯 (외부 호출용) ───────────────────────────────

void AccountWidget::onDeposit() {
    if (m_selectedId < 0) { QMessageBox::information(this, "선택", "계좌를 선택하세요."); return; }
    doDeposit(m_selectedId);
}

void AccountWidget::onWithdraw() {
    if (m_selectedId < 0) { QMessageBox::information(this, "선택", "계좌를 선택하세요."); return; }
    doWithdraw(m_selectedId);
}

// ── 계좌 추가 ─────────────────────────────────────────────

void AccountWidget::onAdd() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("계좌 추가");
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* nameEdit = new QLineEdit(dlg);
    nameEdit->setPlaceholderText("예: 주거래 통장");
    auto* numEdit  = new QLineEdit(dlg);
    numEdit->setPlaceholderText("예: 110-123-456789");
    auto* typeBox  = new QComboBox(dlg);
    typeBox->addItems({"입출금", "저축", "투자"});

    auto* currencyLbl = new QLabel("₩ KRW (대한민국 원)", dlg);
    currencyLbl->setStyleSheet("color:#64748B;font-size:9pt;");

    form->addRow("계좌명:",   nameEdit);
    form->addRow("계좌번호:", numEdit);
    form->addRow("유형:",     typeBox);
    form->addRow("통화:",     currencyLbl);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok   = new QPushButton("생성", dlg);
    auto* can  = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        if (!ValidationHelper::isNotEmpty(nameEdit->text())) {
            QMessageBox::warning(dlg, "오류", "계좌명을 입력하세요."); return;
        }
        if (!ValidationHelper::isValidAccountNumber(numEdit->text())) {
            QMessageBox::warning(dlg, "오류", "계좌번호는 6~20자리 숫자/하이픈이어야 합니다."); return;
        }
        if (!AccountManager::instance().createAccount(
                m_userId, nameEdit->text(), numEdit->text(), typeBox->currentText(), "KRW")) {
            QMessageBox::warning(dlg, "오류", "이미 존재하는 계좌번호입니다."); return;
        }
        dlg->accept();
    });
    dlg->exec();
    dlg->deleteLater();
}

// ── 내부 액션 ─────────────────────────────────────────────

void AccountWidget::doDeposit(int accountId)  { showAmountDialog(accountId, true);  }
void AccountWidget::doWithdraw(int accountId) { showAmountDialog(accountId, false); }

void AccountWidget::doTransfer(int accountId) {
    TransferDialog dlg(m_userId, this);
    dlg.setFromAccount(accountId);
    dlg.exec();
}

void AccountWidget::showAmountDialog(int accountId, bool isDeposit) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(isDeposit ? "입금" : "출금");
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* amtEdit = new QLineEdit(dlg);
    amtEdit->setPlaceholderText("금액 입력 (숫자만)");
    amtEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(R"(\d{1,15})"), dlg));

    auto* catBox = new QComboBox(dlg);
    catBox->addItems({"급여", "식비", "교통", "쇼핑", "의료", "여가", "이체", "기타"});
    auto* descEdit = new QLineEdit(dlg);
    descEdit->setPlaceholderText("거래 설명 (선택사항)");

    form->addRow("금액:",       amtEdit);
    form->addRow("카테고리:",   catBox);
    form->addRow("설명:",       descEdit);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok   = new QPushButton(isDeposit ? "입금" : "출금", dlg);
    auto* can  = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        const double amt = amtEdit->text().toDouble();
        if (amt <= 0) {
            QMessageBox::warning(dlg, "오류", "금액을 입력해 주세요.");
            return;
        }
        const bool ok2   = isDeposit
            ? TransactionManager::instance().deposit( accountId, amt, catBox->currentText(), descEdit->text())
            : TransactionManager::instance().withdraw(accountId, amt, catBox->currentText(), descEdit->text());
        if (!ok2)
            QMessageBox::warning(dlg, "오류", isDeposit ? "입금에 실패했습니다." : "잔액이 부족합니다.");
        else
            dlg->accept();
    });
    dlg->exec();
    dlg->deleteLater();
}

void AccountWidget::doEdit(int accountId) {
    const Account a = AccountManager::instance().getAccount(accountId);
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("계좌 수정");
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* nameEdit = new QLineEdit(a.name, dlg);
    auto* typeBox  = new QComboBox(dlg);
    typeBox->addItems({"입출금", "저축", "투자"});
    typeBox->setCurrentText(typeLabel(a.type));

    form->addRow("계좌명:", nameEdit);
    form->addRow("유형:",   typeBox);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok   = new QPushButton("저장", dlg);
    auto* can  = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addStretch(); btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        if (!ValidationHelper::isNotEmpty(nameEdit->text())) {
            QMessageBox::warning(dlg, "오류", "계좌명을 입력하세요."); return;
        }
        AccountManager::instance().updateAccount(accountId, nameEdit->text(), typeBox->currentText(), "KRW");
        dlg->accept();
    });
    dlg->exec();
    dlg->deleteLater();
}

void AccountWidget::doDelete(int accountId) {
    if (QMessageBox::question(this, "계좌 삭제",
            "이 계좌와 모든 거래 내역을 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.")
            == QMessageBox::Yes) {
        AccountManager::instance().deleteAccount(accountId);
        if (m_selectedId == accountId) m_selectedId = -1;
    }
}
