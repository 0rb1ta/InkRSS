#include "Opml.h"

#include <QXmlStreamReader>

QList<OpmlFeed> parseOpml(const QByteArray &xml)
{
    QList<OpmlFeed> out;
    QXmlStreamReader r(xml);
    QStringList folders; // one entry per open outline: its name if it is a folder, else empty
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement() && r.name() == QLatin1String("outline")) {
            if (!folders.isEmpty())
                folders.removeLast();
            continue;
        }
        if (!r.isStartElement() || r.name() != QLatin1String("outline"))
            continue;
        const auto a = r.attributes();
        QString title = a.value(QLatin1String("title")).toString();
        if (title.isEmpty())
            title = a.value(QLatin1String("text")).toString();
        const QString url = a.value(QLatin1String("xmlUrl")).toString();
        QString top;
        for (const QString &f : folders)
            if (!f.isEmpty()) {
                top = f;
                break;
            }
        if (!url.isEmpty()) {
            out << OpmlFeed{title.isEmpty() ? url : title, url, top};
            folders << QString();
        } else {
            folders << title;
        }
    }
    if (r.hasError())
        return {};
    return out;
}
