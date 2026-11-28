#include "completion.h"
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace {
const QStringList &keywords(const QString &language) {
    static const QStringList cpp = {
        "alignas", "auto", "break", "case", "catch", "class", "const", "constexpr", "continue", "default",
        "delete", "double", "else", "enum", "explicit", "extern", "false", "float", "for", "friend",
        "if", "include", "inline", "namespace", "new", "noexcept", "nullptr", "operator", "override",
        "private", "protected", "public", "return", "sizeof", "static", "struct", "switch", "template",
        "this", "throw", "true", "try", "typedef", "typename", "union", "unsigned", "using", "virtual",
        "void", "volatile", "while", "std::string", "std::vector"};
    static const QStringList rust = {
        "async", "await", "break", "const", "continue", "crate", "else", "enum", "extern", "false",
        "impl", "let", "loop", "match", "mod", "move", "mut", "pub", "ref", "return", "self", "static",
        "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while", "Option",
        "Result", "String", "Vec", "println"};
    static const QStringList python = {
        "and", "assert", "async", "await", "break", "class", "continue", "def", "elif", "else",
        "except", "False", "finally", "for", "from", "global", "if", "import", "lambda", "None",
        "nonlocal", "not", "pass", "raise", "return", "self", "True", "try", "while", "with", "yield",
        "print", "range", "isinstance", "enumerate"};
    static const QStringList js = {
        "async", "await", "break", "case", "catch", "class", "const", "continue", "default", "else",
        "export", "extends", "false", "finally", "for", "function", "if", "import", "interface",
        "let", "null", "return", "static", "super", "switch", "this", "throw", "true", "try",
        "typeof", "undefined", "var", "while", "yield", "console", "document", "Promise"};
    static const QStringList shell = {
        "case", "done", "echo", "elif", "else", "esac", "export", "fi", "for", "function", "if",
        "local", "return", "then", "until", "while", "source", "exit", "readonly"};
    static const QStringList sql = {
        "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES", "UPDATE", "DELETE", "CREATE", "TABLE",
        "ALTER", "DROP", "INDEX", "JOIN", "LEFT", "RIGHT", "INNER", "OUTER", "GROUP", "ORDER", "LIMIT",
        "HAVING", "DISTINCT", "UNION", "PRIMARY", "FOREIGN", "REFERENCES", "NULL", "COUNT"};
    static const QStringList css = {
        "background", "border", "color", "display", "flex", "font-family", "font-size", "grid",
        "height", "margin", "padding", "position", "transition", "width", "@media", "!important"};
    static const QStringList html = {
        "article", "button", "canvas", "div", "footer", "form", "header", "href", "img", "input",
        "label", "main", "nav", "script", "section", "span", "style", "table", "textarea"};
    static const QStringList none;
    static const QHash<QString, const QStringList *> by = {
        {"C", &cpp},         {"C++", &cpp},   {"Rust", &rust}, {"Python", &python}, {"JavaScript", &js},
        {"TypeScript", &js}, {"Bash", &shell}, {"SQL", &sql},   {"CSS", &css},       {"HTML", &html}};
    return *by.value(language, &none);
}
} // namespace

QStringList Completion::candidates(const QString &text, int from, int to, const QString &language) {
    // ponytail: scans a +/-128 KiB window around the cursor; build an index if this ever matters
    from = qBound(0, from, text.size());
    to = qBound(from, to, text.size());
    const int begin = qMax(0, from - window), end = qMin(int(text.size()), to + window);
    static const QRegularExpression word(QStringLiteral("[\\p{L}_][\\p{L}\\p{N}_]{2,}"));
    QSet<QString> seen;
    for (const auto &range : {qMakePair(begin, from), qMakePair(to, end)}) {
        const auto part = text.mid(range.first, range.second - range.first);
        for (auto it = word.globalMatch(part); it.hasNext();)
            seen.insert(it.next().captured());
    }
    for (const auto &k : keywords(language))
        seen.insert(k);
    auto list = QStringList(seen.begin(), seen.end());
    std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) {
        const int c = a.compare(b, Qt::CaseInsensitive);
        return c ? c < 0 : a < b;
    });
    return list;
}
