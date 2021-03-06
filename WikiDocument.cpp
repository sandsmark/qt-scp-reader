#include "WikiDocument.h"

#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextTable>

//#ifdef DEBUG_CSS
#include <private/qcssparser_p.h>
//#endif

WikiDocument::WikiDocument(QObject *parent) :
    QTextDocument(parent)
{
}

QString WikiBrowser::createHtml(const QString &path)
{
    QString content = getFileContents(path);
    if (content.isEmpty()) {
        qWarning() << "Failed to load" << path;
        return QString();
    }

    if (!preprocess(&content)) {
        return QString();
    }

    // Garbage, TODO: parse properly
    content.remove("[[>]]");
    content.remove("[[/>]]");
    content.replace("[[=]]", "<center class='footer-wikiwalk-nav'>");
    content.replace("[[/=]]", "</center>");
    content.replace("<<", "&laquo;");
    content.replace(">>", "&raquo;");

    QRegularExpression titleRegex("title:(.+)");
    Q_ASSERT(titleRegex.isValid());
    content.replace(titleRegex, "<h1>\\1</h1>\n");

    QRegularExpressionMatch match;

    // Horizontal rules/lines
    QRegularExpression hruleRegex("^\\s*----+\\s*$", QRegularExpression::MultilineOption);
    content.replace(hruleRegex, "\n<hr>\n");

    // Quotes
    content.replace(QRegularExpression("^\\s*>\\s*$", QRegularExpression::MultilineOption), "    <br/>"); // empty lines with just ">" creates new lines
    QRegularExpression quoteRegex(R"(^(>.*?)\n^$)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption);
    content.replace(quoteRegex, "<blockquote>\n\\1\n</blockquote>");
    content.replace(QRegularExpression("^>", QRegularExpression::MultilineOption), "   "); // remove them, could do it in one pass but meh

    content.replace("\xc2\xa0\xc2\xa0\xc2\xa0\xc2\xa0", "<br/>");

    // Headers
    for (int i=6; i > 0; i--) {
        QRegularExpression headerRegex("^\\s*" + QStringLiteral("\\+").repeated(i) + " (.+)$", QRegularExpression::MultilineOption);
        Q_ASSERT(headerRegex.isValid());
        match = headerRegex.match(content);
        while (match.hasMatch()) {
            content.replace(match.captured(0), "<h" + QString::number(i) + ">" + match.captured(1).toHtmlEscaped() + "</h" + QString::number(i) + ">");
            match = headerRegex.match(content);
        }
    }

    // Paragraphs
    QRegularExpression paragraphRegex(R"(^$\n([^\[\|].+)\n^)", QRegularExpression::MultilineOption);
    content.replace(paragraphRegex, "\n<p>\\1</p>\n\n");

    // Links to paragraphs
    QRegularExpression paragraphLinkRegex(R"(^$\n(\[\[\[[^\[].+?)\n^)", QRegularExpression::MultilineOption);
    content.replace(paragraphLinkRegex, "\n<p>\\1</p>\n\n");

    // Collapsible sections (the most annoying parts)
    QRegularExpression collapsableRegex(R"(\[\[collapsible.*?\[\[\/collapsible]])", QRegularExpression::DotMatchesEverythingOption);
    match = collapsableRegex.match(content);
    while (match.hasMatch()) {
        content.replace(match.captured(0), parseCollapsable(match.captured(0)));
        match = collapsableRegex.match(content);
    }

    // Inline formatting (bold, italic, strikethrough, etc.)
    QRegularExpression boldRegex(R"(\*\*([^\*]*)\*\*)");
    content.replace(boldRegex, "<b>\\1</b>");

    QRegularExpression italicRegex(R"(//([^/]*)//)");
    content.replace(italicRegex, "<i>\\1</i>");

    QRegularExpression strikethroughRegex(R"(--([^-]+)--)");
    content.replace(strikethroughRegex, "<s>\\1</s>");

    QRegularExpression superscriptRegex(R"(\^\^([^\^]*)\^\^)");
    Q_ASSERT(superscriptRegex.isValid());
    content.replace(superscriptRegex, "<sup>\\1</sup>");

    QRegularExpression underlineRegex("__([^_]+)__");
    Q_ASSERT(underlineRegex.isValid());
    content.replace(underlineRegex, "<u>\\1</u>");


    // Tables
    QRegularExpression tableRegex(R"(\n\s*(\|\|+[^\|].*\|\|))", QRegularExpression::DotMatchesEverythingOption);
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
    QRegularExpression nonAlphaRegex("[^a-z0-9]");
    QRegularExpression pageRegex(R"(\[\[\[([^\[]+)\]\]\])");
    match = pageRegex.match(content);
    while (match.hasMatch()) {
        QString page = match.captured(1).trimmed();
        QString target;
        int separator = page.indexOf('|');
        if (separator == -1) {
            target = page;
        } else {
            target = page.left(separator).trimmed();
            page = page.mid(separator + 1).trimmed();
        }
        target = target.toLower().replace(nonAlphaRegex, "-") + ".txt";
        content = content.replace(match.captured(0), "<a href='" + target + "'>" + page + "</a>");
        match = pageRegex.match(content);
    }



    // [[tags]]
    QRegularExpression elementRegex(R"(\[\[(([^\[]+?)( [^\[]+)?)\]\])");
    match = elementRegex.match(content);
    while (match.hasMatch()) {
        QString firstElement = match.captured(2);
        QString text;

        QString tag = match.captured(2);
        QString tagProperties = match.captured(3);

        static const QSet<QString> validImageTags({
                "image", // plain
                "=image", // centered
                "<image", // left
                ">image", // right
                "f<image", // floating left
                "f>image", // floating right
                });
        if (validImageTags.contains(firstElement)) {
            QStringList parts = tagProperties.split(' ', Qt::SkipEmptyParts);
            QString src = parts.takeFirst();

            if (!src.startsWith('"')) {
                src.replace("\"", "\\\"");
                src.prepend("\"");
                if (src.endsWith('"')) {
                    qWarning() << "Invalid src" << tagProperties;
                }
                src.append("\"");
            }

            parts.prepend("src=" + src);

            QString elementClass;
            if (firstElement.length() == 6) { // single prefix (=, < or >)
                switch(firstElement[0].toLatin1()) {
                case '=':
                    elementClass = "image-container aligncenter";
                    break;
                case '<':
                    elementClass = "image-container alignleft";
                    break;
                case '>':
                    elementClass = "image-container alignright";
                    break;
                default:
                    qWarning() << "Unknown image alignment" << firstElement;
                    break;
                }
            } else if (firstElement.length() == 7) { // floating alignment (f< or f>)
                switch(firstElement[1].toLatin1()) {
                case '<':
                    elementClass = "image-container floatleft";
                    break;
                case '>':
                    elementClass = "image-container floatright";
                    break;
                default:
                    qWarning() << "Unknown image alignment" << firstElement;
                    break;
                }
            }
            if (!elementClass.isEmpty()) {
                parts.prepend("class=\"" + elementClass + "\"");
            }
            tagProperties = parts.join(' ');
            tag = "img";
        } else if (firstElement == "*user") {
            QString username = match.captured(3);
            text = username;
            tag = "a";
            tagProperties = "href=user:info/" + username.toLower().replace(' ', '-');
        } else if (firstElement == "include") {
            // TODO: maybe a more generic include thing?

            QStringList parts = tagProperties.split(' ', Qt::SkipEmptyParts);

            const QString toInclude = parts.takeFirst().trimmed();

            QHash<QString, QString> variables;
            for (const QString &variable : parts.join(' ').split('|')) {
                const int equalsPos = variable.indexOf('=');
                if (equalsPos == -1) {
                    qWarning() << "Invalid variable" << variable;
                    continue;;
                }
                variables[variable.mid(0, equalsPos)] = variable.mid(equalsPos + 1);
            }

            // I think this is the old way to show images
            if (toInclude == "component:image-block") {
                tag = "img";
                QString name = variables["name"];
                if (name.contains('"') && !name.contains("'")) {
                    name = "'" + name + "'";
                } else if (!name.contains('"') && name.contains("'")) {
                    name = '"' + name + '"';
                } else {
                    name.replace("'", "\\'");
                    name.replace("\"", "\\\"");
                }

                tagProperties = "src=" + name;
                const QString caption = variables["caption"];
                if (!caption.isEmpty()) {
                    tagProperties += " alt=" + caption;
                }
            } else {
                qWarning() << "Unhandled include" << toInclude;
                qDebug() << tag << tagProperties;
                content = content.replace(match.captured(0), "");
                match = elementRegex.match(content);
                continue;
            }
        } else  if (firstElement == "module") {
            QStringList arguments = tagProperties.split(' ', Qt::SkipEmptyParts);
            QString module = arguments.takeFirst();
            qDebug() << "modules not handled:" << module << arguments;


            // TODO
            content = content.replace(match.captured(0), "");
            match = elementRegex.match(content);
            continue;
        }

        QString replacement = "<" + tag + ' ' + tagProperties + ">";
//        qDebug().noquote() << replacement;
        if (!text.isEmpty()) {
            replacement += text + "</" + tag + ">";
        }

        content = content.replace(match.captured(0), replacement);
        match = elementRegex.match(content);
    }


    QRegularExpression linkRegex(R"(\[([^\[]+?) ([^\[]+?)\])");
    match = linkRegex.match(content);
    while (match.hasMatch()) {
//        if (QUrl(match.captured(1)).scheme().isEmpty()) {
//            match = linkRegex.match(content, match.capturedEnd());
//            continue;
//        }
        QString url = match.captured(1);
        if (url.startsWith("collapsable:")) {
            qWarning() << "Someone trying to be funny";
            url += ":";
        }
//        qDebug() << match.capturedTexts();
        content = content.replace(match.captured(0), "<a href=" + match.captured(1) + ">" + match.captured(2) + "</a>");
        match = linkRegex.match(content);
    }

//    qDebug() << "\n\n\n======";

    QString html = "<html><head>";
    if (1){
        QFile cssFile(":/style.css");
        if (cssFile.open(QIODevice::ReadOnly)) {
            QByteArray css = cssFile.readAll();
            html += "<style type=\"text/css\">";
            html += QString::fromUtf8(css);
            html += "</style>";

//            QCss::Parser parser(css);
//            QCss::StyleSheet stylesheet;
//            //qDebug().noquote() << css;
//            qDebug() << "=====================";
//            qDebug() << "css Parse success?" << parser.parse(&stylesheet);
//            qDebug().noquote() << css.mid(parser.errorIndex, 100);
//            qDebug().noquote() << parser.errorIndex << parser.errorSymbol().lexem();
        } else {
            qWarning() << "failed to open css file" << cssFile.errorString();
        }
    //#ifdef DEBUG_CSS
//#endif
    }
    if (1){
        QFile cssFile(":/custom.css");
        if (cssFile.open(QIODevice::ReadOnly)) {
            QByteArray css = cssFile.readAll();
            html += "<style type=\"text/css\">";
            html += QString::fromUtf8(css);
            html += "</style>";

//            QCss::Parser parser(css);
//            QCss::StyleSheet stylesheet;
            //qDebug().noquote() << css;
//            qDebug() << "=====================";
//            qDebug() << "css Parse success?" << parser.parse(&stylesheet);
//            qDebug().noquote() << css.mid(parser.errorIndex, 100);
//            qDebug().noquote() << parser.errorIndex << parser.errorSymbol().lexem();
        } else {
            qWarning() << "failed to open css file" << cssFile.errorString();
        }
    }
    html += "</head><body>" + content + "</body></html>";

    //qDebug().noquote() << html;

//    setHtml(html);


//    for (const QString &collapsable : m_collapsableShowNames.keys()) {
//        toggleCollapsable(collapsable);
//    }

    return html;
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
    const QString matchName = name;

    bool shouldHide = m_shownCollapsables[name];
    m_shownCollapsables[name] = !shouldHide;

    for (QTextBlock block = begin(); block.isValid(); block = block.next()) {
        for (const QTextLayout::FormatRange &r : block.textFormats()) {
            if (foundHref && foundContent) {
                break;
            }
            if (r.format.anchorNames().contains(matchName)) {
                QTextCursor cursor(block);
                QTextFrame  *frame = cursor.currentFrame();
                QTextFrame::iterator it = frame->begin();

                cursor.movePosition(QTextCursor::PreviousBlock);

                while(it != frame->end()) {
                    it.currentBlock().setVisible(!shouldHide);
                    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
                    it++;
                }

                foundContent = true;
                continue;
            }

            if (r.format.anchorHref() == "#" + matchName) {
                QTextCursor thisCursor(block);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, r.start);
                thisCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, r.length);

                if (shouldHide) {
                    thisCursor.insertText(m_collapsableShowNames[name]);
                } else {
                    thisCursor.insertText(m_collapsableHiddenNames[name]);
                }
                foundHref = true;
            }

        }
    }
    markContentsDirty(0, characterCount());

    if (!foundHref) {
        qWarning() << "Failed to find link";
    }
    if (!foundContent) {
        qWarning() << "failed to find content";
    }
}

bool WikiBrowser::preprocess(QString *content, int depth)
{
    if (depth > 2) {
        qDebug() << "Too many levels of preprocessing";
        return false;
    }

    QRegularExpression moduleRegex(R"(\[\[module ([^\] ]+) ([^\[]+)\]\])");
    QRegularExpressionMatch match;
    do {
        match = moduleRegex.match(*content);
        if (!match.hasMatch()) {
            break;
        }
        const QString module = match.captured(1);

        QMap<QString, QString> arguments;

        QRegularExpression argumentRegex(R"%(([a-zA-Z]+?)="([^"]*)")%");
        QString argumentsString = match.captured(2);
        do {
            match = argumentRegex.match(argumentsString);
            if (!match.hasMatch()) {
                break;
            }
            arguments[match.captured(1)] = match.captured(2);

            argumentsString = argumentsString.remove(match.captured(0));
            match = argumentRegex.match(argumentsString);
        } while (match.hasMatch());

        const QStringList argumentsList = match.captured(2).split(Qt::SkipEmptyParts);

        if (module == "Redirect") {
            const QString redirectTarget = arguments["destination"];
            qDebug() << "Redirect to" << redirectTarget;
            QString targetContents = getFileContents(redirectTarget);
            if (targetContents.isEmpty()) {
                qWarning() << "Failed to load redirect" << redirectTarget;
                return false;
            }

            if (!preprocess(&targetContents, depth + 1)) {
                qWarning() << "Failed to preprocess" << redirectTarget;
                return false;
            }

            *content = targetContents;
            return true;
        }
        qWarning() << "Unhandled module" << module;
        qDebug() << arguments;


        content->remove(match.captured(0));
    } while(match.hasMatch());

    // Make sure we always end with a newline
    content->append("\n");
    // TODO: this should only check for imports etc.

    return true;
}

QString WikiBrowser::parseTable(const QString &tableText)
{
    QString ret;
    ret += "<table class='wiki-content-table'>\n";
    QStringList lines = tableText.split("\n", Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        ret.append("  <tr>\n");
        const QStringList cells = line.split("||");
        for (const QString &cell : cells.mid(1, cells.count() - 2)) {
            if (cell.startsWith("~ ")) { // header
                ret.append("    <th>" + cell.mid(2) + "</th>\n");
            } else {
                ret.append("    <td>" + cell + "</td>\n");
            }
        }
        ret.append("  </tr>\n");
    }
    ret += "</table>\n";

    return ret;

}

QString WikiBrowser::parseCollapsable(QString content)
{
    QString ret;

    QRegularExpression hRegex(R"%(\[\[collapsible(.*?)\]\])%");
    Q_ASSERT(hRegex.isValid());
    QRegularExpressionMatch match = hRegex.match(content);
    QString header;
    if (match.hasMatch()) {
        header = match.captured(1);
        content = content.remove(match.captured());
    }

    QStringList lines = content.split("\n", Qt::SkipEmptyParts);
    if (lines.length() < 2) {
        qWarning() << "empty content";
        return ret;
    }

    QMap<QString, QString> arguments;
    QRegularExpression argumentSpaceRegex(R"%(([a-zA-Z]+?)="([^"]*)")%");
    Q_ASSERT(argumentSpaceRegex.isValid());
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
    m_shownCollapsables.insert(collapsableName, true);

    lines.last().remove("[[/collapsible]]");

    // better safe than sorry
    showString = showString.toHtmlEscaped();
    hideString = hideString.toHtmlEscaped();
    hideString.replace("[", "&#x5B;");
    hideString.replace("]", "&#x5D;");
    hideString.replace("|", "&#x7C;");
    hideString.replace("=", "&#x3D;");

    ret += "<div class='collapsible-block-unfolded-link'><a class='collapsible-block-link' href=\"#" + collapsableName + "\">" + hideString + "</a></div>\n"; // start with hidestring, we need to use toggleBlock() to hide the blocks..

    // Qt needs a div to let us set an anchor name (with 'id'), but also a table to create a frame so we can get all the content..
    ret += "<div id='" +collapsableName + "'>";
    ret += "<table><tr><td>\n";
    ret += lines.join("\n");
    ret += "\n</td></tr></table>";
    ret += "</div>\n";

    return ret;
}

QString WikiBrowser::getFileContents(const QString &filePath)
{
    QString filename = QUrl(filePath).path();
    if (filename.isEmpty()) {
        filename = filePath;
    }

    QFile file(":" + filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "Failed to open" << file.fileName() << file.errorString();
        return QString();
    }


    return QString::fromUtf8(file.readAll());
}


void WikiBrowser::setSource(const QUrl &url)
{
    QString fileAfter = url.toString(QUrl::RemoveFragment);
    QString fileBefore = source().toString(QUrl::RemoveFragment);

    QTextBrowser::setSource(url);

    WikiDocument *wikiDocument = qobject_cast<WikiDocument*>(document());
    if (!wikiDocument) {
        qWarning() << "Invalid document";
        return;
    }
    if (fileAfter != fileBefore) {
        for (const QString &collapsable : m_collapsableShowNames.keys()) {
            wikiDocument->toggleCollapsable(collapsable);
        }

        return;
    }

    if (!url.fragment().isEmpty()) {
        wikiDocument->toggleCollapsable(url.fragment());
    }
}

QVariant WikiBrowser::loadResource(int type, const QUrl &name)
{
    if (type != QTextDocument::HtmlResource) {
        return QTextBrowser::loadResource(type, name);
    }

    qDebug() << "Browser" << type << name;
    QString html = createHtml(name.toString());
    WikiDocument *wikiDocument = qobject_cast<WikiDocument*>(document());

    if (!wikiDocument) {
        qWarning() << "Invalid document";
        return html;
    }

    wikiDocument->m_collapsableShowNames = m_collapsableShowNames;
    wikiDocument->m_collapsableHiddenNames = m_collapsableHiddenNames;
    wikiDocument->m_shownCollapsables = m_shownCollapsables;

    return html;
}
