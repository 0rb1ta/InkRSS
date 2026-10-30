#include "SettingsScreen.h"
#include "I18n.h"

#include "DesktopIcon.h"
#include "DirectSource.h"
#include "Store.h"
#include "Log.h"
#include "Theme.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsScreen::SettingsScreen(AppController *app, QWidget *parent)
    : Screen(parent)
    , m_app(app)
{
    setObjectName(QStringLiteral("settings"));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *bar = new TopBar(tx("Настройки"), true, this);
    connect(bar, &TopBar::back, this, [this] { m_app->back(); });
    v->addWidget(bar);
    m_body = new QWidget(this);
    new QVBoxLayout(m_body);
    v->addWidget(m_body, 1);
}

void SettingsScreen::shown()
{
    rebuild();
}

void SettingsScreen::rebuild()
{
    QLayout *old = m_body->layout();
    while (QLayoutItem *item = old->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    auto *v = static_cast<QVBoxLayout *>(old);
    v->setContentsMargins(Theme::mm(4), Theme::mm(2), Theme::mm(4), Theme::mm(2));
    v->setSpacing(0);

    auto row = [&](const QString &name, const QString &button, std::function<void()> action) {
        auto *w = new QWidget(m_body);
        w->setStyleSheet(QStringLiteral("border-bottom: 1px solid #999;"));
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(0, Theme::mm(2), 0, Theme::mm(2));
        auto *l = new QLabel(name, w);
        l->setFont(Theme::bodyFont());
        l->setStyleSheet(QStringLiteral("border: none;"));
        l->setWordWrap(true);
        h->addWidget(l, 1);
        if (!button.isEmpty() && action) {
            QPushButton *b = Ui::chip(button, w);
            b->setObjectName(QStringLiteral("setting:") + name);
            connect(b, &QPushButton::clicked, this, [action] { action(); });
            h->addWidget(b);
        } else if (!button.isEmpty()) {
            auto *l = new QLabel(button, w);
            l->setFont(Theme::smallFont());
            l->setStyleSheet(QStringLiteral("border: none;"));
            h->addWidget(l);
        }
        v->addWidget(w);
    };

    Config c = m_app->config();
    if (c.mode == Config::Miniflux) {
        const QString who = c.auth.token.isEmpty() ? c.auth.user : tx("ключ API");
        row(tx("Сервер"), c.auth.server.host() + QStringLiteral(" · ") + who, {});
        row(tx("Статей за обновление"), QString::number(c.limit), [this] {
            const QList<int> values{100, 200, 300, 500};
            QStringList labels;
            for (int n : values)
                labels << QString::number(n);
            const int i = Ui::choose(this, tx("Статей за обновление"), labels);
            if (i < 0)
                return;
            Config next = m_app->config();
            next.limit = values[i];
            m_app->updateConfig(next);
            rebuild();
        });
    } else {
        row(tx("Без сервера: лент %1").arg(m_app->store().feeds().size()), QString(), {});
        row(tx("Импорт OPML из корня памяти"), tx("Найти файл"), [this] {
            const QString path = DirectSource::findOpml(m_app->storageRoot());
            QFile f(path);
            if (path.isEmpty() || !f.open(QIODevice::ReadOnly)) {
                Ui::toast(this, tx("Файл .opml не найден"));
                return;
            }
            Ui::toast(this, tx("Добавлено лент: %1").arg(DirectSource::importOpml(m_app->store(), f.readAll())));
            rebuild();
        });
    }
    row(tx("Главный экран"), c.flatIssue ? tx("Просто лента") : tx("Выпуск"), [this] {
        const int i = Ui::choose(this, tx("Главный экран"),
                                 {tx("Выпуск — главная статья и разделы по папкам"),
                                  tx("Просто лента — все статьи подряд, новые сверху")});
        if (i < 0)
            return;
        Config next = m_app->config();
        next.flatIssue = i == 1;
        m_app->updateConfig(next);
        rebuild();
    });
    row(tx("Картинки в статьях"), c.images ? tx("Включены") : tx("Выключены"), [this] {
        Config next = m_app->config();
        next.images = !next.images;
        m_app->updateConfig(next);
        Ui::toast(this, next.images ? tx("Картинки скачаются при следующем обновлении")
                                    : tx("Картинки больше не скачиваются и не показываются"));
        rebuild();
    });
    const QString lang = c.language.isEmpty() ? tx("Как в прошивке")
                                              : c.language == QLatin1String("en") ? QStringLiteral("English") : tx("Русский");
    row(tx("Язык"), lang, [this] {
        const int i = Ui::choose(this, tx("Язык"), {tx("Как в прошивке"), tx("Русский"), QStringLiteral("English")});
        if (i < 0)
            return;
        Config next = m_app->config();
        next.language = i == 0 ? QString() : i == 1 ? QStringLiteral("ru") : QStringLiteral("en");
        m_app->updateConfig(next);
        m_app->applyLanguage(true);
    });
    row(tx("Шрифт статьи"), c.fontFamily.isEmpty() ? tx("По умолчанию") : c.fontFamily, [this] {
        const QStringList fonts = Theme::articleFamilies();
        const int i = Ui::choose(this, tx("Шрифт статьи"), fonts);
        if (i < 0)
            return;
        Config next = m_app->config();
        next.fontFamily = i == 0 ? QString() : fonts[i];
        m_app->updateConfig(next);
        rebuild();
    });
    auto stepRow = [&](const QString &name, Stepper *stepper) {
        auto *w = new QWidget(m_body);
        w->setStyleSheet(QStringLiteral("QWidget#settingRow { border-bottom: 1px solid #999; }"));
        w->setObjectName(QStringLiteral("settingRow"));
        w->setAttribute(Qt::WA_StyledBackground); // a plain QWidget draws its border only with it
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(0, Theme::mm(2), 0, Theme::mm(2));
        auto *l = new QLabel(name, w);
        l->setFont(Theme::bodyFont());
        h->addWidget(l, 1);
        stepper->setParent(w);
        h->addWidget(stepper);
        v->addWidget(w);
    };
    auto *textSize = new Stepper(c.fontPt > 0 ? c.fontPt : Theme::kDefaultArticlePt, 8, 18);
    connect(textSize, &Stepper::changed, this, [this](int pt) {
        Config next = m_app->config();
        next.fontPt = pt;
        m_app->updateConfig(next);
    });
    stepRow(tx("Размер шрифта статьи"), textSize);
    auto *headSize = new Stepper(c.headlineSize, Config::kHeadlineSizeMin, Config::kHeadlineSizeMax);
    connect(headSize, &Stepper::changed, this, [this](int size) {
        Config next = m_app->config();
        next.headlineSize = size;
        m_app->updateConfig(next);
    });
    stepRow(tx("Размер текста выпуска"), headSize);

    const QString view = DesktopIcon::viewJsonPath(m_app->storageRoot());
    const bool installed = DesktopIcon::isInstalled(view);
    row(tx("Значок в «Приложениях»"), installed ? tx("Убрать") : tx("Добавить"),
        [this, view, installed] {
            const DesktopIcon::Result r = installed ? DesktopIcon::remove(view) : DesktopIcon::install(view, m_app->storageRoot());
            Ui::toast(this, DesktopIcon::message(r));
            rebuild();
        });
    row(tx("Отладка"), Log::debugEnabled() ? tx("Включена") : tx("Выключена"), [this] {
        Log::setDebug(!Log::debugEnabled());
        Log::write(QStringLiteral("debug: %1").arg(Log::debugEnabled() ? QStringLiteral("on") : QStringLiteral("off")));
        rebuild();
    });
    auto *debugHint = new QLabel(
        tx("Для разбора ошибок: каждый открытый экран сохраняется картинкой в system/inkrss/shots/, журнал system/inkrss/log.txt пишется подробнее. Обычно не нужна."),
        m_body);
    debugHint->setFont(Theme::smallFont());
    debugHint->setProperty("role", "muted");
    debugHint->setWordWrap(true);
    debugHint->setContentsMargins(0, Theme::mm(1), 0, Theme::mm(2));
    v->addWidget(debugHint);
    row(c.mode == Config::Miniflux ? tx("Выйти / сменить режим") : tx("Сменить режим"),
        tx("Сменить"), [this] {
            if (Ui::confirm(this, tx("Отключиться и вернуться к выбору режима? Скачанные статьи останутся."),
                            tx("Отключиться")))
                m_app->resetConnection();
        });
    auto *ver = new QLabel(tx("InkRSS %1 · сборка %2").arg(QStringLiteral(APP_VERSION), QStringLiteral(APP_BUILD).replace(QLatin1Char('_'), QLatin1Char(' '))), m_body);
    ver->setFont(Theme::smallFont());
    ver->setProperty("role", "muted");
    v->addSpacing(Theme::mm(3));
    v->addWidget(ver);
    v->addStretch(1);
}
