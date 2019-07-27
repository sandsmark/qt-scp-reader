#include "Widget.h"

#include "WikiDocument.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QTextBrowser>
#include <QVBoxLayout>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_document(new WikiDocument(this))
{
    m_document->load("/home/sandsmark/src/wdotcrawl/scp/scp-2886.txt");
    setLayout(new QVBoxLayout);
    m_browser = new QTextBrowser(this);
    m_browser->setDocument(m_document);
    layout()->addWidget(m_browser);
    layout()->setMargin(0);

    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &Widget::onLinkClicked);
}

Widget::~Widget()
{

}

//void Widget::paintEvent(QPaintEvent *)
//{
//    QPainter painter(this);

//    QRect visible = rect();

////    painter.scale(0.1, 0.1);
////    m_document->setPageSize(QSize(visible.size().width() * 10, visible.height() * 10));
//    m_document->drawContents(&painter, visible);

//}

void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    qDebug() << "click at" << event->pos();


}

void Widget::onLinkClicked(const QUrl &url)
{
//    qDebug() << url.scheme() << "path:" << url.path() << "url" << url;

    if (url.scheme() == "collapsable") {
//        qDebug() << url.scheme() << url.path();
        m_document->toggleBlock(url.path());
        m_browser->update();
        return;
    }


}
