#include "Widget.h"

#include "WikiDocument.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QTextBrowser>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QListView>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QDir>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      m_document(new WikiDocument(this))
{
    QHBoxLayout *mainLayout = new QHBoxLayout;
    setLayout(mainLayout);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    mainLayout->addLayout(leftLayout);

    QLineEdit *pagesFilterEdit = new QLineEdit;
    pagesFilterEdit->setPlaceholderText("Search...");
    leftLayout->addWidget(pagesFilterEdit);

    m_pagesList = new QListView;
    leftLayout->addWidget(m_pagesList);

    QStandardItemModel *pagesModel = new QStandardItemModel(this);
    for (QString fileName : QDir(":/pages").entryList({"*.txt"}, QDir::Files)) {
        fileName.remove(".txt");
        pagesModel->appendRow(new QStandardItem(fileName));
    }
    m_pagesModel = new QSortFilterProxyModel(this);
    m_pagesModel->setSourceModel(pagesModel);
    m_pagesList->setModel(m_pagesModel);
    m_pagesList->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    pagesFilterEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    m_document->load("members-pages");

//    m_document->load("scp-2886");
    //m_document->load("scp-458");
    m_document->setDocumentMargin(20);

    m_browser = new QTextBrowser(this);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_browser->setDocument(m_document);
    layout()->addWidget(m_browser);
    layout()->setMargin(0);

    m_browser->setOpenLinks(false);

    connect(m_browser, &QTextBrowser::anchorClicked, this, &Widget::onLinkClicked);
    connect(m_pagesList, &QListView::clicked, m_browser, [=](const QModelIndex &index) {
        m_browser->anchorClicked(QUrl(index.data().toString()));
    });
    connect(pagesFilterEdit, &QLineEdit::textChanged, m_pagesModel, &QSortFilterProxyModel::setFilterFixedString);
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
