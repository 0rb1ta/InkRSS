#pragma once

#include "AppController.h"
#include "Widgets.h"

class QLabel;
class QLineEdit;
class QStackedWidget;
class QVBoxLayout;

// First start: Miniflux login (address and password or API key), "checking" and "no network"
// pages, and the way to feeds without a server.
class ConnectScreen : public Screen {
    Q_OBJECT
public:
    explicit ConnectScreen(AppController *app, QWidget *parent = nullptr);
    QString describe() const override;

private:
    enum class Page { Checking, Login, NoNetwork };
    void showPage(Page page);
    void setTokenAuth(bool token);
    void connectServer();
    QWidget *page(QVBoxLayout **layout);

    AppController *m_app;
    QStackedWidget *m_pages;
    QWidget *m_checking;
    QWidget *m_login;
    QWidget *m_noNetwork;
    QLabel *m_steps;
    QLabel *m_loginLogo;
    QLabel *m_loginTitle;
    QLineEdit *m_server;
    QLineEdit *m_user;
    QLineEdit *m_password;
    QWidget *m_passwordRow;
    QLineEdit *m_token;
    QPushButton *m_switchAuth;
    QLabel *m_error;
    QLabel *m_noNetworkText;
    QPushButton *m_offline;
    MinifluxAuth m_auth; // the last one tried, for "Retry"
    bool m_useToken = false;
};
