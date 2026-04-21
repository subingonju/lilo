#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "TransactionModel.h"

struct SearchCriteria {
    QString keyword;
    QString startDate;
    QString endDate;
    double  minAmount = -1.0;
    double  maxAmount = -1.0;
    QString category;
    QString transactionType; // "입금" | "출금" | "" (전체)
    int     accountId = -1;
    int     userId    = -1;
};

class SearchEngine : public QObject {
    Q_OBJECT
public:
    static SearchEngine& instance();
    QList<Transaction> search(const SearchCriteria& criteria);

private:
    explicit SearchEngine(QObject* parent = nullptr);
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
};
