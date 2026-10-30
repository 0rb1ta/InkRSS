#pragma once

#include "AppController.h"
#include "Widgets.h"

// Server, articles per update, article font, launcher icon, OPML import, switching the mode.
class SettingsScreen : public Screen {
    Q_OBJECT
public:
    explicit SettingsScreen(AppController *app, QWidget *parent = nullptr);
    void shown() override;

private:
    void rebuild();
    AppController *m_app;
    QWidget *m_body;
};
