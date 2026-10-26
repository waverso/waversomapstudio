// Source Repository: https://github.com/waverso/waversomapstudio

struct Writer
{
    std::vector<std::uint8_t> bytes;

    void U32(std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
        }
    }
    void Int(int value)
    {
        U32(std::bit_cast<std::uint32_t>(value));
    }
    void Float(float value)
    {
        U32(std::bit_cast<std::uint32_t>(value));
    }
    void Bool(bool value)
    {
        U32(value ? 1 : 0);
    }
    template <std::size_t N>
    void String(const char (&value)[N])
    {
        const auto end = std::find(value, value + N, '\0');
        if (end == value + N)
        {
            throw std::runtime_error("Unterminated project string.");
        }
        U32(static_cast<std::uint32_t>(end - value));
        bytes.insert(bytes.end(), value, end);
    }
    void Vector(const float (&value)[3])
    {
        for (float v : value)
        {
            Float(v);
        }
    }
};

struct Reader
{
    std::span<const std::uint8_t> bytes;
    std::size_t offset = 0;

    std::uint32_t U32()
    {
        if (bytes.size() - offset < 4)
        {
            throw std::runtime_error("Truncated project file.");
        }
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
        {
            value |= std::uint32_t(bytes[offset++]) << (i * 8);
        }
        return value;
    }
    int Int()
    {
        return std::bit_cast<int>(U32());
    }
    float Float()
    {
        const float value = std::bit_cast<float>(U32());
        if (!std::isfinite(value))
        {
            throw std::runtime_error("Project contains an invalid numeric value.");
        }
        return value;
    }
    bool Bool()
    {
        const auto value = U32();
        if (value > 1)
        {
            throw std::runtime_error("Invalid project flag.");
        }
        return value != 0;
    }
    template <std::size_t N>
    void String(char (&value)[N])
    {
        const auto count = U32();
        if (count >= N || count > bytes.size() - offset)
        {
            throw std::runtime_error("Invalid project string length.");
        }
        if (std::find(bytes.begin() + offset, bytes.begin() + offset + count, 0) != bytes.begin() + offset + count)
        {
            throw std::runtime_error("Invalid project string.");
        }
        std::memset(value, 0, N);
        std::memcpy(value, bytes.data() + offset, count);
        offset += count;
    }
    void Vector(float (&value)[3])
    {
        for (float& v : value)
        {
            v = Float();
        }
    }
};
