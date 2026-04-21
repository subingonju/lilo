#include "LoginWindow.h"
#include "AuthManager.h"
#include "ValidationHelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDialog>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QApplication>

LoginWindow::LoginWindow(QWidget* parent) : QDialog(parent) {
    setWindowTitle("VEDA");
    setMinimumWidth(420);
    setModal(true);
    setupUi();
}

void LoginWindow::setupUi() {
    setStyleSheet("QDialog { background: #F0F4FA; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 헤더 배너 ───────────────────────────────────────────
    auto* banner = new QFrame(this);
    banner->setFixedHeight(130);
    banner->setStyleSheet("background: #003585; border-radius: 0;");
    auto* bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setAlignment(Qt::AlignCenter);

    auto* logoLbl = new QLabel(banner);
    logoLbl->setFixedHeight(100);
    logoLbl->setAlignment(Qt::AlignCenter);
    logoLbl->setStyleSheet("background: transparent;");
    QPixmap logoPix(":/images/logo.png");
    if (!logoPix.isNull())
        logoLbl->setPixmap(logoPix.scaledToWidth(200, Qt::SmoothTransformation));

    bannerLayout->addWidget(logoLbl);
    root->addWidget(banner);

    // ── 로그인 카드 ──────────────────────────────────────────
    auto* card = new QFrame(this);
    card->setStyleSheet(
        "QFrame { background: white; border-radius: 0; }"
        "QLabel { background: transparent; }");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(40, 32, 40, 32);
    cardLayout->setSpacing(16);

    auto* subtitle = new QLabel("로그인", card);
    subtitle->setStyleSheet("font-size: 13pt; font-weight: 700; color: #1A2233;");
    cardLayout->addWidget(subtitle);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft);
    m_userEdit = new QLineEdit(card);
    m_userEdit->setPlaceholderText("사용자명을 입력하세요");
    m_passEdit = new QLineEdit(card);
    m_passEdit->setPlaceholderText("비밀번호를 입력하세요");
    m_passEdit->setEchoMode(QLineEdit::Password);
    form->addRow("사용자명", m_userEdit);
    form->addRow("비밀번호", m_passEdit);
    cardLayout->addLayout(form);

    m_errorLabel = new QLabel(card);
    m_errorLabel->setStyleSheet(
        "color: #DC2626; background: #FEF2F2; border: 1px solid #FECACA; "
        "border-radius: 6px; padding: 8px 12px; font-size: 9pt;");
    m_errorLabel->hide();
    cardLayout->addWidget(m_errorLabel);

    m_loginBtn = new QPushButton("로그인", card);
    m_loginBtn->setFixedHeight(44);
    m_loginBtn->setDefault(true);
    m_loginBtn->setStyleSheet(
        "QPushButton { background: #003585; color: white; border: none; border-radius: 8px; "
        "font-size: 11pt; font-weight: 700; }"
        "QPushButton:hover { background: #004AB3; }"
        "QPushButton:pressed { background: #002060; }");
    cardLayout->addWidget(m_loginBtn);

    // 구분선
    auto* sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #E2E8F0;");
    cardLayout->addWidget(sep);

    // 회원가입 링크 버튼
    auto* regRow = new QHBoxLayout;
    auto* regHint = new QLabel("계정이 없으신가요?", card);
    regHint->setStyleSheet("color: #64748B; font-size: 9.5pt;");
    m_registerBtn = new QPushButton("회원가입", card);
    m_registerBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #1D4ED8; border: none; "
        "font-size: 9.5pt; font-weight: 700; padding: 0; }"
        "QPushButton:hover { color: #1E40AF; text-decoration: underline; }");
    m_registerBtn->setFlat(true);
    regRow->addStretch();
    regRow->addWidget(regHint);
    regRow->addWidget(m_registerBtn);
    regRow->addStretch();
    cardLayout->addLayout(regRow);

    root->addWidget(card);

    connect(m_loginBtn,    &QPushButton::clicked, this, &LoginWindow::onLogin);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegister);
    connect(m_passEdit, &QLineEdit::returnPressed, this, &LoginWindow::onLogin);
}

void LoginWindow::onLogin() {
    QString user = m_userEdit->text().trimmed();
    QString pass = m_passEdit->text();

    if (!ValidationHelper::isNotEmpty(user) || !ValidationHelper::isNotEmpty(pass)) {
        m_errorLabel->setText("사용자명과 비밀번호를 입력하세요.");
        m_errorLabel->show();
        return;
    }
    if (!AuthManager::instance().login(user, pass)) {
        m_errorLabel->setText("사용자명 또는 비밀번호가 올바르지 않습니다.");
        m_errorLabel->show();
        m_passEdit->clear();
        return;
    }
    accept();
}

void LoginWindow::onRegister() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("회원가입");
    dlg->setMinimumWidth(360);
    dlg->setStyleSheet("QDialog { background: white; } QLabel { background: transparent; }");

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(14);

    auto* title = new QLabel("새 계정 만들기", dlg);
    title->setStyleSheet("font-size:13pt; font-weight:700; color:#1A2233;");
    layout->addWidget(title);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    auto* uEdit  = new QLineEdit(dlg); uEdit->setPlaceholderText("영문/숫자 3~30자");
    auto* pEdit  = new QLineEdit(dlg); pEdit->setEchoMode(QLineEdit::Password);
    pEdit->setPlaceholderText("6자 이상");
    auto* p2Edit = new QLineEdit(dlg); p2Edit->setEchoMode(QLineEdit::Password);
    p2Edit->setPlaceholderText("비밀번호 재입력");
    form->addRow("사용자명", uEdit);
    form->addRow("비밀번호", pEdit);
    form->addRow("비밀번호 확인", p2Edit);
    layout->addLayout(form);

    auto* btns = new QHBoxLayout;
    auto* ok   = new QPushButton("회원가입", dlg);
    ok->setFixedHeight(40);
    ok->setStyleSheet(
        "QPushButton { background:#003585; color:white; border:none; border-radius:6px; "
        "font-size:10pt; font-weight:700; }"
        "QPushButton:hover { background:#004AB3; }");
    auto* can  = new QPushButton("취소", dlg);
    can->setFixedHeight(40);
    btns->addWidget(ok); btns->addWidget(can);
    layout->addLayout(btns);

    connect(can, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(ok,  &QPushButton::clicked, dlg, [=]() {
        if (!ValidationHelper::isValidUsername(uEdit->text()))
            { QMessageBox::warning(dlg,"오류","사용자명은 3~30자의 영문/숫자여야 합니다."); return; }
        if (!ValidationHelper::isValidPassword(pEdit->text()))
            { QMessageBox::warning(dlg,"오류","비밀번호는 6자 이상이어야 합니다."); return; }
        if (pEdit->text() != p2Edit->text())
            { QMessageBox::warning(dlg,"오류","비밀번호가 일치하지 않습니다."); return; }
        if (!AuthManager::instance().registerUser(uEdit->text(), pEdit->text()))
            { QMessageBox::warning(dlg,"오류","이미 사용 중인 사용자명입니다."); return; }
        QMessageBox::information(dlg,"성공","계정이 생성되었습니다. 로그인하세요.");
        dlg->accept();
    });

    dlg->exec();
    dlg->deleteLater();
}
