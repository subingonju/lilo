#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include "DatabaseManager.h"
#include "AuthManager.h"
#include "LoginWindow.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("AccountManager");
    app.setOrganizationName("CourseProject");
    app.setWindowIcon(QIcon(":/images/bank.ico"));

    if (!DatabaseManager::instance().initialize()) {
        QMessageBox::critical(nullptr, "Fatal Error", "Cannot open database.");
        return 1;
    }

    // App restart loop: after logout we re-show the login window
    while (true) {
        LoginWindow login;
        if (login.exec() != QDialog::Accepted) break;

        int userId        = AuthManager::instance().currentUserId();
        QString username  = AuthManager::instance().currentUsername();

        MainWindow mainWin(userId, username);
        mainWin.show();

        // Run event loop; MainWindow calls QApplication::exit(0) on logout
        int ret = app.exec();
        if (ret != 0) return ret;   // real exit
        // ret == 0 means logout — loop back to login
    }
    return 0;
}
