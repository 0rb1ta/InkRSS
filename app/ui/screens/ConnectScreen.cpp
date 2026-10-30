#include "ConnectScreen.h"
#include "I18n.h"

#include "Device.h"
#include "Log.h"
#include "Theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

void addHint(QVBoxLayout *v, QWidget *parent, const QString &text)
{
    v->addStretch(1);
    QLabel *d = Ui::centered(text, Theme::smallFont(), parent);
    d->setProperty("role", "muted");
    v->addWidget(d);
}

QLineEdit *field(const QString &placeholder, const QString &text, QWidget *parent)
{
    auto *e = new QLineEdit(text, parent);
    e->setPlaceholderText(placeholder);
    e->setFont(Theme::bodyFont());
    return e;
}

} // namespace

QWidget *ConnectScreen::page(QVBoxLayout **layout)
{
    auto *w = new QWidget(m_pages);
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(Theme::mm(8), Theme::mm(8), Theme::mm(8), Theme::mm(4));
    v->setSpacing(Theme::mm(2.5));
    *layout = v;
    m_pages->addWidget(w);
    return w;
}

ConnectScreen::ConnectScreen(AppController *app, QWidget *parent)
    : Screen(parent)
    , m_app(app)
{
    setObjectName(QStringLiteral("connect"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_pages = new QStackedWidget(this);
    outer->addWidget(m_pages);
    QVBoxLayout *v;

    // Checking the server.
    m_checking = page(&v);
    v->addStretch(1);
    v->addWidget(Ui::logo(Theme::mm(58), m_checking), 0, Qt::AlignHCenter);
    v->addSpacing(Theme::mm(6));
    v->addWidget(Ui::centered(tx("Проверяем сервер Miniflux…"), Theme::bodyFont(), m_checking));
    m_steps = Ui::centered(QString(), Theme::smallFont(), m_checking);
    v->addWidget(m_steps);
    v->addStretch(2);

    // Login.
    const MinifluxAuth saved = app->config().auth;
    m_login = page(&v);
    m_loginLogo = Ui::logo(Theme::mm(36), m_login);
    v->addWidget(m_loginLogo, 0, Qt::AlignHCenter);
    m_loginTitle = Ui::centered(tx("Подключение к Miniflux"), Theme::titleFont(), m_login);
    v->addWidget(m_loginTitle);
    m_server = field(tx("Адрес сервера, например rss.example.org"), saved.server.toString(), m_login);
    m_user = field(tx("Логин"), saved.user, m_login);
    m_password = field(tx("Пароль"), saved.password, m_login);
    m_password->setEchoMode(QLineEdit::Password);
    auto *eye = new QPushButton(Theme::icon(QStringLiteral("regular:eye")), QString(), m_login);
    eye->setObjectName(QStringLiteral("eye"));
    eye->setFixedWidth(Theme::mm(12));
    eye->setCheckable(true);
    connect(eye, &QPushButton::toggled, this,
            [this](bool on) { m_password->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    m_passwordRow = new QWidget(m_login);
    auto *pw = new QHBoxLayout(m_passwordRow);
    pw->setContentsMargins(0, 0, 0, 0);
    pw->addWidget(m_password, 1);
    pw->addWidget(eye);
    m_token = field(tx("Ключ API (Miniflux → Настройки → Ключи API)"), saved.token, m_login);
    m_error = Ui::centered(QString(), Theme::smallFont(), m_login);
    m_error->setStyleSheet(QStringLiteral("border: 1.5px dashed #000; padding: 4px;"));
    m_error->hide();
    auto *connectButton = Ui::bigButton(QStringLiteral("right-to-bracket"), tx("Подключиться"), true, m_login);
    m_switchAuth = new QPushButton(m_login);
    m_switchAuth->setObjectName(QStringLiteral("switch-auth"));
    m_switchAuth->setFlat(true);
    m_switchAuth->setFont(Theme::smallFont());
    m_switchAuth->setStyleSheet(QStringLiteral("QPushButton { border: none; text-decoration: underline; min-height: 0; padding: %1px; }")
                                    .arg(Theme::mm(1.5)));
    auto *direct = Ui::bigButton(QStringLiteral("rss"), tx("Без сервера: ленты напрямую"), false, m_login);
    for (QWidget *w : {static_cast<QWidget *>(m_server), static_cast<QWidget *>(m_user), m_passwordRow,
                       static_cast<QWidget *>(m_token), static_cast<QWidget *>(m_error),
                       static_cast<QWidget *>(connectButton), static_cast<QWidget *>(m_switchAuth),
                       static_cast<QWidget *>(direct)})
        v->addWidget(w);
    addHint(v, m_login, tx("Можно не вводить на экране: положите config.json в system/inkrss/ через USB."));
    connect(connectButton, &QPushButton::clicked, this, [this] { connectServer(); });
    connect(m_password, &QLineEdit::returnPressed, this, [this] { connectServer(); });
    connect(m_token, &QLineEdit::returnPressed, this, [this] { connectServer(); });
    connect(m_switchAuth, &QPushButton::clicked, this, [this] { setTokenAuth(!m_useToken); });
    connect(direct, &QPushButton::clicked, this, [this] { m_app->startDirect(); });

    // No network.
    m_noNetwork = page(&v);
    v->addWidget(Ui::logo(Theme::mm(46), m_noNetwork), 0, Qt::AlignHCenter);
    v->addSpacing(Theme::mm(4));
    v->addWidget(Ui::centered(tx("Нет подключения к интернету"), Theme::titleFont(), m_noNetwork));
    m_noNetworkText = Ui::centered(QString(), Theme::bodyFont(), m_noNetwork);
    v->addWidget(m_noNetworkText);
    v->addSpacing(Theme::mm(4));
    auto *wifi = Ui::bigButton(QStringLiteral("wifi"), tx("Подключить Wi-Fi"), true, m_noNetwork);
    auto *retry = Ui::bigButton(QStringLiteral("rotate"), tx("Повторить"), false, m_noNetwork);
    m_offline = Ui::bigButton(QStringLiteral("tablet-screen-button"), tx("Без сети: скачанные статьи"), false, m_noNetwork);
    auto *back = Ui::bigButton(QStringLiteral("arrow-left"), tx("Назад ко входу"), false, m_noNetwork);
    for (QPushButton *b : {wifi, retry, m_offline, back})
        v->addWidget(b);
    addHint(v, m_noNetwork, tx("«Подключить Wi-Fi» открывает системный выбор сети PocketBook."));
    connect(wifi, &QPushButton::clicked, this, [this] {
        device::openNetworkDialog();
        connectServer();
    });
    connect(retry, &QPushButton::clicked, this, [this] { connectServer(); });
    connect(m_offline, &QPushButton::clicked, this, [this] { m_app->openOffline(); });
    connect(back, &QPushButton::clicked, this, [this] { showPage(Page::Login); });

    setTokenAuth(!saved.token.isEmpty());
    showPage(Page::Login);

    // The firmware keyboard covers the lower half: while a field is edited, the logo steps aside
    // so the fields and the connect button stay above it.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        const bool editing = now && qobject_cast<QLineEdit *>(now) && m_login->isAncestorOf(now);
        m_loginLogo->setVisible(!editing);
    });
}

QString ConnectScreen::describe() const
{
    const QWidget *w = m_pages->currentWidget();
    return QStringLiteral("page=%1 auth=%2")
        .arg(w == m_checking ? QStringLiteral("checking") : w == m_login ? QStringLiteral("login") : QStringLiteral("no-network"),
             m_useToken ? QStringLiteral("token") : QStringLiteral("password"));
}

void ConnectScreen::showPage(Page page)
{
    switch (page) {
    case Page::Checking: m_pages->setCurrentWidget(m_checking); break;
    case Page::Login: m_pages->setCurrentWidget(m_login); break;
    case Page::NoNetwork:
        m_offline->setVisible(m_app->canReadOffline());
        m_pages->setCurrentWidget(m_noNetwork);
        break;
    }
    device::requireFullUpdate();
    Log::write(QStringLiteral("screen: connect %1").arg(describe()));
}

void ConnectScreen::setTokenAuth(bool token)
{
    m_useToken = token;
    m_user->setVisible(!token);
    m_passwordRow->setVisible(!token);
    m_token->setVisible(token);
    m_switchAuth->setText(token ? tx("Войти по логину и паролю") : tx("Войти по ключу API"));
}

void ConnectScreen::connectServer()
{
    for (QLineEdit *e : {m_server, m_user, m_password, m_token})
        Ui::hideKeyboard(e);
    QString server = m_server->text().trimmed();
    if (server.isEmpty()) {
        m_error->setText(tx("Введите адрес сервера"));
        m_error->show();
        showPage(Page::Login);
        return;
    }
    if (!server.contains(QLatin1String("://")))
        server.prepend(QStringLiteral("https://"));
    MinifluxAuth auth;
    auth.server = QUrl(server);
    if (m_useToken) {
        auth.token = m_token->text().trimmed();
    } else {
        auth.user = m_user->text().trimmed();
        auth.password = m_password->text();
    }
    m_auth = auth;
    m_error->hide();
    m_steps->setText(tx("Wi-Fi …"));
    showPage(Page::Checking);
    QPointer<ConnectScreen> self(this);
    m_app->connectMiniflux(
        auth,
        [self](QString step) {
            if (self)
                self->m_steps->setText(step);
        },
        [self](ConnectResult result, QString error) {
            if (!self)
                return;
            switch (result) {
            case ConnectResult::Ok:
                return; // the issue replaces this screen
            case ConnectResult::NoNetwork:
                self->m_noNetworkText->setText(tx("Сервер %1 не отвечает.").arg(self->m_auth.server.host()));
                self->showPage(Page::NoNetwork);
                return;
            case ConnectResult::Failed:
                self->m_error->setText(error);
                self->m_error->show();
                self->showPage(Page::Login);
                return;
            }
        });
}
