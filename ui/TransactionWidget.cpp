#include "TransactionWidget.h"
#include "SearchEngine.h"
#include "TransactionManager.h"
#include "AccountManager.h"
#include "NotificationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QTextStream>
#include <QDate>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QLineEdit>
#include <QCalendarWidget>
#include <QScreen>
#include <QStyleFactory>

// QDateEdit + 달력 버튼 래퍼 — QDialog 기반 팝업이라 부모 위젯 경계에 잘리지 않음
static QWidget* makeCalendarPicker(QDateEdit* edit, QWidget* dialogParent) {
    auto* wrapper = new QWidget(dialogParent);
    auto* hl = new QHBoxLayout(wrapper);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(3);

    edit->setParent(wrapper);
    hl->addWidget(edit, 1);

    auto* btn = new QPushButton("📅", wrapper);
    btn->setFixedSize(32, 32);
    btn->setToolTip("날짜 선택");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton{"
        "  background:#3B82F6; color:#FFFFFF;"
        "  border:none; border-radius:6px;"
        "  font-size:13px; padding:0;"
        "}"
        "QPushButton:hover{background:#2563EB;}"
        "QPushButton:pressed{background:#1D4ED8;}"
    );
    hl->addWidget(btn);

    QObject::connect(btn, &QPushButton::clicked, dialogParent, [edit, btn, dialogParent]() {
        auto* dlg = new QDialog(dialogParent->window(),
                                Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        auto* vl = new QVBoxLayout(dlg);
        vl->setContentsMargins(4, 4, 4, 4);
        vl->setSpacing(0);

        auto* cal = new QCalendarWidget(dlg);
        cal->setSelectedDate(edit->date());
        cal->setGridVisible(true);
        cal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
        cal->setMinimumSize(cal->sizeHint().expandedTo(QSize(360, 270)));

        // 내비게이션 바 스타일 (전역 QSS 간섭 없는 영역)
        cal->setStyleSheet(R"(
            QCalendarWidget QWidget#qt_calendar_navigationbar {
                background-color: #3B82F6;
            }
            QCalendarWidget QToolButton {
                color: #ffffff; background: transparent; font-weight: 600;
            }
            QCalendarWidget QToolButton:hover { background: #2563EB; border-radius: 4px; }
            QCalendarWidget QSpinBox { color: #ffffff; background: transparent; }
            QCalendarWidget QMenu    { color: #111827; background: #ffffff; }
        )");

        // 전역 QSS가 QAbstractItemView의 color를 덮어쓰므로,
        // 내부 뷰를 직접 찾아 위젯 자체 stylesheet + 셀 최소 크기 강제 지정
        if (auto* v = cal->findChild<QAbstractItemView*>()) {
            v->setStyleSheet(
                "color: #111827;"
                "background-color: #ffffff;"
                "selection-color: #ffffff;"
                "selection-background-color: #3B82F6;"
            );
            QPalette pal = v->palette();
            pal.setColor(QPalette::All, QPalette::Text,            QColor("#111827"));
            pal.setColor(QPalette::All, QPalette::Base,            Qt::white);
            pal.setColor(QPalette::All, QPalette::Highlight,       QColor("#3B82F6"));
            pal.setColor(QPalette::All, QPalette::HighlightedText, Qt::white);
            v->setPalette(pal);

            // 날짜 셀 최소 너비·높이 — 두 자릿수 숫자가 잘리지 않도록 보장
            if (auto* table = qobject_cast<QTableView*>(v)) {
                table->horizontalHeader()->setMinimumSectionSize(42);
                table->verticalHeader()->setMinimumSectionSize(32);
            }
        }

        // 주말 색상은 setWeekdayTextFormat으로 별도 지정
        QTextCharFormat satFmt, sunFmt;
        satFmt.setForeground(QColor("#1D4ED8"));
        sunFmt.setForeground(QColor("#DC2626"));
        cal->setWeekdayTextFormat(Qt::Saturday, satFmt);
        cal->setWeekdayTextFormat(Qt::Sunday,   sunFmt);

        // 이전/다음 달 날짜를 연한 회색으로 dim 처리
        // setDateTextFormat은 setWeekdayTextFormat보다 우선순위가 높으므로 확실히 덮어씀
        // 월이 바뀔 때마다 재적용해야 하므로 currentPageChanged에 연결
        auto dimOtherMonthDates = [cal]() {
            const int   m     = cal->monthShown();
            const int   y     = cal->yearShown();
            const QDate first(y, m, 1);
            const QDate last (y, m, first.daysInMonth());

            QTextCharFormat dimFmt;
            dimFmt.setForeground(QColor("#C4CBD8")); // 연한 회색

            QTextCharFormat clearFmt; // 빈 포맷 = date-specific 오버라이드 제거

            // 현재 월 날짜의 이전 dim 초기화
            for (QDate d = first; d <= last; d = d.addDays(1))
                cal->setDateTextFormat(d, clearFmt);

            // 이전 달 노출 날짜(최대 6일) dim
            for (QDate d = first.addDays(-6); d < first; d = d.addDays(1))
                cal->setDateTextFormat(d, dimFmt);

            // 다음 달 노출 날짜(최대 6일) dim
            for (QDate d = last.addDays(1); d <= last.addDays(6); d = d.addDays(1))
                cal->setDateTextFormat(d, dimFmt);
        };

        dimOtherMonthDates();
        QObject::connect(cal, &QCalendarWidget::currentPageChanged,
                         dlg, [dimOtherMonthDates](int, int) { dimOtherMonthDates(); });

        vl->addWidget(cal);

        QObject::connect(cal, &QCalendarWidget::clicked, dlg, [edit, dlg](const QDate& d) {
            edit->setDate(d);
            dlg->accept();
        });

        dlg->adjustSize();

        // 버튼 아래에 팝업을 위치시키되, 화면 밖으로 벗어나지 않도록 조정
        QPoint pos = btn->mapToGlobal(QPoint(0, btn->height()));
        const QRect screen = btn->screen()->availableGeometry();
        if (pos.x() + dlg->width()  > screen.right())
            pos.setX(screen.right() - dlg->width());
        if (pos.y() + dlg->height() > screen.bottom())
            pos.setY(btn->mapToGlobal(QPoint(0, 0)).y() - dlg->height());
        dlg->move(pos);

        dlg->exec();
    });

    return wrapper;
}

TransactionWidget::TransactionWidget(int userId, QWidget* parent)
    : QWidget(parent), m_userId(userId)
{
    setupUi();
    refresh();

    connect(&TransactionManager::instance(), &TransactionManager::transactionsChanged,
            this, [this](int) { onSearch(); });
    connect(&AccountManager::instance(), &AccountManager::accountsChanged,
            this, [this]() {
                int cur = m_accountCombo->currentIndex();
                m_accountModel->loadAccounts(m_userId);
                m_accountCombo->clear();
                m_accountCombo->addItem("전체 계좌", -1);
                for (const Account& a : m_accountModel->accounts())
                    m_accountCombo->addItem(a.name, a.id);
                m_accountCombo->setCurrentIndex(qMin(cur, m_accountCombo->count()-1));
            });
}

void TransactionWidget::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 10);
    root->setSpacing(8);

    auto* filterBox = new QGroupBox("검색 / 필터", this);
    auto* grid = new QGridLayout(filterBox);
    grid->setContentsMargins(10, 14, 10, 10);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    static const QString kComboStyle =
        "QComboBox {"
        "  background:#FFFFFF; color:#111827;"
        "  border:1px solid #D1D5DB; border-radius:6px;"
        "  padding:0 10px; min-height:32px;"
        "}"
        "QComboBox:hover { border-color:#9CA3AF; }"
        "QComboBox:focus { border:1.5px solid #2563EB; }"
        "QComboBox::drop-down { border:none; width:24px; }"
        "QComboBox QAbstractItemView {"
        "  background:#FFFFFF; color:#111827;"
        "  border:1px solid #E5E7EB; border-radius:6px;"
        "  selection-background-color:#EFF6FF;"
        "  selection-color:#2563EB;"
        "  outline:none; padding:4px;"
        "}"
        "QComboBox QAbstractItemView::item { padding:6px 12px; border-radius:4px; }";

    m_accountCombo   = new QComboBox(this);
    m_accountCombo->setStyleSheet(kComboStyle);
    m_keywordEdit    = new QLineEdit(this);
    m_keywordEdit->setPlaceholderText("키워드...");
    m_categoryFilter = new QComboBox(this);
    m_categoryFilter->setStyleSheet(kComboStyle);
    m_categoryFilter->addItems({"전체", "급여", "식비", "교통", "쇼핑", "의료", "여가", "이체", "기타"});

    m_startDate = new QDateEdit(QDate::currentDate().addMonths(-1), this);
    m_endDate   = new QDateEdit(QDate::currentDate(), this);
    m_startDate->setCalendarPopup(false);
    m_endDate->setCalendarPopup(false);
    m_startDate->setDisplayFormat("yyyy-MM-dd");
    m_endDate->setDisplayFormat("yyyy-MM-dd");
    m_startDate->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_endDate->setButtonSymbols(QAbstractSpinBox::NoButtons);

    m_minAmt = new QSpinBox(this);
    m_maxAmt = new QSpinBox(this);
    m_minAmt->setRange(0, 2'000'000'000); m_minAmt->setSpecialValueText("전체");
    m_maxAmt->setRange(0, 2'000'000'000); m_maxAmt->setSpecialValueText("전체");
    m_minAmt->setValue(0); m_maxAmt->setValue(0);
    m_minAmt->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_maxAmt->setButtonSymbols(QAbstractSpinBox::NoButtons);

    auto* searchBtn = new QPushButton("검색", this);
    auto* resetBtn  = new QPushButton("초기화", this);
    searchBtn->setFixedSize(80, 34);
    resetBtn->setFixedSize(80, 34);

    static const QString kGrayBtn =
        "QPushButton{background:#6B7280;color:#FFFFFF;border:none;border-radius:6px;"
        "font-size:9pt;font-weight:600;}"
        "QPushButton:hover{background:#4B5563;}"
        "QPushButton:pressed{background:#374151;}";
    searchBtn->setStyleSheet(kGrayBtn);
    resetBtn->setStyleSheet(kGrayBtn);

    m_accountCombo->setMinimumWidth(130);
    m_keywordEdit->setMinimumWidth(130);
    m_categoryFilter->setMinimumWidth(130);
    m_startDate->setMinimumWidth(130);
    m_endDate->setMinimumWidth(130);
    m_minAmt->setMinimumWidth(130);
    m_maxAmt->setMinimumWidth(130);

    filterBox->setMinimumHeight(110);

    // row 1 아이템: 레이블 위 + 위젯 아래 구조의 래퍼
    auto makeItem = [this](const QString& labelText, QWidget* w) -> QWidget* {
        auto* c = new QWidget(this);
        c->setStyleSheet("background: transparent;");
        auto* v = new QVBoxLayout(c);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        v->addWidget(new QLabel(labelText, c));
        v->addWidget(w);
        return c;
    };

    // row 0: 계좌(2col), 키워드(2col), 카테고리(2col) — makeItem으로 row1과 구조 통일
    grid->addWidget(makeItem("계좌:",     m_accountCombo),  0, 0, 1, 2);
    grid->addWidget(makeItem("키워드:",   m_keywordEdit),   0, 2, 1, 2);
    grid->addWidget(makeItem("카테고리:", m_categoryFilter),0, 4, 1, 2);

    // row 1: 시작일, 종료일, 최소금액, 최대금액, 검색/초기화
    grid->addWidget(makeItem("시작일:",    makeCalendarPicker(m_startDate, this)), 1, 0);
    grid->addWidget(makeItem("종료일:",    makeCalendarPicker(m_endDate,   this)), 1, 1);
    grid->addWidget(makeItem("최소 금액:", m_minAmt),                              1, 2);
    grid->addWidget(makeItem("최대 금액:", m_maxAmt),                              1, 3);
    grid->addWidget(searchBtn, 1, 4, Qt::AlignBottom | Qt::AlignHCenter);
    grid->addWidget(resetBtn,  1, 5, Qt::AlignBottom | Qt::AlignHCenter);

    // 6 컬럼 균등 확장
    for (int c = 0; c < 6; ++c) grid->setColumnStretch(c, 1);

    root->addWidget(filterBox);

    m_accountModel = new AccountModel(this);
    m_accountModel->loadAccounts(m_userId);
    m_accountCombo->addItem("전체 계좌", -1);
    for (const Account& a : m_accountModel->accounts())
        m_accountCombo->addItem(a.name, a.id);

    m_model = new TransactionModel(this);
    m_view  = new QTableView(this);
    m_view->setModel(m_model);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->verticalHeader()->hide();
    m_view->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_view->setStyle(QStyleFactory::create("Fusion"));
    m_view->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "  background: #F3F4F6;"
        "  color: #6B7280;"
        "  font-size: 8.5pt; font-weight: 600;"
        "  border: none;"
        "  border-bottom: 1px solid #E5E7EB;"
        "  padding: 6px 8px;"
        "}");
    m_view->setStyleSheet(
        "QTableView {"
        "  outline: 0;"
        "}"
        "QTableView::item {"
        "  border: none;"
        "  outline: 0;"
        "}"
        "QTableView::item:selected {"
        "  background: #DBEAFE;"
        "  color: #1E40AF;"
        "  border: none;"
        "  outline: 0;"
        "}"
        "QTableView::item:focus {"
        "  background: #DBEAFE;"
        "  color: #1E40AF;"
        "  border: none;"
        "  outline: 0;"
        "}");
    root->addWidget(m_view);

    auto* btns      = new QHBoxLayout;
    btns->setContentsMargins(0, 4, 0, 0);
    btns->setSpacing(8);
    auto* editBtn   = new QPushButton("수정",         this);
    auto* deleteBtn = new QPushButton("삭제",         this);
    auto* exportBtn = new QPushButton("CSV 내보내기", this);
    editBtn->setFixedHeight(36);
    deleteBtn->setFixedHeight(36);
    exportBtn->setFixedHeight(36);
    editBtn->setMinimumWidth(80);
    deleteBtn->setMinimumWidth(80);
    exportBtn->setMinimumWidth(120);
    editBtn->setStyleSheet(
        "QPushButton{background:#F0F9FF;color:#0284C7;border:1px solid #BAE6FD;"
        "border-radius:7px;font-weight:600;}"
        "QPushButton:hover{background:#0284C7;color:#FFFFFF;border-color:#0284C7;}"
        "QPushButton:pressed{background:#0369A1;}");
    deleteBtn->setStyleSheet(
        "QPushButton{background:#FEF2F2;color:#DC2626;border:1px solid #FECACA;"
        "border-radius:7px;font-weight:600;}"
        "QPushButton:hover{background:#DC2626;color:#FFFFFF;border-color:#DC2626;}"
        "QPushButton:pressed{background:#B91C1C;}");
    exportBtn->setStyleSheet(
        "QPushButton{background:#F0FDF4;color:#059669;border:1px solid #A7F3D0;"
        "border-radius:7px;font-weight:600;}"
        "QPushButton:hover{background:#059669;color:#FFFFFF;border-color:#059669;}"
        "QPushButton:pressed{background:#047857;}");
    btns->addWidget(editBtn);
    btns->addWidget(deleteBtn);
    btns->addStretch();
    btns->addWidget(exportBtn);
    root->addLayout(btns);

    connect(searchBtn, &QPushButton::clicked, this, &TransactionWidget::onSearch);
    connect(resetBtn,  &QPushButton::clicked, this, [this]() {
        m_keywordEdit->clear();
        m_categoryFilter->setCurrentIndex(0);
        m_startDate->setDate(QDate::currentDate().addMonths(-1));
        m_endDate->setDate(QDate::currentDate());
        m_minAmt->setValue(0); m_maxAmt->setValue(0);
        m_accountCombo->setCurrentIndex(0);
        onSearch();
    });
    connect(editBtn,   &QPushButton::clicked, this, &TransactionWidget::onEdit);
    connect(deleteBtn, &QPushButton::clicked, this, &TransactionWidget::onDelete);
    connect(exportBtn, &QPushButton::clicked, this, &TransactionWidget::onExportCsv);
    connect(m_accountCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TransactionWidget::onAccountChanged);
    connect(m_categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TransactionWidget::onSearch);
}

void TransactionWidget::refresh() {
    onSearch();
}

void TransactionWidget::onSearch() {
    SearchCriteria c;
    c.userId    = m_userId;
    int aid = m_accountCombo->currentData().toInt();
    if (aid > 0) c.accountId = aid;
    c.keyword   = m_keywordEdit->text().trimmed();
    c.startDate = m_startDate->date().toString("yyyy-MM-dd");
    c.endDate   = m_endDate->date().toString("yyyy-MM-dd");
    if (m_minAmt->value() > 0) c.minAmount = m_minAmt->value();
    if (m_maxAmt->value() > 0) c.maxAmount = m_maxAmt->value();
    // "전체" means no category filter
    c.category  = (m_categoryFilter->currentText() == "전체") ? "All" : m_categoryFilter->currentText();

    m_model->setTransactions(SearchEngine::instance().search(c));
    auto* hdr = m_view->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::Fixed);
    hdr->setSectionResizeMode(6, QHeaderView::Stretch);
    m_view->setColumnWidth(0, 50);
    m_view->setColumnWidth(1, 165);
    m_view->setColumnWidth(2, 120);
    m_view->setColumnWidth(3, 65);
    m_view->setColumnWidth(4, 90);
    m_view->setColumnWidth(5, 160);
}

void TransactionWidget::onAccountChanged(int) {
    onSearch();
}

int TransactionWidget::selectedRow() {
    auto idx = m_view->selectionModel()->currentIndex();
    return idx.isValid() ? idx.row() : -1;
}

int TransactionWidget::selectedTransactionId() {
    int row = selectedRow();
    return row >= 0 ? m_model->transactionAt(row).id : -1;
}

void TransactionWidget::onEdit() {
    int row = selectedRow();
    if (row < 0) {
        QMessageBox mb(QMessageBox::Information, "선택", "거래를 선택하세요.",
                       QMessageBox::Ok, this);
        mb.button(QMessageBox::Ok)->setMinimumWidth(140);
        mb.exec();
        return;
    }

    Transaction t = m_model->transactionAt(row);

    auto* dlg  = new QDialog(this);
    dlg->setWindowTitle("거래 수정");
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* catBox = new QComboBox(dlg);
    catBox->addItems({"급여", "식비", "교통", "쇼핑", "의료", "여가", "이체", "기타"});
    catBox->setCurrentText(t.category);

    auto* descEdit = new QLineEdit(t.description, dlg);
    form->addRow("카테고리:", catBox);
    form->addRow("설명:", descEdit);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok  = new QPushButton("저장", dlg);
    auto* can = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        if (!TransactionManager::instance().updateTransaction(t.id, t.amount,
                catBox->currentText(), descEdit->text())) {
            QMessageBox::warning(dlg, "오류", "수정에 실패했습니다.");
            return;
        }
        NotificationManager::instance().checkBudgets(m_userId);
        dlg->accept();
    });
    dlg->exec();
    dlg->deleteLater();
}

void TransactionWidget::onDelete() {
    int id = selectedTransactionId();
    if (id < 0) {
        QMessageBox mb(QMessageBox::Information, "선택", "거래를 선택하세요.",
                       QMessageBox::Ok, this);
        mb.button(QMessageBox::Ok)->setMinimumWidth(140);
        mb.exec();
        return;
    }

    if (QMessageBox::question(this, "삭제", "이 거래를 삭제하시겠습니까? 잔액이 재계산됩니다.")
            == QMessageBox::Yes) {
        TransactionManager::instance().deleteTransaction(id);
    }
}

void TransactionWidget::onExportCsv() {
    QString path = QFileDialog::getSaveFileName(this, "CSV 내보내기", "거래내역.csv",
                                                "CSV 파일 (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "오류", "파일을 열 수 없습니다.");
        return;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out << "\xEF\xBB\xBF"; // UTF-8 BOM
    out << "ID,날짜,계좌,유형,카테고리,금액,설명\n";

    for (const Transaction& t : m_model->transactions()) {
        QString name = t.accountName;
        QString desc = t.description;

        out << t.id << ","
            << t.createdAt << ","
            << "\"" << name.replace("\"", "\"\"") << "\","
            << t.type << ","
            << t.category << ","
            << QString::number(t.amount, 'f', 2) << ","
            << "\"" << desc.replace("\"", "\"\"") << "\"\n";
    }

    QMessageBox::information(this, "내보내기", "내보내기가 완료되었습니다.");
}
