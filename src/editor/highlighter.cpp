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
    if (language == "Python" || language == "Bash" || language == "YAML" || language == "TOML")
        rule("#.*$", comment);
    else if (language == "SQL")
        rule("--.*$", comment);
    else {
        rule("//.*$", comment);
        int start = previousBlockState() == 1 ? 0 : s.indexOf("/*");
        setCurrentBlockState(0);
        while (start >= 0) {
            int end = s.indexOf("*/", start + (previousBlockState() == 1 && start == 0 ? 0 : 2));
            if (end < 0) {
                setFormat(start, s.size() - start, comment);
                setCurrentBlockState(1);
                break;
            }
            setFormat(start, end - start + 2, comment);
            start = s.indexOf("/*", end + 2);
        }
    }
}
