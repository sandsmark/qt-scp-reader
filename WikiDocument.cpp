#include "WikiDocument.h"

#include <QFile>
#include <QDebug>
#include <QRegularExpression>

WikiDocument::WikiDocument(QObject *parent) :
    QTextDocument(parent)
{
    QFont f = defaultFont();
    f.setPixelSize(10);
    setDefaultFont(f);
}

bool WikiDocument::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << file.errorString();
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
//    content = preprocess(std::move(content));

    content = content.remove("[[>]]");
    content = content.remove("[[/>]]");
    content = content.remove("[[=]]");

    content = content.replace(">", "&lt;");
//    content = content.replace("[[div", "<div>");

    QRegularExpression titleRegex("title:(.+)");
    QRegularExpressionMatch match = titleRegex.match(content);
    if (match.hasMatch()) {
        qDebug() << match.captured(0) << match.captured(1);
        content = content.replace(match.captured(0), "<h1>" + match.captured(1) + "</h1>");
    } else {
        qDebug() << "no match";
    }

    QRegularExpression collapsableRegex(R"(\[\[collapsible.*?\[\[\/collapsible]])", QRegularExpression::DotMatchesEverythingOption);
//    qDebug() << collapsableRegex.errorString();
    match = collapsableRegex.match(content);
    while (match.hasMatch()) {
        content.replace(match.captured(0), parseCollapsable(match.captured(0)));
        match = collapsableRegex.match(content);
    }
    content = content.replace("\n", "<br/>\n");

    QRegularExpression divRegex(R"(\[\[div([^\]]*)\]\])");
    match = divRegex.match(content);
    while (match.hasMatch()) {
        content = content.replace(match.captured(0), "<div" + match.captured(1) + ">");
        match = divRegex.match(content);
    }
    QRegularExpression boldRegex(R"(\*\*([^\*]*)\*\*)");
//    qDebug() << boldRegex.errorString();
    match = boldRegex.match(content);
    while (match.hasMatch()) {
        qDebug() << "bold" << match.captured(0);
        content = content.replace(match.captured(0), "<b>" + match.captured(1) + "</b>");
        match = boldRegex.match(content);
    }

    QRegularExpression tableRegex(R"((\|\|[^\|].*\|\|))", QRegularExpression::DotMatchesEverythingOption);
    match = tableRegex.match(content);
    while (match.hasMatch()) {
        content.replace(match.captured(0), parseTable(match.captured(1)));
        match = tableRegex.match(content);
    }

    const QSet<QString> moduleBlacklist({"Rate"});
    QRegularExpression moduleRegex(R"(\[\[module ([^\]]*)\]\])");
    match = moduleRegex.match(content);
    while (match.hasMatch()) {
        if (moduleBlacklist.contains(match.captured(1))) {
            content = content.remove(match.captured(0));
        }
//         = content.replace(match.captured(0), "<b>" + match.captured(1) + "</b>");
        match = moduleRegex.match(content);
    }

    QRegularExpression linkRegex(R"(\[([^\[]+?) \])");
    match = linkRegex.match(content);
    while (match.hasMatch()) {
        content = content.replace(match.captured(0), "<a href=" + match.captured(1) + "</b>");
        qDebug() << match.captured();
        match = linkRegex.match(content);
    }


//    QRegularExpression crapRegex(R"(\[\[[^\]]*\]\])");
//    content = content.remove(crapRegex);
    content = "<body>" + content + "</body>";

//    QRegularExpressionMatchIterator it = titleRegex.globalMatch(content);
//    content.replace(QRegularExpression("title:(.*", "</div>");
    content = content.replace("[[/div]]", "</div>");
//    content = "<html><body>" + content + "</body></html>";
//    qDebug().noquote() << content;

    setHtml(content);

    return true;
}

QString WikiDocument::preprocess(QString content)
{
    // TODO: this should only check for imports etc.
    return "<html><body>" + content + "</body></html>";
}

QString WikiDocument::parseTable(const QString &tableText)
{
    QString ret;
    ret += "<table>\n";
    QStringList lines = tableText.split("\n", QString::SkipEmptyParts);
    bool isHeader = true;
    for (const QString &line : lines) {
        ret.append("  <tr>\n");
        QStringList cells = line.split("||");
        if (cells.count() % 2 == 1 && cells.first() == "") {
            cells.takeFirst();
        }
        for (const QString &cell : cells) {
            if (isHeader) {
                ret.append("    <th>" + cell + "</th>\n");
            } else {
                ret.append("    <td>" + cell + "</td>\n");
            }
        }
        ret.append("  </tr>\n");
        isHeader = false;
    }
    ret += "</table>\n";
    return ret;

}

QString WikiDocument::parseCollapsable(QString content)
{
    QString ret;

    QRegularExpression hRegex(R"%(\[\[collapsible(.*?)\]\])%");
    QRegularExpressionMatch match = hRegex.match(content);
    QString header;
    if (match.hasMatch()) {
        header = match.captured(1);
        content = content.remove(match.captured());
    } else {
        qWarning() << "nomatch" << hRegex.errorString();
        qDebug().noquote() << hRegex.pattern();
//        qDebug() << match.capturedTexts();
    }

    QStringList lines = content.split("\n", QString::SkipEmptyParts);
    if (lines.length() < 2) {
        qWarning() << "empty content";
        return ret;
    }

//    QString header = lines.takeFirst();

    QMap<QString, QString> arguments;
    QRegularExpression argumentSpaceRegex(R"%(([a-zA-Z]+?)="([^"]*)")%");
    match = argumentSpaceRegex.match(header);
    if (match.hasMatch()) {
        arguments[match.captured(1)] = match.captured(2);

        header = header.remove(match.captured(0));
        match = argumentSpaceRegex.match(header);
    }

//    QRegularExpression argumentsNoSpaceRegex(R"%(([a-zA-Z]+?)=([^ ]*))%");
//    match = argumentsNoSpaceRegex.match(header);
//    if (match.hasMatch()) {
//        arguments[match.captured(1)] = match.captured(2);

//        header = header.remove(match.captured(0));
//        match = argumentsNoSpaceRegex.match(header);
//    }

    static int collapsables = 0;
    QString collapsableName = "collapsable" + QString::number(collapsables++);

    QString showString = arguments["show"];
    QString hideString = arguments["show"];
    if (showString.isEmpty() && !hideString.isEmpty()) {
        showString = hideString;
    } else if (!showString.isEmpty() && hideString.isEmpty()) {
        hideString = showString;
    } else if (showString.isEmpty() && hideString.isEmpty()) {
        showString = "Show";
        hideString = "Show";
    }

    m_collapsableShowNames[collapsableName] = showString;
    m_collapsableHiddenNames[collapsableName] = hideString;

    lines.last().remove("[[/collapsible]]");


    ret += "<div><a href=\"collapsable:" + collapsableName + "\">" + showString + "</a>\n";
    ret += lines.join("<br/>\n");
    ret += "</div>";

//    qDebug().noquote() << ret;



//    qDebug() << header;
//    qDebug() << lines.first();
    return ret;
}

//void WikiDocument::parseCollapsable(QString *content)
//{

//}
