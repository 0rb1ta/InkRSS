#include "AppController.h"
#include "I18n.h"

#include "ArticleHtml.h"
#include "ArticleScreen.h"
#include "ConnectScreen.h"
#include "Device.h"
#include "DirectSource.h"
#include "FeedsScreen.h"
#include "ImageCache.h"
#include "IssueScreen.h"
#include "Log.h"
#include "MinifluxApi.h"
#include "MinifluxSource.h"
#include "NetworkGate.h"
#include "Paths.h"
#include "QtHttpTransport.h"
#include "RetryTransport.h"
#include "SettingsScreen.h"
#include "Shots.h"
#include "Store.h"
#include "Theme.h"
#include "UpdateCheck.h"
#include "Widgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

AppController::AppController(QWidget *parent)
    : QWidget(parent)
{
    QApplication::setFont(Theme::bodyFont());
    QApplication::setFont(Theme::smallFont(), "QPushButton");
    // showFullScreen() alone leaves the window at its size hint on this firmware (a narrow strip on
    // a grey screen): the window is fixed to the screen, and no page may stretch it.
    const QScreen *screen = QGuiApplication::primaryScreen();
    const QRect sg = screen ? screen->geometry() : QRect(0, 0, 825, 1200);
    setFixedSize(sg.width(), sg.height());
    Log::write(QStringLiteral("screen %1x%2, firmware dpi %3, panel %4, mm(8)=%5px")
                   .arg(sg.width()).arg(sg.height()).arg(device::screenDpi()).arg(device::panelHeight()).arg(Theme::mm(8)));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, qMax(0, device::panelHeight()));
    m_nav = new Navigator(this);
    m_nav->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    v->addWidget(m_nav);

    m_configPath = Paths::dataDir() + QStringLiteral("/config.json");
    m_transport.reset(new QtHttpTransport(QCoreApplication::applicationDirPath() + QStringLiteral("/cacert.pem")));
    m_net.reset(new RetryTransport(*m_transport));
    m_updates = new UpdateCheck(m_net.get(), QStringLiteral(APP_VERSION), this);
    m_gate.reset(new NetworkGate([] { return device::networkConnected(); },
                                 [](std::function<void(bool)> done) { device::connectNetworkAsync(std::move(done)); },
                                 30000, 3000));
    qApp->installEventFilter(this);
    device::installKeyHandler([this](int key) { handleKey(key); });
    // Back goes to the previous screen (or closes a dialog); on the first screen the firmware
    // handles it as usual.
    device::installBackFilter([this] {
        return m_nav->depth() > 1 || QApplication::activeModalWidget() || QApplication::activePopupWidget();
    });
}

AppController::~AppController() = default;

void AppController::start()
{
    m_config = loadConfig(m_configPath);
    applyLanguage(false);
    if (m_config.mode == Config::None) {
        m_nav->setRoot(new ConnectScreen(this));
        return;
    }
    buildSource();
    showIssue(false);
}

QString AppController::storageRoot() const
{
    return QFileInfo(QStringLiteral("/mnt/ext1")).isDir() ? QStringLiteral("/mnt/ext1") : QDir::homePath();
}

void AppController::removeFeed(qint64 feedId)
{
    removeArticleFiles(articlesDir(), m_store->removeFeed(feedId));
    removeUnusedImages(articlesDir(), imagesDir());
}

QString AppController::imagesDir() const
{
    return Paths::dataDir() + QStringLiteral("/images");
}

QString AppController::articlesDir() const
{
    return Paths::dataDir() + QStringLiteral("/articles");
}

bool AppController::fullText() const
{
    return m_source && m_source->fullText();
}

void AppController::updateConfig(const Config &config)
{
    m_config = config;
    saveConfig(m_configPath, m_config);
    if (m_miniflux)
        m_miniflux->setLimit(m_config.limit); // read at the start of each sync
}

bool AppController::busy()
{
    // Sources, store and image cache are rebuilt below; a running sync still holds them.
    if (!m_syncing)
        return false;
    Ui::toast(this, tx("Дождитесь конца обновления"));
    return true;
}

void AppController::buildSource()
{
    m_source.reset();
    m_miniflux = nullptr;
    m_api.reset();
    m_store.reset(new Store);
    const QString data = Paths::dataDir();
    const bool miniflux = m_config.mode == Config::Miniflux;
    m_store->open(data + (miniflux ? QStringLiteral("/feeds-miniflux.db") : QStringLiteral("/feeds-direct.db")));
    // Images no wider than the text column, so the article view never has to scale them.
    const QSize screen = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->size() : QSize(1200, 1600);
    m_images.reset(new ImageCache(*m_net, imagesDir(), screen.width() - 2 * Theme::mm(6)));
    // The images setting can change at any time; it is read for each article.
    const PrepareFn withImages = makePrepare(*m_images, articlesDir(), true);
    const PrepareFn textOnly = makePrepare(*m_images, articlesDir(), false);
    PrepareFn prepare = [this, withImages, textOnly](const Entry &e, std::function<void()> done) {
        (m_config.images ? withImages : textOnly)(e, std::move(done));
    };
    const QString dir = articlesDir();
    RemovedFn removed = [dir](const QList<qint64> &ids) { removeArticleFiles(dir, ids); };
    ReadyFn ready = [dir](qint64 id) { return QFile::exists(articlePath(dir, id)); };
    if (miniflux) {
        m_api.reset(new MinifluxApi(*m_net, m_config.auth));
        m_miniflux = new MinifluxSource(*m_api, m_config.limit, prepare, removed, ready);
        m_source.reset(m_miniflux);
    } else {
        m_source.reset(new DirectSource(*m_net, prepare, removed, ready));
    }
}

void AppController::showIssue(bool syncNow)
{
    auto *issue = new IssueScreen(this, ListScope());
    m_nav->setRoot(issue);
    device::requireFullUpdate();
    if (syncNow)
        issue->startSync();
}

void AppController::ensureNetwork(std::function<void(bool)> done)
{
    // The firmware reconnects quietly only to a network it already knows and only when Wi-Fi is
    // on. Otherwise the system network picker is shown once, then one more try.
    m_gate->run([this, done](bool ok) {
        Log::write(QStringLiteral("network: %1").arg(ok ? QStringLiteral("up") : QStringLiteral("no answer, opening the Wi-Fi picker")));
        if (ok)
            return done(true);
        device::openNetworkDialog();
        m_gate->run([done](bool again) {
            Log::write(QStringLiteral("network after picker: %1").arg(again ? QStringLiteral("up") : QStringLiteral("down")));
            done(again);
        });
    });
}

void AppController::sync(std::function<void(int, int)> progress, std::function<void(QString)> done)
{
    if (m_syncing || !m_source)
        return;
    m_syncing = true;
    ensureNetwork([this, progress, done](bool ok) {
        if (!ok) {
            m_syncing = false;
            done(tx("Нет Wi-Fi"));
            return;
        }
        m_source->sync(*m_store, progress, [this, done](QString error) {
            m_syncing = false;
            const ImageStats st = m_images->takeStats();
            const int images = removeUnusedImages(articlesDir(), imagesDir());
            Log::write(QStringLiteral("sync: images fetched=%1 failed=%2 KB=%3, cleanup images=%4")
                           .arg(st.fetched).arg(st.failed).arg(st.bytes / 1024).arg(images));
            Log::write(QStringLiteral("sync done: %1").arg(error.isEmpty() ? QStringLiteral("ok") : error));
            if (error.isEmpty() && !m_updatesChecked) {
                m_updatesChecked = true;
                m_updates->run(); // once per start, while the network is up
            }
            done(error);
        });
    });
}

void AppController::connectMiniflux(const MinifluxAuth &auth, std::function<void(QString)> stage,
                                    std::function<void(ConnectResult, QString)> done)
{
    if (busy())
        return done(ConnectResult::Failed, tx("Дождитесь конца обновления"));
    Log::write(QStringLiteral("connect: %1 by %2").arg(auth.server.host(),
                                                       auth.token.isEmpty() ? QStringLiteral("password") : QStringLiteral("token")));
    ensureNetwork([this, auth, stage, done](bool ok) {
        if (!ok)
            return done(ConnectResult::NoNetwork, QString());
        stage(tx("Wi-Fi ✓ · сервер …"));
        auto api = std::make_shared<MinifluxApi>(*m_net, auth);
        api->me([this, api, auth, done](ApiFail fail, QString user) {
            Log::write(QStringLiteral("connect: %1").arg(fail == ApiFail::None ? QStringLiteral("ok") : failText(fail)));
            if (fail == ApiFail::Network)
                return done(ConnectResult::NoNetwork, failText(fail));
            if (fail != ApiFail::None)
                return done(ConnectResult::Failed, failText(fail));
            Log::write(QStringLiteral("connected to Miniflux as %1").arg(user));
            Config c = m_config;
            c.mode = Config::Miniflux;
            c.auth = auth;
            updateConfig(c);
            buildSource();
            done(ConnectResult::Ok, QString());
            showIssue(true);
        });
    });
}

bool AppController::canReadOffline() const
{
    if (m_config.auth.server.isEmpty())
        return false;
    Store store;
    return store.open(Paths::dataDir() + QStringLiteral("/feeds-miniflux.db")) && !store.allIds().isEmpty();
}

void AppController::openOffline()
{
    if (busy())
        return;
    Config c = m_config;
    c.mode = Config::Miniflux;
    updateConfig(c);
    buildSource();
    showIssue(false);
}

void AppController::startDirect()
{
    if (busy())
        return;
    Config c = m_config;
    c.mode = Config::Direct;
    updateConfig(c);
    buildSource();
    const QString opml = DirectSource::findOpml(storageRoot());
    if (!opml.isEmpty()) {
        QFile f(opml);
        if (f.open(QIODevice::ReadOnly)) {
            const int added = DirectSource::importOpml(*m_store, f.readAll());
            Log::write(QStringLiteral("imported %1 feeds from %2").arg(added).arg(opml));
        }
    }
    showIssue(!m_store->feeds().isEmpty());
}

void AppController::resetConnection()
{
    if (busy())
        return;
    Config c = m_config;
    c.mode = Config::None;
    updateConfig(c);
    m_source.reset();
    m_miniflux = nullptr;
    m_api.reset();
    m_nav->setRoot(new ConnectScreen(this));
    device::requireFullUpdate();
}

void AppController::applyLanguage(bool redraw)
{
    const QString code = m_config.language.isEmpty() ? I18n::fromFirmware(device::firmwareLanguage()) : m_config.language;
    I18n::setLanguage(code);
    Log::write(QStringLiteral("language %1 (firmware %2)").arg(code, device::firmwareLanguage()));
    if (!redraw)
        return;
    if (m_config.mode == Config::None)
        m_nav->setRoot(new ConnectScreen(this));
    else
        showIssue(false);
}

void AppController::openArticle(qint64 id)
{
    m_nav->push(new ArticleScreen(this, id));
}

void AppController::openList(const ListScope &scope)
{
    m_nav->push(new IssueScreen(this, scope));
}

void AppController::openFeeds()
{
    m_nav->push(new FeedsScreen(this));
}

void AppController::openSettings()
{
    m_nav->push(new SettingsScreen(this));
}

void AppController::showMenu(QWidget *anchor)
{
    const int starred = m_store ? m_store->starred().size() : 0;
    // "Шрифт и размер" opens in the menu itself; the issue behind it redraws after every change.
    auto apply = [this](std::function<void(Config &)> change) {
        Config next = m_config;
        change(next);
        updateConfig(next);
        if (Screen *s = m_nav->current())
            s->shown();
    };
    Ui::FontPanel font;
    font.family = m_config.fontFamily;
    font.fontChanged = [apply](const QString &family) { apply([family](Config &n) { n.fontFamily = family; }); };
    font.sizes = {{tx("Текст"), m_config.headlineSize, Config::kHeadlineSizeMin, Config::kHeadlineSizeMax,
                   [apply](int size) { apply([size](Config &n) { n.headlineSize = size; }); }}};
    const int choice = Ui::cornerMenu(anchor, {{QStringLiteral("star"), starred ? tx("Избранное · %1").arg(starred) : tx("Избранное")},
                                               {QStringLiteral("rss"), tx("Ленты и папки")},
                                               {QStringLiteral("font"), tx("Шрифт и размер")},
                                               {QStringLiteral("gear"), tx("Настройки")}},
                                      &font, 2);
    switch (choice) {
    case 0: {
        ListScope s;
        s.kind = ListScope::Starred;
        s.title = tx("Избранное");
        openList(s);
        break;
    }
    case 1: openFeeds(); break;
    case 3: openSettings(); break;
    default: break;
    }
}

void AppController::back()
{
    m_nav->pop();
}

void AppController::handleKey(int key)
{
    // Each hardware press arrives twice (InkView handler and Qt key event).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (key == m_lastKey && now - m_lastKeyAt < 300)
        return;
    m_lastKey = key;
    m_lastKeyAt = now;
    if (QWidget *popup = QApplication::activePopupWidget()) {
        if (key == device::KeyBack)
            popup->close();
        return;
    }
    if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
        if (key == device::KeyBack)
            dialog->reject(); // left alone, the firmware sends the app to the background
        return;
    }
    Log::write(QStringLiteral("key %1, depth %2").arg(key).arg(m_nav->depth()));
    if (key == device::KeyBack) {
        m_nav->pop(); // on the first screen it does nothing: leaving is the firmware's Home
        return;
    }
    if (Screen *s = m_nav->current())
        s->pageKey(key);
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Polish:
        // No keyboard focus on buttons: the focus frame was drawn smaller than the button.
        if (auto *button = qobject_cast<QAbstractButton *>(watched))
            button->setFocusPolicy(Qt::NoFocus);
        break;
    case QEvent::MouseButtonPress:
        m_pressPos = static_cast<QMouseEvent *>(event)->globalPos();
        m_pressAt = QDateTime::currentMSecsSinceEpoch();
        break;
    case QEvent::MouseButtonRelease: {
        // A long press on the bottom line (the page number) saves a picture of the screen.
        const QPoint at = static_cast<QMouseEvent *>(event)->globalPos();
        const int panelTop = mapToGlobal(QPoint(0, height() - qMax(0, device::panelHeight()))).y();
        if (!m_pressPos.isNull() && QDateTime::currentMSecsSinceEpoch() - m_pressAt >= 1500
            && at.y() >= panelTop - Theme::mm(9) && at.y() < panelTop && m_pressPos.y() >= panelTop - Theme::mm(9)) {
            m_pressPos = QPoint();
            Shots::save(this, m_nav->current() ? m_nav->current()->objectName() : QString());
            Ui::toast(this, tx("Снимок экрана сохранён"));
            return true;
        }
        // Which button was pressed, by name (never the text of an input field).
        if (auto *b = qobject_cast<QAbstractButton *>(watched))
            Log::write(QStringLiteral("tap: %1").arg(b->objectName().isEmpty() ? b->text().trimmed() : b->objectName()));
        // Horizontal swipe anywhere = page flip (right-to-left: next page).
        const QPoint d = static_cast<QMouseEvent *>(event)->globalPos() - m_pressPos;
        if (!m_pressPos.isNull() && qAbs(d.x()) > Theme::mm(15) && qAbs(d.x()) > 2 * qAbs(d.y())) {
            m_pressPos = QPoint();
            if (auto *b = qobject_cast<QAbstractButton *>(watched))
                b->setDown(false);
            handleKey(d.x() < 0 ? device::KeyNext : device::KeyPrev);
            return true;
        }
        break;
    }
    case QEvent::KeyRelease:
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent *>(event);
        const int k = ke->key();
        const bool pageOrBack = k == Qt::Key_PageDown || k == Qt::Key_PageUp || k == Qt::Key_Back || k == Qt::Key_Escape;
        // An unhandled key event travels up the parents and reaches this filter once per widget,
        // all within a millisecond or two; a held key repeats every few hundred.
        static int lastType = 0, lastKey = 0;
        static qint64 lastAt = 0;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (pageOrBack && !(lastType == event->type() && lastKey == k && nowMs - lastAt < 30)) {
            lastType = event->type();
            lastKey = k;
            lastAt = nowMs;
            Log::write(QStringLiteral("qt key %1 0x%2 repeat %3")
                           .arg(event->type() == QEvent::KeyPress ? QStringLiteral("press") : QStringLiteral("release"))
                           .arg(k, 0, 16).arg(ke->isAutoRepeat()));
        }
        if (event->type() == QEvent::KeyRelease)
            break;
        if (k == Qt::Key_PageDown || k == Qt::Key_PageUp) {
            handleKey(k == Qt::Key_PageDown ? device::KeyNext : device::KeyPrev);
            return true;
        }
        // The same press also comes from InkView; the 300 ms filter drops the copy.
        if (k == Qt::Key_Back || k == Qt::Key_Escape) {
            handleKey(device::KeyBack);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}
