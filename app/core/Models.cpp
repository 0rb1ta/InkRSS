#include "Models.h"

#include <QRegularExpression>

QString plainText(const QString &html)
{
    static const QRegularExpression hidden(QStringLiteral("<(script|style)\\b[^>]*>.*?</\\1\\s*>"),
                                           QRegularExpression::CaseInsensitiveOption
                                               | QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    static const QRegularExpression entity(QStringLiteral("&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+);"));
    QString s = html;
    s.remove(hidden);
    s.replace(tag, QStringLiteral(" "));

    QString out;
    out.reserve(s.size());
    int pos = 0;
    auto it = entity.globalMatch(s);
    while (it.hasNext()) {
        const auto m = it.next();
        out += s.midRef(pos, m.capturedStart() - pos);
        const QString name = m.captured(1);
        uint code = 0;
        if (name.startsWith(QLatin1String("#x")))
            code = name.mid(2).toUInt(nullptr, 16);
        else if (name.startsWith(QLatin1Char('#')))
            code = name.mid(1).toUInt();
        else if (name == QLatin1String("amp")) code = '&';
        else if (name == QLatin1String("lt")) code = '<';
        else if (name == QLatin1String("gt")) code = '>';
        else if (name == QLatin1String("quot")) code = '"';
        else if (name == QLatin1String("apos")) code = '\'';
        else if (name == QLatin1String("nbsp")) code = ' ';
        if (code > 0 && code <= 0x10FFFF)
            out += QString::fromUcs4(&code, 1);
        else
            out += m.captured(0);
        pos = m.capturedEnd();
    }
    out += s.midRef(pos);
    return out.simplified();
}

int readingMinutes(const QString &html)
{
    static const QRegularExpression space(QStringLiteral("\\s+"));
    const QString text = plainText(html);
    if (text.isEmpty())
        return 0;
    const int words = text.split(space, QString::SkipEmptyParts).size();
    return qMax(1, (words + 199) / 200);
}

int textLength(const QString &html)
{
    return plainText(html).size();
}
