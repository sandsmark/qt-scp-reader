#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class WikiDocument;
class QTextBrowser;
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

private slots:
    void onLinkClicked(const QUrl &url);

private:
    WikiDocument *m_document;
    QTextBrowser *m_browser;
    QListView *m_pagesList;
    QSortFilterProxyModel *m_pagesModel;
};

#endif // WIDGET_H
