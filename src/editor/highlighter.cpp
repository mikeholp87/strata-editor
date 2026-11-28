#include "highlighter.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextCharFormat>
Highlighter::Highlighter(QTextDocument *d, const QString &l) : QSyntaxHighlighter(d), language(l) {}
void Highlighter::setLightTheme(bool value) {
    if (light == value)
        return;
    light = value;
    rehighlight();
}
QString Highlighter::languageForPath(const QString &p) {
    const auto e = QFileInfo(p).suffix().toLower();
    static const QHash<QString, QString> m = {{"c", "C"},           {"h", "C++"},
                                              {"cpp", "C++"},       {"hpp", "C++"},
                                              {"rs", "Rust"},       {"py", "Python"},
                                              {"js", "JavaScript"}, {"jsx", "JavaScript"},
                                              {"ts", "TypeScript"}, {"tsx", "TypeScript"},
                                              {"html", "HTML"},     {"htm", "HTML"},
                                              {"css", "CSS"},       {"json", "JSON"},
                                              {"yaml", "YAML"},     {"yml", "YAML"},
                                              {"toml", "TOML"},     {"sh", "Bash"},
                                              {"bash", "Bash"},     {"sql", "SQL"},
                                              {"md", "Markdown"},   {"markdown", "Markdown"}};
    return m.value(e, "Plain text");
}
void Highlighter::highlightBlock(const QString &s) {
    if (language == "Plain text" || s.size() > 20000)
        return;
    auto rule = [&](const QString &pattern, const QColor &color, bool bold = false) {
        QTextCharFormat f;
        f.setForeground(color);
        if (bold)
            f.setFontWeight(QFont::Bold);
        if (!patterns.contains(pattern))
            patterns.insert(pattern, QRegularExpression(pattern));
        auto matches = patterns.value(pattern).globalMatch(s);
        while (matches.hasNext()) {
            auto m = matches.next();
            setFormat(m.capturedStart(), m.capturedLength(), f);
        }
    };
    const QColor blue(light ? "#1E659E" : "#4DA8FF");
    const QColor green(light ? "#18704F" : "#58C49C");
    const QColor amber(light ? "#875510" : "#E6AD58");
    const QColor violet(light ? "#6E46A3" : "#B5A0E6");
    const QColor comment(light ? "#536477" : "#93A3B5");
    if (language == "Markdown") {
        rule("^#{1,6} .*", blue, true);
        rule("`[^`]+`", green);
        rule("\\*\\*[^*]+\\*\\*", amber, true);
        rule("\\[[^]]+\\]\\([^)]*\\)", violet);
        return;
    }
    rule("\\b(class|struct|enum|namespace|public|private|protected|const|constexpr|static|void|int|auto|"
         "return|if|else|for|while|switch|case|break|continue|new|delete|try|catch|throw|import|from|def|fn|"
         "let|mut|use|pub|impl|function|var|async|await|export|interface|type|true|false|null|nullptr|None|"
         "True|False|SELECT|FROM|WHERE|INSERT|UPDATE|CREATE|TABLE)\\b",
         violet, true);
    rule("\\b[0-9]+(?:\\.[0-9]+)?\\b", amber);
    rule("\"(?:[^\"\\\\]|\\\\.)*\"|\x27(?:[^\x27\\\\]|\\\\.)*\x27", green);
    if (language == "HTML" || language == "CSS")
        rule("</?[A-Za-z][^>]*>|#[0-9a-fA-F]{3,8}\\b", blue);
    // Comment markers inside string literals ("https://x", "#fff") are not comments.
    static const QRegularExpression stringLiteral(QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\"|'(?:[^'\\\\]|\\\\.)*'"));
    QList<QPair<int, int>> strings;
    for (auto it = stringLiteral.globalMatch(s); it.hasNext();) {
        const auto m = it.next();
        strings.append({int(m.capturedStart()), int(m.capturedEnd())});
    }
    auto outside = [&](const QString &token, int from) {
        if (token.isEmpty())
            return -1;
        for (int i = s.indexOf(token, from); i >= 0; i = s.indexOf(token, i + 1)) {
            bool inString = false;
            for (const auto &r : strings)
                inString |= i >= r.first && i < r.second;
            if (!inString)
                return i;
        }
        return -1;
    };
    const bool slashes = language == "C" || language == "C++" || language == "Rust" ||
                         language == "JavaScript" || language == "TypeScript";
    const QString marker = (language == "Python" || language == "Bash" || language == "YAML" ||
                            language == "TOML")
                               ? "#"
                           : language == "SQL" ? "--"
                           : slashes           ? "//"
                                               : QString();
    const QString open = (slashes || language == "CSS" || language == "SQL") ? "/*"
                         : language == "HTML"                              ? "<!--"
                                                                           : QString();
    const QString close = open == "/*" ? "*/" : "-->";
    setCurrentBlockState(0);
    // Walk left to right so a "//" inside a block comment (or "/*" after "//") is not misread.
    int i = 0;
    auto block = [&](int start, int searchFrom) { // returns where scanning resumes, or -1
        const int end = s.indexOf(close, searchFrom);
        if (end < 0) {
            setFormat(start, s.size() - start, comment);
            setCurrentBlockState(1);
            return -1;
        }
        setFormat(start, end + close.size() - start, comment);
        return int(end + close.size());
    };
    if (previousBlockState() == 1)
        i = block(0, 0);
    while (i >= 0 && i <= s.size()) {
        const int line = outside(marker, i), start = outside(open, i);
        if (line >= 0 && (start < 0 || line < start)) {
            setFormat(line, s.size() - line, comment);
            break;
        }
        if (start < 0)
            break;
        i = block(start, start + open.size());
    }
}
