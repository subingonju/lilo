#include "BudgetWidget.h"
#include "DatabaseManager.h"
#include "NotificationManager.h"
#include "AccountModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QDialog>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QLocale>
#include <QDebug>
#include <QStyledItemDelegate>
#include <QPainter>
#include <map>

// ── 열 정렬 위임자 + 가로 구분선 전용 렌더링 ─────────────────────
class AlignmentDelegate : public QStyledItemDelegate {
public:
    AlignmentDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setColumnAlignment(int column, Qt::Alignment alignment) {
        m_alignments[column] = alignment;
    }

    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        auto it = m_alignments.find(index.column());
        if (it != m_alignments.end())
            option->displayAlignment = it->second;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        painter->save();
        painter->setPen(QPen(QColor("#EEF2F7"), 1));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
    }

private:
    std::map<int, Qt::Alignment> m_alignments;
};

// ─────────────────────────────────────────────────────────────────

BudgetWidget::BudgetWidget(int userId, QWidget* parent)
    : QWidget(parent), m_userId(userId)
{
    setupUi();
    loadBudgets();
}

void BudgetWidget::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(12);

    // ── 요약 카드 3개 ─────────────────────────────────────────
    auto* cardsRow = new QHBoxLayout;
    cardsRow->setSpacing(12);

    auto makeCard = [&](const QString& title, QLabel*& valLbl, const QString& color) -> QGroupBox* {
        auto* box = new QGroupBox(this);
        box->setFixedHeight(90);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        box->setStyleSheet(QString(
            "QGroupBox { background:#FFFFFF; border:1px solid #E5E7EB; "
            "border-top:3px solid %1; border-radius:8px; margin-top:0; }").arg(color));
        auto* vl = new QVBoxLayout(box);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(2);
        vl->setAlignment(Qt::AlignVCenter);
        auto* titleLbl = new QLabel(title, box);
        titleLbl->setWordWrap(false);
        titleLbl->setStyleSheet("color:#6B7280; font-size:8.5pt; font-weight:600; background:transparent;");
        valLbl = new QLabel("₩0", box);
        valLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        valLbl->setMinimumWidth(0);
        valLbl->setWordWrap(false);
        valLbl->setStyleSheet(QString(
            "color:%1; font-size:11pt; font-weight:700; background:transparent;").arg(color));
        valLbl->adjustSize();
        vl->addWidget(titleLbl);
        vl->addWidget(valLbl);
        return box;
    };

    cardsRow->addWidget(makeCard("총 예산",   m_totalBudgetLabel, "#2563EB"), 1);
    cardsRow->addWidget(makeCard("총 지출",   m_totalSpentLabel,  "#DC2626"), 1);
    cardsRow->addWidget(makeCard("남은 예산", m_remainingLabel,   "#059669"), 1);
    root->addLayout(cardsRow);

    // ── 테이블 ────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"ID", "카테고리", "월 한도", "지출액", "잔여", "상태", "진행률"});
    m_table->horizontalHeaderItem(1)->setTextAlignment(Qt::AlignLeft  | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(3)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(4)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(5)->setTextAlignment(Qt::AlignCenter);
    m_table->horizontalHeaderItem(6)->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);

    auto* alignDelegate = new AlignmentDelegate(m_table);
    alignDelegate->setColumnAlignment(1, Qt::AlignLeft  | Qt::AlignVCenter);
    alignDelegate->setColumnAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    alignDelegate->setColumnAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    alignDelegate->setColumnAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
    alignDelegate->setColumnAlignment(5, Qt::AlignCenter);
    alignDelegate->setColumnAlignment(6, Qt::AlignLeft  | Qt::AlignVCenter);
    m_table->setItemDelegate(alignDelegate);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setColumnHidden(0, true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->setStyleSheet(
        "QTableWidget {"
        "  background:#FFFFFF;"
        "  border:1px solid #E2E8F0;"
        "  border-radius:8px;"
        "  font-size:9.5pt;"
        "  outline:0;"
        "}"
        "QTableWidget::item {"
        "  padding:0px 10px;"
        "  color:#1E293B;"
        "}"
        "QTableWidget::item:selected {"
        "  background:#EFF6FF;"
        "  color:#1D4ED8;"
        "}"
        "QHeaderView::section {"
        "  background:#F8FAFC;"
        "  color:#475569;"
        "  font-size:8.5pt;"
        "  font-weight:700;"
        "  border:none;"
        "  border-bottom:2px solid #E2E8F0;"
        "  border-right:1px solid #E2E8F0;"
        "  padding:0px 10px;"
        "  height:36px;"
        "}"
    );
    root->addWidget(m_table);

    auto mkBtn = [this](const QString& text, const QString& bg,
                        const QString& fg, const QString& border) {
        auto* b = new QPushButton(text, this);
        b->setFixedHeight(34);
        b->setMinimumWidth(80);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton{background:%1;color:%2;border:1px solid %3;border-radius:8px;"
            "font-size:9pt;font-weight:700;padding:0 14px;}"
            "QPushButton:hover{background:%2;color:#FFFFFF;border-color:%2;}"
        ).arg(bg, fg, border));
        return b;
    };

    auto* btns   = new QHBoxLayout;
    btns->setContentsMargins(0, 4, 0, 0);
    btns->setSpacing(8);
    auto* addBtn = mkBtn("예산 추가", "#ECFDF5", "#059669", "#A7F3D0");
    m_editBtn    = mkBtn("수정",      "#EFF6FF", "#1D4ED8", "#BFDBFE");
    m_deleteBtn  = mkBtn("삭제",      "#FEF2F2", "#DC2626", "#FECACA");
    btns->addWidget(addBtn);
    btns->addWidget(m_editBtn);
    btns->addWidget(m_deleteBtn);
    btns->addStretch();
    root->addLayout(btns);

    connect(addBtn,      &QPushButton::clicked, this, &BudgetWidget::onAdd);
    connect(m_editBtn,   &QPushButton::clicked, this, &BudgetWidget::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &BudgetWidget::onDelete);
}

void BudgetWidget::loadBudgets() {
    m_table->setRowCount(0);

    QSqlQuery q(DatabaseManager::instance().database());
    q.prepare(R"(
        SELECT b.id, b.category, b.monthly_limit,
               COALESCE(SUM(t.amount), 0) as spent
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

    if (!q.exec()) {
        qWarning() << "loadBudgets failed:" << q.lastError().text();
        return;
    }

    double totalLimit = 0, totalSpent = 0;
    int row = 0;
    while (q.next()) {
        int     id        = q.value(0).toInt();
        QString cat       = q.value(1).toString();
        double  limit     = q.value(2).toDouble();
        double  spent     = q.value(3).toDouble();
        double  remaining = limit - spent;
        int     pct       = limit > 0 ? (int)(spent * 100.0 / limit) : 0;

        totalLimit += limit;
        totalSpent += spent;

        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(id)));

        auto* catItem = new QTableWidgetItem(cat);
        m_table->setItem(row, 1, catItem);

        auto* limitItem = new QTableWidgetItem(AccountModel::formatKRW(limit));
        m_table->setItem(row, 2, limitItem);

        auto* spentItem = new QTableWidgetItem(AccountModel::formatKRW(spent));
        m_table->setItem(row, 3, spentItem);

        // 잔여 컬럼 (col 4) — 초과 시 마이너스 부호 + 빨간색
        QString remText = (remaining >= 0)
            ? AccountModel::formatKRW(remaining)
            : "-" + AccountModel::formatKRW(-remaining);
        auto* remItem = new QTableWidgetItem(remText);
        remItem->setForeground(remaining >= 0 ? QColor("#059669") : QColor("#DC2626"));
        m_table->setItem(row, 4, remItem);

        // 상태 배지 (col 5) — 소프트 파스텔 팔레트
        QString badgeText, badgeBg, badgeFg;
        if (spent > limit) {
            badgeText = "초과"; badgeBg = "#FEE2E2"; badgeFg = "#991B1B";
        } else if (spent == limit) {
            badgeText = "도달"; badgeBg = "#FFEDD5"; badgeFg = "#9A3412";
        } else if (pct >= 80) {
            badgeText = "위험"; badgeBg = "#FEF08A"; badgeFg = "#854D0E";
        } else {
            badgeText = "안전"; badgeBg = "#DCFCE7"; badgeFg = "#166534";
        }
        auto* badgeCell = new QWidget(m_table);
        badgeCell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* badgeLay = new QHBoxLayout(badgeCell);
        badgeLay->setContentsMargins(4, 6, 4, 6);
        badgeLay->setAlignment(Qt::AlignCenter);

        auto* badge = new QLabel(badgeText, badgeCell);
        badge->setFixedWidth(48);
        badge->setAlignment(Qt::AlignCenter);
        badge->setStyleSheet(QString(
            "background:%1; color:%2; border-radius:10px; padding:3px 6px; "
            "font-size:8pt; font-weight:700;").arg(badgeBg, badgeFg));
        badgeLay->addWidget(badge);
        m_table->setCellWidget(row, 5, badgeCell);

        // 진행률 바 (col 6)
        QString barColor;
        if (spent > limit)          barColor = "#DC2626";
        else if (spent == limit)    barColor = "#F97316";
        else if (pct >= 80)         barColor = "#F59E0B";
        else if (pct >= 60)         barColor = "#FBBF24";
        else                        barColor = "#3B82F6";

        // 배경색이 진할수록 퍼센트 텍스트를 흰색으로 반전
        QString textColor = (pct >= 60 || spent >= limit) ? "#FFFFFF" : "#374151";

        auto* barCell = new QWidget(m_table);
        barCell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* barCellLay = new QHBoxLayout(barCell);
        barCellLay->setContentsMargins(8, 0, 8, 0);
        barCellLay->setSpacing(0);
        barCellLay->setAlignment(Qt::AlignVCenter);

        auto* bar = new QProgressBar(barCell);
        bar->setRange(0, 100);
        bar->setValue(spent > limit ? 100 : qMin(pct, 100));
        bar->setTextVisible(true);
        bar->setFormat(QString::number(pct) + "%");
        bar->setFixedHeight(20);
        bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        if (spent > limit) {
            // 초과: 대각선 그라데이션으로 경고 의미를 세련되게 연출
            bar->setStyleSheet(
                "QProgressBar {"
                "  background:#FEE2E2; border:none; border-radius:10px;"
                "  text-align:center; font-size:8pt; font-weight:700; color:#FFFFFF;"
                "}"
                "QProgressBar::chunk {"
                "  background:qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                "    stop:0 #991B1B, stop:0.3 #DC2626,"
                "    stop:0.5 #EF4444, stop:0.7 #DC2626, stop:1 #991B1B);"
                "  border-radius:10px; margin:0;"
                "}");
        } else {
            bar->setStyleSheet(QString(
                "QProgressBar {"
                "  background:#F1F5F9; border:none; border-radius:10px;"
                "  text-align:center; font-size:8pt; font-weight:700; color:%1;"
                "}"
                "QProgressBar::chunk { background:%2; border-radius:10px; margin:0; }")
                .arg(textColor, barColor));
            QPalette barPal = bar->palette();
            barPal.setColor(QPalette::Highlight, QColor(barColor));
            bar->setPalette(barPal);
        }
        barCellLay->addWidget(bar);
        m_table->setCellWidget(row, 6, barCell);
        ++row;
    }

    // 컬럼 너비 설정
    auto* hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::Fixed);
    hdr->setSectionResizeMode(6, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 90);
    m_table->setColumnWidth(2, 120);
    m_table->setColumnWidth(3, 120);
    m_table->setColumnWidth(4, 120);
    m_table->setColumnWidth(5, 70);

    // 요약 카드 업데이트
    double totalRemaining = totalLimit - totalSpent;
    if (m_totalBudgetLabel)
        m_totalBudgetLabel->setText(AccountModel::formatKRW(totalLimit));
    if (m_totalSpentLabel)
        m_totalSpentLabel->setText(AccountModel::formatKRW(totalSpent));
    if (m_remainingLabel) {
        QString remText = (totalRemaining >= 0)
            ? AccountModel::formatKRW(totalRemaining)
            : "-" + AccountModel::formatKRW(-totalRemaining);
        m_remainingLabel->setText(remText);
        m_remainingLabel->setStyleSheet(QString(
            "color:%1; font-size:11pt; font-weight:700; background:transparent;")
            .arg(totalRemaining >= 0 ? "#059669" : "#DC2626"));
    }
}

void BudgetWidget::refresh() {
    loadBudgets();
    NotificationManager::instance().checkBudgets(m_userId);
}

void BudgetWidget::onAdd() {
    auto* dlg  = new QDialog(this);
    dlg->setWindowTitle("예산 추가");
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* catBox = new QComboBox(dlg);
    catBox->addItems({"급여", "식비", "교통", "쇼핑", "의료", "여가", "이체", "기타"});
    auto* limitSpin = new QDoubleSpinBox(dlg);
    limitSpin->setRange(1, 1e12);
    limitSpin->setDecimals(0);
    limitSpin->setSingleStep(10000);
    limitSpin->setValue(100000);

    form->addRow("카테고리:", catBox);
    form->addRow("월 한도:", limitSpin);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok   = new QPushButton("추가", dlg);
    auto* can  = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("INSERT OR REPLACE INTO budgets (user_id, category, monthly_limit) "
                  "VALUES (:uid, :cat, :lim)");
        q.bindValue(":uid", m_userId);
        q.bindValue(":cat", catBox->currentText());
        q.bindValue(":lim", limitSpin->value());
        if (!q.exec()) {
            QMessageBox::warning(dlg, "오류", "예산 저장에 실패했습니다: " + q.lastError().text());
            return;
        }
        dlg->accept();
        loadBudgets();
    });
    dlg->exec();
    dlg->deleteLater();
}

void BudgetWidget::onEdit() {
    int row = m_table->currentRow();
    if (row < 0) { QMessageBox::information(this, "선택", "수정할 예산을 선택하세요."); return; }

    int     id    = m_table->item(row, 0)->text().toInt();
    QString cat   = m_table->item(row, 1)->text();
    double  limit = m_table->item(row, 2)->text().toDouble();

    auto* dlg  = new QDialog(this);
    dlg->setWindowTitle("예산 수정 — " + cat);
    dlg->setMinimumWidth(400);
    auto* form = new QFormLayout(dlg);
    form->setSpacing(14);
    form->setContentsMargins(24, 24, 24, 24);

    auto* limitSpin = new QDoubleSpinBox(dlg);
    limitSpin->setRange(1, 1e12);
    limitSpin->setDecimals(0);
    limitSpin->setSingleStep(10000);
    limitSpin->setValue(limit);
    form->addRow("월 한도:", limitSpin);

    auto* btns = new QHBoxLayout;
    btns->setSpacing(8);
    auto* ok   = new QPushButton("저장", dlg);
    auto* can  = new QPushButton("취소", dlg);
    ok->setFixedHeight(36);
    can->setFixedHeight(36);
    btns->addWidget(ok); btns->addWidget(can);
    form->addRow(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("UPDATE budgets SET monthly_limit = :lim WHERE id = :id");
        q.bindValue(":lim", limitSpin->value());
        q.bindValue(":id",  id);
        if (!q.exec()) {
            QMessageBox::warning(dlg, "오류", "수정에 실패했습니다.");
            return;
        }
        dlg->accept();
        loadBudgets();
    });
    dlg->exec();
    dlg->deleteLater();
}

void BudgetWidget::onDelete() {
    int row = m_table->currentRow();
    if (row < 0) { QMessageBox::information(this, "선택", "삭제할 예산을 선택하세요."); return; }
    int id = m_table->item(row, 0)->text().toInt();

    if (QMessageBox::question(this, "삭제", "이 예산을 삭제하시겠습니까?") == QMessageBox::Yes) {
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare("DELETE FROM budgets WHERE id = :id");
        q.bindValue(":id", id);
        q.exec();
        loadBudgets();
    }
}

void BudgetWidget::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (e->type() == QEvent::PaletteChange) {
        if (layout()) layout()->invalidate();
        updateGeometry();
    }
}
