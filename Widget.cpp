#include "Widget.h"

#include "WikiDocument.h"

#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QTextBrowser>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

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
    QHBoxLayout *navigationLayout = new QHBoxLayout;
    leftLayout->addLayout(navigationLayout);

    QPushButton *backButton = new QPushButton("Back");
    navigationLayout->addWidget(backButton);
    backButton->setEnabled(false);
    QPushButton *forwardButton = new QPushButton("Forward");
    forwardButton->setEnabled(false);
    navigationLayout->addWidget(forwardButton);

    QLineEdit *pagesFilterEdit = new QLineEdit;
    pagesFilterEdit->setPlaceholderText("Search...");
    leftLayout->addWidget(pagesFilterEdit);

    QListView *pageListView = new QListView;
    leftLayout->addWidget(pageListView);

    QStandardItemModel *pagesModel = new QStandardItemModel(this);
    for (QString fileName : QDir(":/pages").entryList({"*.txt"}, QDir::Files)) {
        fileName.remove(".txt");
        pagesModel->appendRow(new QStandardItem(fileName));
    }

    QSortFilterProxyModel *searchModel = new QSortFilterProxyModel(this);
    searchModel->setSourceModel(pagesModel);
    pageListView->setModel(searchModel);
    pageListView->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    pagesFilterEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

    m_document->setDocumentMargin(20);

    m_browser = new WikiBrowser(this);
    m_browser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_browser->setDocument(m_document);
    layout()->addWidget(m_browser);
    layout()->setMargin(0);

//    m_browser->setOpenLinks(false);
    connect(m_browser, &QTextBrowser::historyChanged, this, []() {
        qWarning() << "history changed";
    });
    connect(m_browser, &QTextBrowser::sourceChanged, this, [](const QUrl &src) {
        qWarning() << "source changed" << src;
    });

    connect(m_browser, &QTextBrowser::backwardAvailable, backButton, &Widget::setEnabled);
    connect(m_browser, &QTextBrowser::forwardAvailable, forwardButton, &Widget::setEnabled);
    connect(backButton, &QPushButton::clicked, m_browser, &QTextBrowser::backward);
    connect(forwardButton, &QPushButton::clicked, m_browser, &QTextBrowser::forward);
    connect(pageListView, &QListView::clicked, m_browser, [=](const QModelIndex &index) {
        m_browser->setSource("qrc:///pages/" + index.data().toString() + ".txt");
    });
    m_browser->setSource(QString("qrc:///pages/members-pages.txt"));
    connect(pagesFilterEdit, &QLineEdit::textChanged, searchModel, &QSortFilterProxyModel::setFilterFixedString);
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
