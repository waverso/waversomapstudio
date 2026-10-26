// Source Repository: https://github.com/waverso/waversomapstudio

using Numbers = std::unordered_map<std::string, double>;

class Expression
{
    public:
    Expression(Tokens tokens, const Numbers& values) : m_tokens(tokens), m_values(values)
    {
    }

    std::optional<double> Read()
    {
        const double value = Sum(0);
        if (!m_valid || m_position != m_tokens.size() || !std::isfinite(value))
        {
            return std::nullopt;
        }
        return value;
    }

    private:
    bool Take(std::string_view text)
    {
        if (m_position < m_tokens.size() && m_tokens[m_position].text == text)
        {
            ++m_position;
            return true;
        }
        return false;
    }
    double Atom(int depth)
    {
        if (depth > 32 || m_position >= m_tokens.size())
        {
            m_valid = false;
            return 0;
        }
        if (Take("+"))
        {
            return Atom(depth + 1);
        }
        if (Take("-"))
        {
            return -Atom(depth + 1);
        }
        if (Take("("))
        {
            const double value = Sum(depth + 1);
            if (!Take(")"))
            {
                m_valid = false;
            }
            return value;
        }
        const auto& token = m_tokens[m_position++];
        if (token.kind == Kind::Identifier)
        {
            if (Take(":"))
            {
                return Atom(depth + 1);
            }
            auto found = m_values.find(token.text);
            if (found != m_values.end())
            {
                return found->second;
            }
        }
        else if (token.kind == Kind::Number)
        {
            std::string value = token.text;
            value.erase(std::remove(value.begin(), value.end(), '_'), value.end());
            if (value.starts_with("0x") || value.starts_with("0X"))
            {
                std::uint32_t integer = 0;
                const auto parsed = std::from_chars(value.data() + 2, value.data() + value.size(), integer, 16);
                if (parsed.ec == std::errc {} && parsed.ptr == value.data() + value.size())
                {
                    return integer;
                }
            }
            else
            {
                if (!value.empty() && (value.back() == 'f' || value.back() == 'F'))
                {
                    value.pop_back();
                }
                double number = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                if (parsed.ec == std::errc {} && parsed.ptr == value.data() + value.size())
                {
                    return number;
                }
            }
        }
        m_valid = false;
        return 0;
    }
    double Product(int depth)
    {
        double value = Atom(depth);
        for (;;)
        {
            if (Take("*"))
            {
                value *= Atom(depth);
            }
            else if (Take("/"))
            {
                const double divisor = Atom(depth);
                if (divisor == 0)
                {
                    m_valid = false;
                }
                else
                {
                    value /= divisor;
                }
            }
            else
            {
                return value;
            }
        }
    }
    double Sum(int depth)
    {
        double value = Product(depth);
        for (;;)
        {
            if (Take("+"))
            {
                value += Product(depth);
            }
            else if (Take("-"))
            {
                value -= Product(depth);
            }
            else
            {
                return value;
            }
        }
    }

    Tokens m_tokens;
    const Numbers& m_values;
    std::size_t m_position = 0;
    bool m_valid = true;
};
