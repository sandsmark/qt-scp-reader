#ifndef WIKIDOCUMENT_H
#define WIKIDOCUMENT_H

#include <QTextDocument>
#include <QTextBlock>

/**
 * @brief A QTextDocument with support for wikisyntax
 */
class WikiDocument : public QTextDocument
{
public:
    WikiDocument(QObject *parent);

    bool load(const QString &path);
    void toggleCollapsable(const QString &name);

private:
    QString preprocess(QString content);
    QString parseTable(const QString &tableText);
    QString parseCollapsable(QString content);

    QMap<QString, QString> m_collapsableHiddenNames;
    QMap<QString, QString> m_collapsableShowNames;
    QMap<QString, QTextBlock> m_collapsableBlocks;
};

#endif // WIKIDOCUMENT_H
