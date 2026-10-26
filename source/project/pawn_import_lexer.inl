// Source Repository: https://github.com/waverso/waversomapstudio

enum class Kind
{
    Identifier,
    Number,
    String,
    Symbol
};

struct Token
{
    Kind kind = Kind::Symbol;
    std::string text;
    std::size_t line = 1;
    bool valid = true;
};

bool IdentifierStart(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@';
}

bool Digit(char c)
{
    return c >= '0' && c <= '9';
}

std::vector<Token> Tokenize(std::string_view source, Result& result)
{
    std::vector<Token> tokens;
    std::size_t line = 1;
    for (std::size_t i = 0; i < source.size();)
    {
        const char c = source[i];
        if (c == '\n')
        {
            ++line;
            ++i;
            continue;
        }
        if (c == ' ' || c == '\r' || c == '\t')
        {
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/')
        {
            while (i < source.size() && source[i] != '\n')
            {
                ++i;
            }
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < source.size() && source.substr(i, 2) != "*/")
            {
                if (source[i++] == '\n')
                {
                    ++line;
                }
            }
            i = std::min(i + 2, source.size());
            continue;
        }
        Token token;
        token.line = line;
        if (c == '"')
        {
            token.kind = Kind::String;
            ++i;
            bool closed = false;
            while (i < source.size())
            {
                char value = source[i++];
                if (value == '"')
                {
                    closed = true;
                    break;
                }
                if (value == '\n' || value == '\r')
                {
                    --i;
                    break;
                }
                if (value == '\\' && i < source.size())
                {
                    value = source[i++];
                    switch (value)
                    {
                        case 'n':
                            value = '\n';
                            break;
                        case 'r':
                            value = '\r';
                            break;
                        case 't':
                            value = '\t';
                            break;
                        case '\\':
                        case '"':
                        case '\'':
                            break;
                        default:
                            token.valid = false;
                            break;
                    }
                }
                if (value == 0)
                {
                    token.valid = false;
                }
                token.text += value;
            }
            token.valid = token.valid && closed;
        }
        else if (IdentifierStart(c))
        {
            token.kind = Kind::Identifier;
            const auto start = i++;
            while (i < source.size() && (IdentifierStart(source[i]) || Digit(source[i])))
            {
                ++i;
            }
            token.text = source.substr(start, i - start);
        }
        else if (Digit(c) || (c == '.' && i + 1 < source.size() && Digit(source[i + 1])))
        {
            token.kind = Kind::Number;
            const auto start = i++;
            while (i < source.size())
            {
                const char n = source[i];
                if (IdentifierStart(n) || Digit(n) || n == '.')
                {
                    ++i;
                }
                else if ((n == '+' || n == '-') && i > start && (source[i - 1] == 'e' || source[i - 1] == 'E')
                    && source.substr(start, 2) != "0x" && source.substr(start, 2) != "0X")
                {
                    ++i;
                }
                else
                {
                    break;
                }
            }
            token.text = source.substr(start, i - start);
        }
        else
        {
            token.text.assign(1, c);
            ++i;
        }
        tokens.push_back(std::move(token));
        if (tokens.size() > MaxTokens)
        {
            result.failed = true;
            result.error = "Clipboard code contains too many tokens.";
            return {};
        }
    }
    return tokens;
}

using Tokens = std::span<const Token>;
