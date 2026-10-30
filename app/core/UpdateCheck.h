#pragma once

#include <QObject>
#include <QString>

class HttpTransport;

// Asks GitHub once per start whether a newer InkRSS release exists. Network errors, missing
// releases (404) and rate limits are silent: the check is only a hint in the home header.
class UpdateCheck : public QObject {
    Q_OBJECT
public:
    UpdateCheck(HttpTransport *transport, const QString &currentVersion, QObject *parent = nullptr);
    void run();
    bool available() const { return !m_latest.isEmpty(); }
    QString latestVersion() const { return m_latest; }
    QString releaseUrl() const { return m_url; }

    // <0, 0, >0 like strcmp; "v1.2" == "1.2.0", non-numeric parts compare as 0.
    static int compareVersions(const QString &a, const QString &b);

signals:
    void found();

private:
    HttpTransport *m_transport;
    QString m_current;
    QString m_latest;
    QString m_url;
};
