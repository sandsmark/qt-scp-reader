#include "WikiDocument.h"

#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextTable>

WikiDocument::WikiDocument(QObject *parent) :
    QTextDocument(parent)
{
}

bool WikiDocument::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << file.errorString();
        return false;
    }
    const QRegularExpression nonAlphaRegex("[^a-z0-9]");

    QString content = QString::fromUtf8(file.readAll());

    // Garbage, TODO: parse properly
    content.remove("[[>]]");
    content.remove("[[/>]]");
    content.replace("[[=]]", "<code>");
    content.replace("[[/=]]", "</code>");
    content.replace("<<", "&laquo;");
    content.replace(">>", "&raquo;");

    // Quotes
    QRegularExpression quoteRegex(R"(^(>.*?)\n^$)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption);
    content.replace(quoteRegex, "<pre>\n\\1\n</pre>");
    content.replace(QRegularExpression("^>", QRegularExpression::MultilineOption), "   "); // remove them, could do it in one pass but meh

//    // Paragraphs
    QRegularExpression paragraphRegex(R"(^$\n([^\[].+)\n^$)", QRegularExpression::MultilineOption);
    content.replace(paragraphRegex, "\n<p>\\1</p>\n\n");

    QRegularExpression titleRegex("title:(.+)");
    content.replace(titleRegex, "<h1>\\1</h1>");

    QRegularExpression collapsableRegex(R"(\[\[collapsible.*?\[\[\/collapsible]])", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = collapsableRegex.match(content);
    while (match.hasMatch()) {
        content.replace(match.captured(0), parseCollapsable(match.captured(0)));
        match = collapsableRegex.match(content);
    }

    QRegularExpression boldRegex(R"(\*\*([^\*]*)\*\*)");
    content.replace(boldRegex, "<b>\\1</b>");

    QRegularExpression italicRegex(R"(//([^\/]*)//)");
    content.replace(italicRegex, "<i>\\1</i>");

    QRegularExpression tableRegex(R"((\|\|[^\|].*\|\|))", QRegularExpression::DotMatchesEverythingOption);
    match = tableRegex.match(content);
    while (match.hasMatch()) {
        content.replace(match.captured(0), parseTable(match.captured(1)));
        match = tableRegex.match(content);
    }

//    const QSet<QString> moduleBlacklist({"Rate"});
//    QRegularExpression moduleRegex(R"(\[\[module ([^\]]*)\]\])");
//    match = moduleRegex.match(content);
//    while (match.hasMatch()) {
//        if (moduleBlacklist.contains(match.captured(1))) {
//            content = content.remove(match.captured(0));
//        }
//        match = moduleRegex.match(content, match.capturedEnd());
//    }
//    QRegularExpression includeRegex(R"(\[\[include ([^\]]*)\]\])");
//    match = includeRegex.match(content);
//    while (match.hasMatch()) {
//        content = content.remove(match.captured(0));
//        match = includeRegex.match(content);
//    }

    // [[[internal links]]]
    QRegularExpression pageRegex(R"(\[\[\[([^\[]+)\]\]\])");
    match = pageRegex.match(content);
    while (match.hasMatch()) {
        QString page = match.captured(1);
        QString target = page.toLower().replace(nonAlphaRegex, "-") + ".txt";
        content = content.replace(match.captured(0), "<a href='" + target + "'>" + page + "</a>");
        match = pageRegex.match(content);
    }


    QRegularExpression elementRegex(R"(\[\[(([^\[]+?)( [^\[]+)?)\]\])");
    match = elementRegex.match(content);

    QSet<QString> standaloneTags({
        "div", "/div",
        "footnote", "/footnote",
    });

    // [[tags]]
    while (match.hasMatch()) {
        qDebug() << match.capturedTexts();
        QString firstElement = match.captured(2);
        QString text;

        QString tag = match.captured(2);
        QString tagProperties = match.captured(3);
        if (firstElement == "image") {
            QStringList parts = tagProperties.split(' ', QString::SkipEmptyParts);
            QString src = parts.takeFirst();

            if (src.startsWith('"')) {
                src.remove(0, 1);
            }
            if (src.endsWith('"')) {
                src.chop(1);
            }
            parts.prepend("src=" + src);
            tagProperties = parts.join(' ');
        } else if (firstElement == "*user") {
            QString username = match.captured(3);
            text = username;
            tag = "a";
            tagProperties = "href=user:info/" + username.toLower().replace(' ', '-');
        } else if (firstElement == "include") {
            // TODO
            content = content.replace(match.captured(0), "");
            match = elementRegex.match(content);
            continue;
        } else  if (firstElement == "module") {
            // TODO
            content = content.replace(match.captured(0), "");
            match = elementRegex.match(content);
            continue;
        }

        QString replacement = "<" + tag + ' ' + tagProperties + ">";
        qDebug().noquote() << replacement;
        if (!text.isEmpty()) {
            replacement += text + "</" + tag + ">";
        }

        content = content.replace(match.captured(0), replacement);
        match = elementRegex.match(content);
    }


    QRegularExpression linkRegex(R"(\[([^\[]+?) ([^\[]+?)\])");
    match = linkRegex.match(content);
    while (match.hasMatch()) {
        if (QUrl(match.captured(1)).scheme().isEmpty()) {
            match = linkRegex.match(content, match.capturedEnd());
            continue;
        }
        QString url = match.captured(1);
        if (url.startsWith("collapsable:")) {
            qWarning() << "Someone trying to be funny";
            url += ":";
        }
        content = content.replace(match.captured(0), "<a href=" + match.captured(1) + ">" + match.captured(2) + "</a>");
        match = linkRegex.match(content);
    }

    content.replace(QRegularExpression("^$"), "<br/>");
    qDebug().noquote() << content;

    content = "<html><body>" + content + "</body></html>";

    setHtml(content);

    for (const QString &collapsable : m_collapsableShowNames.keys()) {
        toggleCollapsable(collapsable);
    }

    return true;
}

void WikiDocument::toggleCollapsable(const QString &name)
{
    qDebug() << "toggling" << name;
    if (!m_collapsableShowNames.contains(name)) {
        qWarning() << "can't find" << name;
        return;
    }

    bool foundHref = false;
    bool foundContent = false;
    const QString matchName = "collapsable:" + name;

    for (QTextBlock block = begin(); block.isValid(); block = block.next()) {
        for (const QTextLayout::FormatRange &r : block.textFormats()) {
            if (foundHref && foundContent) {
                break;
            }
            if (r.format.anchorNames().contains(matchName)) {
                QTextCursor cursor(block);
                QTextFrame::iterator it = cursor.currentFrame()->begin();
                bool wasVisible = false;
                while(it != cursor.currentFrame()->end()) {
                    it.currentBlock().setVisible(!it.currentBlock().isVisible());
                    if (it.currentBlock().isValid()) {
                        wasVisible = true;
                    }
                    it++;
                }
                QTextFrameFormat frameFormat = cursor.currentFrame()->frameFormat();
                QTextCharFormat charFormat = cursor.charFormat();
                if (wasVisible) {
                    frameFormat.setProperty(QTextFormat::FrameHeight, 0);
                    charFormat.setProperty(QTextTableFormat::LineHeight, 0);

                } else {
                    frameFormat.clearProperty(QTextFormat::FrameHeight);
                    charFormat.clearProperty(QTextTableFormat::LineHeight);
                }
                cursor.setCharFormat(charFormat);
                cursor.currentFrame()->setFrameFormat(frameFormat);
                foundContent = true;
                continue;
            }

            if (r.format.anchorHref() == matchName) {
                QTextCursor thisCursor(block);
                thisCursor.setKeepPositionOnInsert(true);
                thisCursor.movePosition(QTextCursor::StartOfBlock);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, r.start);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, r.length);

                if (thisCursor.selectedText() == m_collapsableHiddenNames[name]) {
                    thisCursor.insertText(m_collapsableShowNames[name]);
                } else {
                    thisCursor.insertText(m_collapsableHiddenNames[name]);
                }
                foundHref = true;
            }

        }
    }
    if (!foundHref) {
        qWarning() << "Failed to find link";
    }
    if (!foundContent) {
        qWarning() << "failed to find content";
    }
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
    }

    QStringList lines = content.split("\n", QString::SkipEmptyParts);
    if (lines.length() < 2) {
        qWarning() << "empty content";
        return ret;
    }

    QMap<QString, QString> arguments;
    QRegularExpression argumentSpaceRegex(R"%(([a-zA-Z]+?)="([^"]*)")%");
    match = argumentSpaceRegex.match(header);
    while (match.hasMatch()) {
        arguments[match.captured(1)] = match.captured(2);

        header = header.remove(match.captured(0));
        match = argumentSpaceRegex.match(header);
    }

    static int collapsables = 0;
    QString collapsableName = "collapsable" + QString::number(collapsables++);

    QString showString = arguments["show"];
    QString hideString = arguments["hide"];
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

    ret += "<center><h3><a href=\"collapsable:" + collapsableName + "\">" + hideString + "</a></h3></center>\n"; // start with hidestring, we need to use toggleBlock() to hide the blocks..

    // Qt needs a div to let us set an anchor name (with 'id'), but also a table to create a frame so we can get all the content..
    ret += "<div id='collapsable:" +collapsableName + "'>";
    ret += "<table><tr><td>";
    ret += lines.join("\n");
    ret += "</td></tr></table>";
    ret += "</div>";

    return ret;
}
