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
    m_document->load("members-pages");
//    m_document->load("scp-2886");
    //m_document->load("scp-458");
    m_document->setDocumentMargin(20);
    setLayout(new QVBoxLayout);
    m_browser = new QTextBrowser(this);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_browser->setDocument(m_document);
    layout()->addWidget(m_browser);
    layout()->setMargin(0);

    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &Widget::onLinkClicked);
}

Widget::~Widget()
{

}

void Widget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    qDebug() << "click at" << event->pos();


}

void Widget::onLinkClicked(const QUrl &url)
{
    qDebug() << url.scheme() << "path:" << url.path() << "url" << url;

    if (url.scheme() == "collapsable") {
//        qDebug() << url.scheme() << url.path();
        m_document->toggleCollapsable(url.path());
        m_browser->update();
        return;
    }

    m_document->load(url.toString());
}
