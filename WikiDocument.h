#ifndef WIKIDOCUMENT_H
#define WIKIDOCUMENT_H

#include <QTextDocument>

/**
 * @brief A QTextDocument with support for wikisyntax
 */
class WikiDocument : public QTextDocument
{
public:
    WikiDocument(QObject *parent);

    bool load(const QString &path);

private:
    QString preprocess(QString content);
    QString parseTable(const QString &tableText);
    QString parseCollapsable(QString content);
//    void parseCollapsable(QString *content);

    QMap<QString, QString> m_collapsableHiddenNames;
    QMap<QString, QString> m_collapsableShowNames;
};

#endif // WIKIDOCUMENT_H
