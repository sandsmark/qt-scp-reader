#include "WikiDocument.h"

#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextTable>

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


    QRegularExpression quoteRegex(R"(^>(.*?)^[^>])", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption);
    qDebug().noquote() << quoteRegex.pattern();
//    qDebug().noquote() << content;
//    QRegularExpression quoteRegex(R"(^>(.*)\n)", QRegularExpression::DotMatchesEverythingOption);
//    qDebug() << collapsableRegex.errorString();
    QRegularExpressionMatch match = quoteRegex.match(content);
//    while (match.hasMatch()) {
//        QString preContent = match.captured(1);
//        preContent.replace(">", "   ");
//        preContent = "<pre>" + preContent + "</pre>";
//        content = content.replace(match.captured(0), preContent);

//        match = quoteRegex.match(content);
//    }

    content = content.remove("[[>]]");
    content = content.remove("[[/>]]");
    content = content.remove("[[=]]");

    content = content.replace(">", "&lt;");
//    content = content.replace("[[div", "<div>");

    QRegularExpression titleRegex("title:(.+)");
    match = titleRegex.match(content);
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
        match = moduleRegex.match(content);
    }
    QRegularExpression includeRegex(R"(\[\[include ([^\]]*)\]\])");
    match = includeRegex.match(content);
    while (match.hasMatch()) {
        content = content.remove(match.captured(0));
        match = includeRegex.match(content);
    }

    QRegularExpression linkRegex(R"(\[([^\[]+?) ([^\[]+?)\])");
    match = linkRegex.match(content);
    while (match.hasMatch()) {
        if (QUrl(match.captured(1)).scheme().isEmpty()) {
            match = linkRegex.match(content, match.capturedEnd());
            continue;
        }
        content = content.replace(match.captured(0), "<a href=" + match.captured(1) + ">" + match.captured(2) + "</a>");
        qDebug() << "captured" << match.captured() << match.captured(1) << match.captured(2);
        match = linkRegex.match(content);
    }

    content.replace("\n\n", "<br/><br/>");

//    QRegularExpression paragraphRegex(R"(^([^<\[].+)\n\n)", QRegularExpression::MultilineOption);
//    QRegularExpression paragraphRegex(R"(^[^<](.+)\n\n)", QRegularExpression::MultilineOption);
//    match = paragraphRegex.match(content);
//    while (match.hasMatch()) {
////        content = "<p>" + match.captured() + "</p>";
//        qDebug() << "paragraph" << match.captured() << match.captured(1);
//        match = paragraphRegex.match(content, match.capturedEnd());
//    }


//    QRegularExpression crapRegex(R"(\[\[[^\]]*\]\])");
//    content = content.remove(crapRegex);
//    content = "<body>" + content + "</body>";

//    QRegularExpressionMatchIterator it = titleRegex.globalMatch(content);
//    content.replace(QRegularExpression("title:(.*", "</div>");
    content = content.replace("[[/div]]", "</div>");
    content = "<html><body>" + content + "</body></html>";
//    qDebug().noquote() << content;

    setHtml(content);

//    for (QTextBlock block = begin(); block.isValid(); block = block.next()) {
//        for (const QTextLayout::FormatRange &r : block.textFormats()) {
//            if (r.format.anchorHref().isEmpty()) {
//                continue;
//            }

//            if (!block.next().isValid()) {
//                break;
//            }
//            qDebug() << r.format.anchorHref();
//            QString name = r.format.anchorHref();
//            if (!name.startsWith("collapsable:")) {
//                continue;
//            }
//            name.remove("collapsable:");
//            block.next().setVisible(false);
//            m_collapsableBlocks[name] = block.next();
//        }
//    }
    for (const QString &coll : m_collapsableBlocks.keys()) {
        toggleBlock(coll);
    }

    return true;
}

void WikiDocument::toggleBlock(const QString &name)
{
    qDebug() << "toggling" << name;
    if (!m_collapsableShowNames.contains(name)) {
        qWarning() << "can't find" << name;
        return;
    }

//    for (const QTextFormat &f : allFormats()) {
//        qDebug() << f.type();
//        if (objectForFormat(f)) {
//            qDebug() << "object" << objectForFormat(f);
//        }
//        if (!f.hasProperty(QTextFormat::AnchorName)) {
//            continue;
//        }
//        qDebug() << f.property(QTextFormat::AnchorName) << objectForFormat(f);
//    }

    bool foundHref = false;
    bool foundContent = false;
    const QString matchName = "collapsable:" + name;

    for (QTextBlock block = begin(); block.isValid(); block = block.next()) {
        qDebug() << block.blockNumber();
        for (const QTextLayout::FormatRange &r : block.textFormats()) {
            if (foundHref && foundContent) {
                break;
            }
//            if (!r.format.anchorHref().isEmpty()) {
//                qDebug() << "anchor href" << r.format.anchorHref();
//                qDebug().noquote() << block.text().simplified().left(10) + "...";
//            }
            if (r.format.anchorNames().contains(matchName)) {
                QTextCursor cursor(block);
                qDebug() << cursor.currentFrame();
                qDebug() << "anchor names" << r.format.anchorNames();
                qDebug().noquote() << block.text().simplified().left(10) + "...";
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
            }
//            if(r.format.isAnchor()) {
//                qDebug() << block.text();
//            }
//            continue;
            if (r.format.isTableFormat()) {
                qDebug() << "==============================";
                qDebug() << block.text().left(10) + "...";
            }
            if (r.format.isFrameFormat()) {
                qDebug() << "==============================";
                qDebug() << block.text().left(10) + "...";
            }
            if (r.format.isTableCellFormat()) {
                qDebug() << "==============================";
                qDebug() << block.text().left(10) + "...";
            }
//            qDebug() << r.format.type();

//            if (foundHref) {
//                if (r.format.isTableFormat()) {
//                    block.setVisible(!block.isVisible());
//                    qDebug() << "==============================";
//                    qDebug() << "visible after" << block.isVisible();
//                    qDebug() << block.text().left(10) + "...";
////                    return;
//                }
//            }

//            if (r.format.isTableFormat()) {
//                qDebug() << block.text();
//            }

            if (r.format.anchorHref() == matchName) {
                qDebug() << "href" << r.format.anchorHref();

                QTextCursor thisCursor(block);
                qDebug() << "frame" << thisCursor.currentFrame();
                thisCursor.setKeepPositionOnInsert(true);
                thisCursor.movePosition(QTextCursor::StartOfBlock);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, r.start);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, r.length);
                qDebug() << "textbefore" << thisCursor.selectedText();

                //            thisCursor.select(QTextCursor::BlockUnderCursor);
                if (thisCursor.selectedText() == m_collapsableHiddenNames[name]) {
                    thisCursor.insertText(m_collapsableShowNames[name]);
                } else {
                    thisCursor.insertText(m_collapsableHiddenNames[name]);
                }
                foundHref = true;
                continue;

                thisCursor = QTextCursor(block);
                while (thisCursor.movePosition(QTextCursor::NextBlock)) {
                    if (!thisCursor.currentTable()) {
                        qDebug() << "does not have a current table" << thisCursor.block().text();
                        continue;
                    }
//                    qDebug() << "this block text" << thisCursor.block().text();
                    if (!thisCursor.currentTable()->parentFrame()) {
                        continue;
                    }
//                    thisCursor = QTextCursor(thisCursor.currentTable()->parentFrame());
                    thisCursor = QTextCursor(thisCursor.currentTable());
                    qDebug() << "anchor names" << thisCursor.charFormat().anchorNames();
                    QTextFrameFormat frm = thisCursor.currentFrame()->frameFormat();
                    frm.setHeight(0);
//                    thisCursor.currentFrame()->setFrameFormat(frm);
//                    qDebug() << "this block text" << thisCursor.block().text();
//                    qDebug() << thisCursor.currentTable();
//                    qDebug() << thisCursor.currentTable()->parentFrame();
//                    thisCursor.movePosition(QTextCursor::NextBlock);
//                    qDebug() << thisCursor.block().text();
                    thisCursor.block().setVisible(!thisCursor.block().isVisible());
//                    return;
                    break;

                }
                qDebug() << "textafter" << thisCursor.selectedText();
                //            QTextCursor nextCursor(block.next());

                //            if (block.next().isVisible()) {
                ////                block.set
                //            }
                foundHref = true;
//                return;
//                break;
            }

//            if (foundContent) {
//                QVector<QTextLayout::FormatRange> formats = block.textFormats();

//                continue;
//            }
        }
    }
    return;

    for (int i=0; object(i); i++) {
        QTextTable *f = qobject_cast<QTextTable*>(object(i));
        if (f) {
            QTextFrame::iterator it = f->begin();
            while(it != f->end()) {
                it.currentBlock().setVisible(!it.currentBlock().isVisible());
                it++;

            }
        }
    }
    qWarning() << "--------------------------" << name;
    qWarning() << "failed to find" << name;
//    qDebug() << m_collapsableBlocks[name].isVisible();
//    m_collapsableBlocks[name].setVisible(!m_collapsableBlocks[name].isVisible());
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
    while (match.hasMatch()) {
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

    qDebug() << arguments;
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
//    qDebug() << collapsableName << showString << hideString;

    m_collapsableShowNames[collapsableName] = showString;
    m_collapsableHiddenNames[collapsableName] = hideString;

    lines.last().remove("[[/collapsible]]");

    qDebug().noquote() << "==============\n\n";

//    ret += "<div><a href=\"collapsable:" + collapsableName + "\">" + showString + "</a>\n<table id=" +collapsableName + "table><tr><td>";
    ret += "<a href=\"collapsable:" + collapsableName + "\">" + showString + "</a>\n";
    // Qt needs a div to let us set an anchor name (with 'id'), but also a table to create a frame so we can get all the content..
    ret += "<div id='collapsable:" +collapsableName + "'>";
    ret += "<table><tr><td>";
    ret += lines.join("\n");
    ret += "</td></tr></table>";
    ret += "</div>";

    qDebug().noquote() << ret;

    qDebug().noquote() << "==============\n\n";


//    qDebug() << header;
//    qDebug() << lines.first();
    return ret;
}

//void WikiDocument::parseCollapsable(QString *content)
//{

//}
