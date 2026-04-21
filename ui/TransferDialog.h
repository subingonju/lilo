#pragma once
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>

class TransferDialog : public QDialog {
    Q_OBJECT
public:
    explicit TransferDialog(int userId, QWidget* parent = nullptr);
    void setFromAccount(int accountId);

private slots:
    void onTransfer();

private:
    int           m_userId;
    QComboBox*  m_fromCombo;
    QComboBox*  m_toCombo;
    QLineEdit*  m_amtEdit;
    QLineEdit*  m_memoEdit;
};
