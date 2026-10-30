#pragma once

#include <QWidget>

#include <functional>

class QLabel;
class QVBoxLayout;

// A list shown a page at a time (no scrolling on e-ink): rows are measured for the current width,
// packed into pages, and only the visible page's widgets exist. An optional last page closes the list.
class RowPager : public QWidget {
    Q_OBJECT
public:
    struct Row {
        std::function<int(int width)> height;
        std::function<QWidget *(QWidget *parent)> make;
    };

    explicit RowPager(QWidget *parent = nullptr);
    void setRows(const QList<Row> &rows, bool keepPage = false);
    void setEndPage(std::function<QWidget *(QWidget *parent)> make) { m_end = std::move(make); }
    void setEmptyText(const QString &text) { m_empty = text; }
    void flip(int direction);
    int page() const { return m_page; }
    int pageCount() const;
    int rowCount() const { return m_rows.size(); }
    // The bottom line: `left` and `right` around the centred page number.
    void setFooter(const QString &left, const QString &right);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutPages();
    void showPage();

    QList<Row> m_rows;
    QList<int> m_starts;
    std::function<QWidget *(QWidget *)> m_end;
    QString m_empty;
    int m_page = 0;
    int m_laidWidth = -1;
    int m_laidHeight = -1;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
    QLabel *m_footer = nullptr;
    QLabel *m_footLeft = nullptr;
    QLabel *m_footRight = nullptr;
};

// A tappable row: calls `onClick` on release.
class TapRow : public QWidget {
    Q_OBJECT
public:
    explicit TapRow(std::function<void()> onClick, QWidget *parent = nullptr);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    std::function<void()> m_onClick;
};
