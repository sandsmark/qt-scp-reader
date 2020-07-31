#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTextBrowser>

class WikiDocument;
class QListView;
class QSortFilterProxyModel;

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

protected:
//    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    WikiDocument *m_document;
    QTextBrowser *m_browser;
};

#endif // WIDGET_H
