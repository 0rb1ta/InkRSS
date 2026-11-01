#pragma once

#include "Config.h"

#include <QPoint>
#include <QWidget>

#include <functional>
#include <memory>

class ImageCache;
class MinifluxApi;
class MinifluxSource;
class Navigator;
class NetworkGate;
class QtHttpTransport;
class RetryTransport;
class Source;
class Store;
class UpdateCheck;

// What a list screen shows: the morning issue, one feed, one folder or the starred articles.
struct ListScope {
    enum Kind { Issue, Feed, Category, Starred };
    Kind kind = Issue;
    qint64 id = -1;
    QString title;
};

enum class ConnectResult { Ok, NoNetwork, Failed };

// Owns the services and the screen stack; screens ask it to navigate and to sync.
class AppController : public QWidget {
    Q_OBJECT
public:
    explicit AppController(QWidget *parent = nullptr);
    ~AppController() override;
    void start();

    Store &store() { return *m_store; }
    const Config &config() const { return m_config; }
    bool updateConfig(const Config &config);
    bool fullText() const;
    QString articlesDir() const;
    QString imagesDir() const;
    // Unsubscribes (no-server mode) and deletes the feed's article pages and unused images.
    void removeFeed(qint64 feedId);
    QString storageRoot() const;
    bool syncing() const { return m_syncing; }
    UpdateCheck *updates() const { return m_updates; } // a newer release on GitHub, checked once per start

    // Brings Wi-Fi up, sends queued marks and downloads new articles. `done` gets an error or "".
    void sync(std::function<void(int done, int total)> progress, std::function<void(QString error)> done);
    void fetchFullText(qint64 id, std::function<void(QString error)> done);
    // Checks the server and login; on success saves the settings and opens the issue. `stage`
    // gets the progress line ("Wi-Fi ✓ · сервер …").
    void connectMiniflux(const MinifluxAuth &auth, std::function<void(QString stage)> stage,
                         std::function<void(ConnectResult result, QString error)> done);
    // Articles from an earlier Miniflux connection are on the reader and can be read without a network.
    bool canReadOffline() const;
    void openOffline();
    void startDirect();
    void resetConnection();
    // Applies the language setting (or the firmware's) and redraws the screens in it.
    void applyLanguage(bool redraw);

    void openArticle(qint64 id);
    void openList(const ListScope &scope);
    void openFeeds();
    void openSettings();
    void showMenu(QWidget *anchor); // the main menu, pinned under the ☰ button
    void back();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool busy(); // a sync is running: says so on screen
    void ensureNetwork(std::function<void(bool ok)> done);
    void migrateLegacyAccount(const QString &profile);
    bool buildSource();
    void showIssue(bool syncNow);
    void handleKey(int key);

    Config m_config;
    QString m_configPath;
    QString m_articlesDir;
    std::unique_ptr<QtHttpTransport> m_transport;
    std::unique_ptr<RetryTransport> m_net; // every request goes through it
    std::unique_ptr<NetworkGate> m_gate;
    std::unique_ptr<Store> m_store;
    std::unique_ptr<MinifluxApi> m_api;
    std::unique_ptr<ImageCache> m_images;
    std::unique_ptr<Source> m_source;
    MinifluxSource *m_miniflux = nullptr; // m_source when it is Miniflux
    Navigator *m_nav = nullptr;
    bool m_syncing = false;
    UpdateCheck *m_updates = nullptr;
    bool m_updatesChecked = false;
    int m_lastKey = 99;
    qint64 m_lastKeyAt = 0;
    QPoint m_pressPos;
    qint64 m_pressAt = 0;
};
