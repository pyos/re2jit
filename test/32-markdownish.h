#include "00-definitions.h"

/* A parser of a small (but most commonly used) subset of Markdown.
 * It uses regexps to tokenize the input text. Obviously.
 *
 * Initially, this thing was written as a bet that I could make a parser
 * of a subset of Markdown suitable for comments, etc. in less than 200 SLOC
 * in Python. (Full-fledged parsers were considered too extreme for that,
 * plus python-markdown2 generated malformed HTML on improperly nested
 * syntactic constructs at the time -- looks like they fixed that.) I won.
 * The project it was written for is long dead, but the parser still exists,
 * rewritten in dg, although it does not see much use. Here it is, in all its
 * magnificient regexp-y glory: https://github.com/pyos/dg/blob/gh-pages/dmark.dg
 */
#include <fstream>
#include <sstream>
#include <iostream>
#include <iterator>

// Damn, that looks bad without the verbose syntax (flag x).
static const constexpr char stage1_re[] = "(?is)(?P<fenced>```)|(?P<code> {4}|\\t)|(?P<rule>\\s*(?:[*-]\\s*){3,}$)|(?P<ol>\\s*\\d+\\.\\s+)|(?P<ul>\\s*[*+-]\\s+)|(?P<h>\\s*(?P<h_size>#{1,6})\\s*)|(?P<quote>\\s*>)|(?P<break>\\s*$)|(?P<p>\\s*)";
static const constexpr int  stage1_groups = 12;

// Original regexp contains a backreference in order to properly match inline code:
//
//   (?P<code> (`+) ... \2 )
//
// re2 does not support them, so we'll change the syntax a bit to only accept single quotes.
static const constexpr char stage2_re[] = "(?is)(?P<code>`(?P<code_>.+?)`)|(?P<bold>\\*\\*(?P<bold_>(?:\\\\?.)+?)\\*\\*)|(?P<italic>\\*(?P<italic_>(?:\\\\?.)+?)\\*)|(?P<strike>~~(?P<strike_>(?:\\\\?.)+?)~~)|(?P<invert>%%(?P<invert_>(?:\\\\?.)+?)%%)|(?P<hyperlink>[a-z][a-z0-9+.-]*:(?:[,.?]?[^\\s(<>)\"',.?%]|%[0-9a-f]{2}|\\([^\\s(<>)\"']+\\))+)|(?P<text>[\\w_]*\\w)|(?P<escape>\\\\?(?P<escaped>.))";
static const constexpr int  stage2_groups = 15;

static const RE2        stage1_re2(stage1_re);
static const RE2        stage2_re2(stage2_re);
static const re2jit::it stage1_jit(stage1_re);
static const re2jit::it stage2_jit(stage2_re);


template <typename T, int i> static const T& REGEXP();
template <typename T, int i> static const T& REGEXP();
template <> auto REGEXP<RE2, 1>()        -> decltype((stage1_re2)) { return stage1_re2; }
template <> auto REGEXP<RE2, 2>()        -> decltype((stage2_re2)) { return stage2_re2; }
template <> auto REGEXP<re2jit::it, 1>() -> decltype((stage1_jit)) { return stage1_jit; }
template <> auto REGEXP<re2jit::it, 2>() -> decltype((stage2_jit)) { return stage2_jit; }


template <typename M> auto join(const std::string& s, const M& parts) -> std::string
{
    std::ostringstream u;
    auto i = std::begin(parts);
    auto e = std::end(parts);
    if (i != e) u << *i++; while (i != e) u << s << *i++;
    return u.str();
}


auto lastgroup(const RE2& r, const re2::StringPiece *match, int nmatch) -> std::string
{
    const std::map<int, std::string> &names = r.CapturingGroupNames();
    const char *end = NULL;

    int grp = 0;

    for (int i = 1; i < nmatch; i++) {  // 0 = whole match
        if (match[i].data() == NULL || match[i].data() < end)
            continue;

        end = match[i].data() + match[i].size();
        grp = i;
    }

    const auto it = names.find(grp);

    if (it == names.cend())
        return "";

    std::string n = it->second;
    return n;
}


template <typename T> auto stage2(std::string text) -> std::string
{
    size_t off = 0;
    re2::StringPiece groups[stage2_groups];

    while (match(REGEXP<T, 2>(), text.c_str() + off, RE2::UNANCHORED, groups, stage2_groups)) {
        auto lg = lastgroup(stage2_re2, groups, stage2_groups);
        auto rp =
            lg == "text"      ? groups[ 0].as_string()
          : lg == "escape"    ? groups[14].as_string()
          : lg == "hyperlink" ? "<a href=\"" + groups[0].as_string() + "\">" + groups[0].as_string() + "</a>"
          : lg == "code"      ? "<code>"                 + stage2<T>(groups[ 2].as_string()) + "</code>"
          : lg == "bold"      ? "<strong>"               + stage2<T>(groups[ 4].as_string()) + "</strong>"
          : lg == "italic"    ? "<em>"                   + stage2<T>(groups[ 6].as_string()) + "</em>"
          : lg == "strike"    ? "<del>"                  + stage2<T>(groups[ 8].as_string()) + "</em>"
          : lg == "invert"    ? "<span class='spoiler'>" + stage2<T>(groups[10].as_string()) + "</span>"
          : "";

        text.replace(off = groups[0].data() - text.data(),
                           groups[0].size(), rp);

        off += rp.size();
    }

    return text;
}


template <typename T, typename M> auto stage1(const M& lines) -> std::string
{
    std::string key, out;
    std::vector<std::string> grp;

    for (auto i = std::begin(lines), e = std::end(lines); i != e; ++i)
    {
        re2::StringPiece groups[stage1_groups];
        // always matches at least an empty substring
        match(REGEXP<T, 1>(), re2::StringPiece{ i->data(), (int) i->size() },
              RE2::ANCHOR_START, groups, stage1_groups);

        auto lg = lastgroup(stage1_re2, groups, stage1_groups);
        auto ln = i->substr(groups[0].size());

        if (lg == "h")
            lg = "h" + std::to_string(groups[7].size());

        if (grp.size() && key != lg) {
            out += key    == "break"  ? ""
                 : key    == "rule"   ? "<br />"
                 : key    == "quote"  ? "<blockquote>"  + stage1<T>(grp)             + "</blockquote>"
                 : key    == "p"      ? "<p>"           + stage2<T>(join("\n", grp)) + "</p>"
                 : key    == "code"   ? "<pre>"         + join("\n",        grp)     + "</pre>"
                 : key    == "fenced" ? "<pre>"         + join("\n",        grp)     + "</pre>"
                 : key    == "ul"     ? "<ul><li>"      + join("</li><li>", grp)     + "</li></ul>"
                 : key    == "ol"     ? "<ol><li>"      + join("</li><li>", grp)     + "</li></ol>"
                 : key[0] == 'h'      ? "<" + key + ">" + join("\n",        grp)     + "</" + key + ">"
                 : "";
            grp.clear();
        }

        key = lg;

        if (lg == "fenced")
            while (++i != e && *i != "```") grp.push_back(*i);
        else
            grp.push_back(ln);
    }

    return out;
}


template <typename T> auto markdownish(std::istream& in) -> std::string
{
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) lines.push_back(line);
    lines.push_back("");  // induce a break
    return stage1<T>(lines);
}


template <typename T> auto markdownish(const std::string& input) -> std::string
{
    std::istringstream ss(input);
    return markdownish<T>(ss);
}


auto readall(std::string name) -> std::string
{
    std::ifstream in(name, std::ios::in);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return contents;
    }
    throw errno;
}


#define MARKDOWNISH_TEST(name, kind, input, expect) \
    test_case(name) {                                      \
        auto   a = markdownish<kind>(input);               \
        return a == expect                                 \
             ? Result::Pass("ok")                          \
             : Result::Fail("wrong: %s", a.c_str());       \
    }


#define MARKDOWNISH_RE2_TEST(name, input)  \
    test_case(name) {                             \
        auto a = markdownish<RE2>        (input); \
        auto b = markdownish<re2jit::it> (input); \
        return a == b;                            \
    }


#define MARKDOWNISH_FILE_RE2_TEST(name, fn) \
        MARKDOWNISH_RE2_TEST(name, readall(fn))


#define MARKDOWNISH_PERF_TEST(n, name, input) \
    MARKDOWNISH_RE2_TEST(name, input);        \
                                              \
    GENERIC_PERF_TEST(name " [re2]", n        \
      , std::string r = input;                \
      , markdownish<RE2>(r);                  \
      , {});                                  \
                                              \
    GENERIC_PERF_TEST(name " [jit]", n        \
      , std::string r = input;                \
      , markdownish<re2jit::it>(r);           \
      , {});


#define MARKDOWNISH_FILE_PERF_TEST(n, name, fn) \
        MARKDOWNISH_PERF_TEST(n, name, readall(fn))
