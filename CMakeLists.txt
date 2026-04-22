cmake_minimum_required(VERSION 3.16)
project(AccountManager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Charts Network)

set(SOURCES
    main.cpp
    data/DatabaseManager.cpp
    data/AccountModel.cpp
    data/TransactionModel.cpp
    logic/AuthManager.cpp
    logic/AccountManager.cpp
    logic/TransactionManager.cpp
    logic/TransferManager.cpp
    logic/SearchEngine.cpp
    logic/NotificationManager.cpp
    logic/ValidationHelper.cpp
    ui/LoginWindow.cpp
    ui/MainWindow.cpp
    ui/DashboardWidget.cpp
    ui/AccountWidget.cpp
    ui/TransactionWidget.cpp
    ui/TransferDialog.cpp
    ui/BudgetWidget.cpp
    ui/SettingsDialog.cpp
)

set(HEADERS
    data/DatabaseManager.h
    data/AccountModel.h
    data/TransactionModel.h
    logic/AuthManager.h
    logic/AccountManager.h
    logic/TransactionManager.h
    logic/TransferManager.h
    logic/SearchEngine.h
    logic/NotificationManager.h
    logic/ValidationHelper.h
    ui/LoginWindow.h
    ui/MainWindow.h
    ui/DashboardWidget.h
    ui/AccountWidget.h
    ui/TransactionWidget.h
    ui/TransferDialog.h
    ui/BudgetWidget.h
    ui/SettingsDialog.h
)

if(WIN32)
    set(WIN_RC app.rc)
endif()

qt_add_executable(AccountManager
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
    ${WIN_RC}
)

target_link_libraries(AccountManager PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Charts
    Qt6::Network
)

target_include_directories(AccountManager PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/logic
    ${CMAKE_SOURCE_DIR}/ui
)

if(WIN32)
    set_target_properties(AccountManager PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
