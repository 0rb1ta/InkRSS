#include "DesktopIcon.h"
#include "I18n.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

const QString kKey = QStringLiteral("U_InkRSS"); // must match applications/InkRSS.app

bool read(const QString &path, QJsonObject *root, DesktopIcon::Result *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = DesktopIcon::Result::NoViewJson;
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = DesktopIcon::Result::BadJson;
        return false;
    }
    *root = doc.object();
    return true;
}

bool write(const QString &path, const QJsonObject &root)
{
    const QString backup = path + QStringLiteral(".inkrss-backup");
    if (!QFileInfo::exists(backup) && !QFile::copy(path, backup))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

// Calls edit(apps) for every group's "apps" array and stores the result back.
template <typename F>
void forEachGroup(QJsonObject &root, F edit)
{
    QJsonObject view = root.value(QStringLiteral("view")).toObject();
    QJsonArray groups = view.value(QStringLiteral("groups")).toArray();
    for (int i = 0; i < groups.size(); ++i) {
        QJsonObject g = groups[i].toObject();
        QJsonArray apps = g.value(QStringLiteral("apps")).toArray();
        if (edit(i, g, apps)) {
            g.insert(QStringLiteral("apps"), apps);
            groups[i] = g;
        }
    }
    view.insert(QStringLiteral("groups"), groups);
    root.insert(QStringLiteral("view"), view);
}

bool listed(const QJsonArray &apps)
{
    return apps.contains(QJsonValue(kKey));
}

} // namespace

QString DesktopIcon::viewJsonPath(const QString &storageRoot)
{
    return storageRoot + QStringLiteral("/system/config/desktop/view.json");
}

bool DesktopIcon::isInstalled(const QString &viewJson)
{
    QJsonObject root;
    Result ignored;
    return read(viewJson, &root, &ignored)
        && root.value(QStringLiteral("applications")).toObject().contains(kKey);
}

DesktopIcon::Result DesktopIcon::install(const QString &viewJson, const QString &storageRoot)
{
    const QString icon = storageRoot + QStringLiteral("/applications/icons/inkrss.bmp");
    const QString focused = storageRoot + QStringLiteral("/applications/icons/inkrss_f.bmp");
    if (!QFileInfo::exists(icon) || !QFileInfo::exists(focused))
        return Result::NoIcons;
    QJsonObject root;
    Result error;
    if (!read(viewJson, &root, &error))
        return error;

    QJsonObject apps = root.value(QStringLiteral("applications")).toObject();
    apps.insert(kKey, QJsonObject{
                          {QStringLiteral("path"), storageRoot + QStringLiteral("/applications/InkRSS.app")},
                          {QStringLiteral("title"), QStringLiteral("InkRSS")},
                          {QStringLiteral("icon"), icon},
                          {QStringLiteral("focused_icon"), focused},
                      });
    root.insert(QStringLiteral("applications"), apps);

    // Listed once: in "@General" if there is such a group, otherwise in the first one.
    bool already = false;
    int general = -1;
    forEachGroup(root, [&](int i, const QJsonObject &g, QJsonArray &list) {
        already |= listed(list);
        if (general < 0 && g.value(QStringLiteral("title")).toString() == QLatin1String("@General"))
            general = i;
        return false;
    });
    if (!already) {
        if (general < 0)
            general = 0;
        forEachGroup(root, [&](int i, const QJsonObject &, QJsonArray &list) {
            if (i != general)
                return false;
            list.append(kKey);
            return true;
        });
    }
    return write(viewJson, root) ? Result::Installed : Result::WriteFailed;
}

DesktopIcon::Result DesktopIcon::remove(const QString &viewJson)
{
    QJsonObject root;
    Result error;
    if (!read(viewJson, &root, &error))
        return error;
    QJsonObject apps = root.value(QStringLiteral("applications")).toObject();
    apps.remove(kKey);
    root.insert(QStringLiteral("applications"), apps);
    forEachGroup(root, [](int, const QJsonObject &, QJsonArray &list) {
        bool changed = false;
        for (int i = list.size() - 1; i >= 0; --i) {
            if (list[i].toString() == kKey) {
                list.removeAt(i);
                changed = true;
            }
        }
        return changed;
    });
    return write(viewJson, root) ? Result::Removed : Result::WriteFailed;
}

QString DesktopIcon::message(Result r)
{
    switch (r) {
    case Result::Installed:
        return tx("Иконка добавлена. Перезагрузите читалку, чтобы она появилась в меню приложений.");
    case Result::Removed:
        return tx("Иконка убрана. Перезагрузите читалку.");
    case Result::NoViewJson:
        return tx("Не найден файл меню приложений (system/config/desktop/view.json). Ничего не изменено.");
    case Result::BadJson:
        return tx("Файл меню приложений не удалось разобрать. Ничего не изменено.");
    case Result::NoIcons:
        return tx("Нет файлов иконки в applications/icons. Скопируйте их из архива релиза.");
    case Result::WriteFailed:
        return tx("Не удалось записать файл меню приложений.");
    }
    return QString();
}
