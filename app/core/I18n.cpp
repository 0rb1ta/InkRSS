#include "I18n.h"

#include <QHash>

namespace {

bool g_english = false;

const QHash<QString, QString> &table()
{
    static const QHash<QString, QString> t{
        {QStringLiteral("Иконка добавлена. Перезагрузите читалку, чтобы она появилась в меню приложений."), QStringLiteral("Icon added. Restart the reader to see it in the Applications menu.")},
        {QStringLiteral("Иконка убрана. Перезагрузите читалку."), QStringLiteral("Icon removed. Restart the reader.")},
        {QStringLiteral("Не найден файл меню приложений (system/config/desktop/view.json). Ничего не изменено."), QStringLiteral("The Applications menu file (system/config/desktop/view.json) was not found. Nothing was changed.")},
        {QStringLiteral("Файл меню приложений не удалось разобрать. Ничего не изменено."), QStringLiteral("The Applications menu file could not be read. Nothing was changed.")},
        {QStringLiteral("Нет файлов иконки в applications/icons. Скопируйте их из архива релиза."), QStringLiteral("No icon files in applications/icons. Copy them from the release archive.")},
        {QStringLiteral("Не удалось записать файл меню приложений."), QStringLiteral("Could not write the Applications menu file.")},
        {QStringLiteral("Нет связи с интернетом"), QStringLiteral("No internet connection")},
        {QStringLiteral("Нет связи"), QStringLiteral("No connection")},
        {QStringLiteral("Ошибка сайта (HTTP %1)"), QStringLiteral("Site error (HTTP %1)")},
        {QStringLiteral("Пустой ответ или не XML"), QStringLiteral("Empty response or not XML")},
        {QStringLiteral("По адресу не RSS-лента"), QStringLiteral("Not an RSS feed at this address")},
        {QStringLiteral("Лента повреждена: %1"), QStringLiteral("Broken feed: %1")},
        {QStringLiteral("Без папки"), QStringLiteral("No folder")},
        {QStringLiteral("≈ %1 мин"), QStringLiteral("≈ %1 min")},
        {QStringLiteral("≈ %1 ч %2 мин"), QStringLiteral("≈ %1 h %2 min")},
        {QStringLiteral("≈ %1 ч"), QStringLiteral("≈ %1 h")},
        {QStringLiteral("Нет связи с сервером Miniflux"), QStringLiteral("Cannot reach the Miniflux server")},
        {QStringLiteral("Сервер не принял логин, пароль или ключ API"), QStringLiteral("The server rejected the login, password or API key")},
        {QStringLiteral("По этому адресу не отвечает Miniflux"), QStringLiteral("No Miniflux at this address")},
        {QStringLiteral("Ошибка на сервере Miniflux"), QStringLiteral("Miniflux server error")},
        {QStringLiteral("Сервер отклонил изменение"), QStringLiteral("The server refused the change")},
        {QStringLiteral("Нет Wi-Fi"), QStringLiteral("No Wi-Fi")},
        {QStringLiteral("Меню"), QStringLiteral("Menu")},
        {QStringLiteral("Избранное · %1"), QStringLiteral("Starred · %1")},
        {QStringLiteral("Избранное"), QStringLiteral("Starred")},
        {QStringLiteral("Ленты и папки"), QStringLiteral("Feeds and folders")},
        {QStringLiteral("Настройки"), QStringLiteral("Settings")},
        {QStringLiteral("Пусто"), QStringLiteral("Empty")},
        {QStringLiteral("Закрыть"), QStringLiteral("Close")},
        {QStringLiteral("Отмена"), QStringLiteral("Cancel")},
        {QStringLiteral("По умолчанию"), QStringLiteral("Default")},
        {QStringLiteral("по умолчанию"), QStringLiteral("default")},
        {QStringLiteral("Шрифт: %1"), QStringLiteral("Font: %1")},
        {QStringLiteral("Размер: %1"), QStringLiteral("Size: %1")},
        {QStringLiteral("Убрать из избранного"), QStringLiteral("Unstar")},
        {QStringLiteral("В избранное"), QStringLiteral("Star")},
        {QStringLiteral("Оставить непрочитанной"), QStringLiteral("Keep unread")},
        {QStringLiteral("QR-код ссылки"), QStringLiteral("Link as QR code")},
        {QStringLiteral("Закрыть статью"), QStringLiteral("Close article")},
        {QStringLiteral("Шрифт статьи"), QStringLiteral("Article font")},
        {QStringLiteral("Размер шрифта статьи"), QStringLiteral("Article font size")},
        {QStringLiteral("Снимок экрана сохранён"), QStringLiteral("Screenshot saved")},
        {QStringLiteral("Размер шрифта"), QStringLiteral("Font size")},
        {QStringLiteral("Убрано из избранного"), QStringLiteral("Unstarred")},
        {QStringLiteral("В избранном"), QStringLiteral("Starred")},
        {QStringLiteral("Наведите камеру телефона"), QStringLiteral("Point your phone camera here")},
        {QStringLiteral("Подключение"), QStringLiteral("Connection")},
        {QStringLiteral("Без сервера"), QStringLiteral("No server")},
        {QStringLiteral("Адрес сервера"), QStringLiteral("Server address")},
        {QStringLiteral("Логин и пароль"), QStringLiteral("Login and password")},
        {QStringLiteral("Ключ API"), QStringLiteral("API key")},
        {QStringLiteral("Логин"), QStringLiteral("Login")},
        {QStringLiteral("Пароль"), QStringLiteral("Password")},
        {QStringLiteral("Ключ API (Miniflux → Настройки → Ключи API)"), QStringLiteral("API key (Miniflux → Settings → API Keys)")},
        {QStringLiteral("Подключиться"), QStringLiteral("Connect")},
        {QStringLiteral("Можно не вводить на экране: положите config.json в system/inkrss/ через USB."), QStringLiteral("No need to type it here: put config.json into system/inkrss/ over USB.")},
        {QStringLiteral("Ленты скачиваются прямо с сайтов, без полного текста статей. Список лент можно взять из файла .opml в корне памяти читалки."), QStringLiteral("Feeds are fetched straight from the sites, without full article text. The feed list can come from an .opml file in the root of the reader's storage.")},
        {QStringLiteral("Начать"), QStringLiteral("Start")},
        {QStringLiteral("Введите адрес сервера"), QStringLiteral("Enter the server address")},
        {QStringLiteral("Подключение…"), QStringLiteral("Connecting…")},
        {QStringLiteral("Адрес RSS-ленты"), QStringLiteral("RSS feed address")},
        {QStringLiteral("Добавить"), QStringLiteral("Add")},
        {QStringLiteral("Лент нет. Добавьте их на сайте Miniflux и обновите."), QStringLiteral("No feeds. Add them in Miniflux and refresh.")},
        {QStringLiteral("Лент нет. Нажмите «+» или загрузите OPML-файл."), QStringLiteral("No feeds. Tap “+” or load an OPML file.")},
        {QStringLiteral("Лента добавлена; статьи появятся после обновления"), QStringLiteral("Feed added; articles will appear after a refresh")},
        {QStringLiteral("Положите файл .opml в корень памяти читалки"), QStringLiteral("Put an .opml file into the root of the reader's storage")},
        {QStringLiteral("Добавлено лент: %1"), QStringLiteral("Feeds added: %1")},
        {QStringLiteral("%1 мин"), QStringLiteral("%1 min")},
        {QStringLiteral("Отписаться от ленты «%1»?"), QStringLiteral("Unsubscribe from “%1”?")},
        {QStringLiteral("Отписаться"), QStringLiteral("Unsubscribe")},
        {QStringLiteral("Вышла InkRSS %1"), QStringLiteral("InkRSS %1 is out")},
        {QStringLiteral("У вас версия %1"), QStringLiteral("You have version %1")},
        {QStringLiteral("Откройте страницу релиза на телефоне или компьютере, скачайте архив и скопируйте его на читалку по USB поверх старых файлов. Подключение и статьи сохранятся."), QStringLiteral("Open the release page on a phone or computer, download the archive and copy it onto the reader over USB on top of the old files. Your connection and articles stay.")},
        {QStringLiteral("Отладка"), QStringLiteral("Debugging")},
        {QStringLiteral("Включена"), QStringLiteral("On")},
        {QStringLiteral("Выключена"), QStringLiteral("Off")},
        {QStringLiteral("Для разбора ошибок: каждый открытый экран сохраняется картинкой в system/inkrss/shots/, журнал system/inkrss/log.txt пишется подробнее. Обычно не нужна."), QStringLiteral("For troubleshooting: every screen opened is saved as a picture in system/inkrss/shots/, and system/inkrss/log.txt gets more detail. Usually not needed.")},
        {QStringLiteral("Текст"), QStringLiteral("Text")},
        {QStringLiteral("Заголовок"), QStringLiteral("Title")},
        {QStringLiteral("Размер текста выпуска"), QStringLiteral("Issue text size")},
        {QStringLiteral("Размер заголовка статьи"), QStringLiteral("Article title size")},
        {QStringLiteral("Шрифт"), QStringLiteral("Font")},
        {QStringLiteral("Готово"), QStringLiteral("Done")},
        {QStringLiteral("Размер текста"), QStringLiteral("Text size")},
        {QStringLiteral("Размер заголовков"), QStringLiteral("Headline size")},
        {QStringLiteral("Размер заголовков выпуска"), QStringLiteral("Issue headline size")},
        {QStringLiteral("Wi-Fi …"), QStringLiteral("Wi-Fi …")},
        {QStringLiteral("Wi-Fi ✓ · сервер …"), QStringLiteral("Wi-Fi ✓ · server …")},
        {QStringLiteral("Адрес сервера, например rss.example.org"), QStringLiteral("Server address, e.g. rss.example.org")},
        {QStringLiteral("Без сервера: ленты напрямую"), QStringLiteral("No server: feeds directly")},
        {QStringLiteral("Без сети: скачанные статьи"), QStringLiteral("Offline: downloaded articles")},
        {QStringLiteral("Войти по ключу API"), QStringLiteral("Sign in with an API key")},
        {QStringLiteral("Войти по логину и паролю"), QStringLiteral("Sign in with login and password")},
        {QStringLiteral("Назад ко входу"), QStringLiteral("Back to sign-in")},
        {QStringLiteral("Нет подключения к интернету"), QStringLiteral("No internet connection")},
        {QStringLiteral("Отметить остальное прочитанным"), QStringLiteral("Mark the rest as read")},
        {QStringLiteral("Повторить"), QStringLiteral("Retry")},
        {QStringLiteral("Подключение к Miniflux"), QStringLiteral("Connect to Miniflux")},
        {QStringLiteral("Подключить Wi-Fi"), QStringLiteral("Connect Wi-Fi")},
        {QStringLiteral("«Подключить Wi-Fi» открывает системный выбор сети PocketBook."), QStringLiteral("“Connect Wi-Fi” opens the PocketBook network picker.")},
        {QStringLiteral("Проверяем сервер Miniflux…"), QStringLiteral("Checking the Miniflux server…")},
        {QStringLiteral("Шрифт и размер"), QStringLiteral("Font and size")},
        {QStringLiteral("Сервер %1 не отвечает."), QStringLiteral("Server %1 does not answer.")},
        {QStringLiteral("InkRSS %1 · сборка %2"), QStringLiteral("InkRSS %1 · build %2")},
        {QStringLiteral("Сервер"), QStringLiteral("Server")},
        {QStringLiteral("Лент пока нет.\nМеню → «Ленты и папки» → «+»"), QStringLiteral("No feeds yet.\nMenu → “Feeds and folders” → “+”")},
        {QStringLiteral("Новых статей нет.\nНажмите «Обновить» вверху."), QStringLiteral("No new articles.\nTap Refresh at the top.")},
        {QStringLiteral("Здесь пока пусто"), QStringLiteral("Nothing here yet")},
        {QStringLiteral("Ещё не обновлялось"), QStringLiteral("Never refreshed")},
        {QStringLiteral("Обновлено в %1"), QStringLiteral("Refreshed at %1")},
        {QStringLiteral("Обновлено %1"), QStringLiteral("Refreshed %1")},
        {QStringLiteral("%1 новых"), QStringLiteral("%1 new")},
        {QStringLiteral("Обновление…"), QStringLiteral("Refreshing…")},
        {QStringLiteral("Обновление… %1 / %2"), QStringLiteral("Refreshing… %1 / %2")},
        {QStringLiteral("Отметить все статьи выпуска прочитанными?"), QStringLiteral("Mark every article in the issue as read?")},
        {QStringLiteral("Отметить"), QStringLiteral("Mark")},
        {QStringLiteral("Отметить всю ленту прочитанной?"), QStringLiteral("Mark the whole feed as read?")},
        {QStringLiteral("Отметить всю папку прочитанной?"), QStringLiteral("Mark the whole folder as read?")},
        {QStringLiteral("Всё прочитано"), QStringLiteral("All read")},
        {QStringLiteral("Выпуск прочитан"), QStringLiteral("Issue finished")},
        {QStringLiteral("Просмотрено %1, прочитано %2."), QStringLiteral("%1 seen, %2 read.")},
        {QStringLiteral(" Отметить остальное прочитанным"), QStringLiteral(" Mark the rest as read")},
        {QStringLiteral(" Обновить"), QStringLiteral(" Refresh")},
        {QStringLiteral("Обновить"), QStringLiteral("Refresh")},
        {QStringLiteral("ключ API"), QStringLiteral("API key")},
        {QStringLiteral("Сервер: %1 · %2"), QStringLiteral("Server: %1 · %2")},
        {QStringLiteral("Статей за обновление"), QStringLiteral("Articles per refresh")},
        {QStringLiteral("Вступит в силу после перезапуска"), QStringLiteral("Takes effect after a restart")},
        {QStringLiteral("Без сервера: лент %1"), QStringLiteral("No server: %1 feeds")},
        {QStringLiteral("Импорт OPML из корня памяти"), QStringLiteral("Import OPML from the storage root")},
        {QStringLiteral("Найти файл"), QStringLiteral("Find file")},
        {QStringLiteral("Файл .opml не найден"), QStringLiteral("No .opml file found")},
        {QStringLiteral("Главный экран"), QStringLiteral("Main screen")},
        {QStringLiteral("Просто лента"), QStringLiteral("Plain list")},
        {QStringLiteral("Выпуск"), QStringLiteral("Issue")},
        {QStringLiteral("Выпуск — главная статья и разделы по папкам"), QStringLiteral("Issue — lead story and sections by folder")},
        {QStringLiteral("Просто лента — все статьи подряд, новые сверху"), QStringLiteral("Plain list — all articles, newest first")},
        {QStringLiteral("Картинки в статьях"), QStringLiteral("Images in articles")},
        {QStringLiteral("Включены"), QStringLiteral("On")},
        {QStringLiteral("Выключены"), QStringLiteral("Off")},
        {QStringLiteral("Картинки скачаются при следующем обновлении"), QStringLiteral("Images will download on the next refresh")},
        {QStringLiteral("Картинки больше не скачиваются и не показываются"), QStringLiteral("Images are no longer downloaded or shown")},
        {QStringLiteral("Шрифт и размер статьи"), QStringLiteral("Article font and size")},
        {QStringLiteral("В меню статьи"), QStringLiteral("In the article menu")},
        {QStringLiteral("Откройте статью и нажмите в центр страницы"), QStringLiteral("Open an article and tap the middle of the page")},
        {QStringLiteral("Значок в «Приложениях»"), QStringLiteral("Icon in Applications")},
        {QStringLiteral("Убрать"), QStringLiteral("Remove")},
        {QStringLiteral("Выйти / сменить режим"), QStringLiteral("Sign out / change mode")},
        {QStringLiteral("Сменить режим"), QStringLiteral("Change mode")},
        {QStringLiteral("Сменить"), QStringLiteral("Change")},
        {QStringLiteral("Отключиться и вернуться к выбору режима? Скачанные статьи останутся."), QStringLiteral("Disconnect and go back to choosing the mode? Downloaded articles stay.")},
        {QStringLiteral("Отключиться"), QStringLiteral("Disconnect")},
        {QStringLiteral("Язык"), QStringLiteral("Language")},
        {QStringLiteral("‹ Назад"), QStringLiteral("‹ Back")},
        {QStringLiteral("Меню ☰"), QStringLiteral("Menu ☰")},
        {QStringLiteral("Ссылка"), QStringLiteral("Link")},
        {QStringLiteral("Дождитесь конца обновления"), QStringLiteral("Wait for the refresh to finish")},
        {QStringLiteral("Русский"), QStringLiteral("Русский")},
        {QStringLiteral("Как в прошивке"), QStringLiteral("As in the firmware")},
        {QStringLiteral("статья"), QStringLiteral("article")},
        {QStringLiteral("статей"), QStringLiteral("articles")},
    };
    return t;
}

} // namespace

void I18n::setLanguage(const QString &code)
{
    g_english = code == QLatin1String("en");
}

bool I18n::english()
{
    return g_english;
}

QString I18n::fromFirmware(const QString &code)
{
    if (code.isEmpty() || code.startsWith(QLatin1String("ru")))
        return QStringLiteral("ru");
    return QStringLiteral("en");
}

QString I18n::english(const QString &russian)
{
    return table().value(russian);
}

QString tx(const char *russian)
{
    const QString ru = QString::fromUtf8(russian);
    if (!g_english)
        return ru;
    const QString en = table().value(ru);
    return en.isEmpty() ? ru : en;
}

QString txn(int n, const char *one, const char *few, const char *many)
{
    if (g_english)
        return tx(n == 1 ? one : many);
    const int mod100 = n % 100;
    const int mod10 = n % 10;
    if (mod10 == 1 && mod100 != 11)
        return QString::fromUtf8(one);
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        return QString::fromUtf8(few);
    return QString::fromUtf8(many);
}
