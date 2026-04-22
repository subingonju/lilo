#pragma once
#include <QWidget>
#include <QTableView>
#include <QComboBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QSpinBox>
#include "TransactionModel.h"
#include "AccountModel.h"

class TransactionWidget : public QWidget {
    Q_OBJECT
public:
    explicit TransactionWidget(int userId, QWidget* parent = nullptr);
    void refresh();

private slots:
    void onSearch();
    void onEdit();
    void onDelete();
    void onExportCsv();
    void onAccountChanged(int index);

private:
    void setupUi();
    int  selectedTransactionId();
    int  selectedRow();

    int               m_userId;
    QTableView*       m_view;
    TransactionModel* m_model;
    AccountModel*     m_accountModel;

    QComboBox*      m_accountCombo;
    QLineEdit*      m_keywordEdit;
    QDateEdit*      m_startDate;
    QDateEdit*      m_endDate;
    QSpinBox* m_minAmt;
    QSpinBox* m_maxAmt;
    QComboBox*      m_categoryFilter;
    QComboBox*      m_typeFilter;
};
