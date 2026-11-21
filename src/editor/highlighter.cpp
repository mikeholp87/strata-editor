#include "highlighter.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextCharFormat>
Highlighter::Highlighter(QTextDocument *d, const QString &l) : QSyntaxHighlighter(d), language(l) {}
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
    if (language == "Markdown") {
        rule("^#{1,6} .*", QColor("#4DA8FF"), true);
        rule("`[^`]+`", QColor("#58C49C"));
        rule("\\*\\*[^*]+\\*\\*", QColor("#E6AD58"), true);
        rule("\\[[^]]+\\]\\([^)]*\\)", QColor("#B5A0E6"));
        return;
    }
    rule("\\b(class|struct|enum|namespace|public|private|protected|const|constexpr|static|void|int|auto|"
         "return|if|else|for|while|switch|case|break|continue|new|delete|try|catch|throw|import|from|def|fn|"
         "let|mut|use|pub|impl|function|var|async|await|export|interface|type|true|false|null|nullptr|None|"
         "True|False|SELECT|FROM|WHERE|INSERT|UPDATE|CREATE|TABLE)\\b",
         QColor("#B5A0E6"), true);
    rule("\\b[0-9]+(?:\\.[0-9]+)?\\b", QColor("#E6AD58"));
    rule("\"(?:[^\"\\\\]|\\\\.)*\"|\x27(?:[^\x27\\\\]|\\\\.)*\x27", QColor("#58C49C"));
    if (language == "HTML" || language == "CSS")
        rule("</?[A-Za-z][^>]*>|#[0-9a-fA-F]{3,8}\\b", QColor("#4DA8FF"));
    if (language == "Python" || language == "Bash" || language == "YAML" || language == "TOML")
        rule("#.*$", QColor("#93A3B5"));
    else if (language == "SQL")
        rule("--.*$", QColor("#93A3B5"));
    else {
        rule("//.*$", QColor("#93A3B5"));
        int start = previousBlockState() == 1 ? 0 : s.indexOf("/*");
        setCurrentBlockState(0);
        while (start >= 0) {
            int end = s.indexOf("*/", start + (previousBlockState() == 1 && start == 0 ? 0 : 2));
            if (end < 0) {
                setFormat(start, s.size() - start, QColor("#93A3B5"));
                setCurrentBlockState(1);
                break;
            }
            setFormat(start, end - start + 2, QColor("#93A3B5"));
            start = s.indexOf("/*", end + 2);
        }
    }
}
