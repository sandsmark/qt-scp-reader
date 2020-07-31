#ifndef WIKIDOCUMENT_H
#define WIKIDOCUMENT_H

#include <QTextDocument>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocumentFragment>

/**
 * @brief A QTextDocument with support for wikisyntax
 */
class WikiDocument : public QTextDocument
{
    Q_OBJECT

public:
    WikiDocument(QObject *parent);

    void toggleCollapsable(const QString &name);


    QMap<QString, QString> m_collapsableHiddenNames;
    QMap<QString, QString> m_collapsableShowNames;
    QMap<QString, QTextDocumentFragment> m_hiddenFragments;

    QHash<QString, bool> m_shownCollapsables;
};

class WikiBrowser  : public QTextBrowser
{
    Q_OBJECT

public:
    WikiBrowser(QWidget *parent) : QTextBrowser(parent) {  }

public slots:
    void setSource(const QUrl &url) override;

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    bool preprocess(QString *content, int depth=0);
    QString parseTable(const QString &tableText);
    QString parseCollapsable(QString content);
    QString getFileContents(const QString &filePath);
    QString createHtml(const QString &path);

    QMap<QString, QString> m_collapsableHiddenNames;
    QMap<QString, QString> m_collapsableShowNames;
    QHash<QString, bool> m_shownCollapsables;
};

#endif // WIKIDOCUMENT_H
