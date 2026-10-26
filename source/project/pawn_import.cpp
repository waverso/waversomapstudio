// Source Repository: https://github.com/waverso/waversomapstudio

#include "pawn_import.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>

#include "../shared/animation_catalog.h"
#include "../shared/skin_catalog.h"
#include "../pickups/pickup_position.h"

namespace PawnImport
{
    namespace
    {
        constexpr std::size_t MaxSourceBytes = 4 * 1024 * 1024;
        constexpr std::size_t MaxTokens = 1000000;
        constexpr std::size_t MaxCreatedItems = 8192;

#include "pawn_import_lexer.inl"
#include "pawn_import_expressions.inl"
#include "pawn_import_syntax.inl"

        struct ImportedItem
        {
            ItemSnapshot snapshot;
            bool dynamic = false;
            bool destroyed = false;
        };

        class Parser
        {
            public:
            Parser(const Environment& environment, Result& result) : env(environment), out(result)
            {
                values = { { "true", 1 }, { "false", 0 }, { "STREAMER_OBJECT_SD", 300 }, { "STREAMER_OBJECT_DD", 0 },
                    { "STREAMER_PICKUP_SD", PickupData().streamDistance }, { "STREAMER_ACTOR_SD", ActorData().streamDistance },
                    { "OBJECT_MATERIAL_TEXT_ALIGN_LEFT", 0 }, { "OBJECT_MATERIAL_TEXT_ALIGN_CENTER", 1 },
                    { "OBJECT_MATERIAL_TEXT_ALIGN_CENT", 1 }, { "OBJECT_MATERIAL_TEXT_ALIGN_CENTRE", 1 },
                    { "OBJECT_MATERIAL_TEXT_ALIGN_RIGHT", 2 }, { "OBJECT_MATERIAL_TEXT_ALIGN_RIGT", 2 } };
                const char* sizes[] = { "32x32", "64x32", "64x64", "128x32", "128x64", "128x128", "256x32", "256x64", "256x128",
                    "256x256", "512x64", "512x128", "512x256", "512x512" };
                for (int i = 0; i < std::size(sizes); ++i)
                {
                    values[std::string("OBJECT_MATERIAL_SIZE_") + sizes[i]] = (i + 1) * 10;
                }
                constants = values;
            }

            void Run(Tokens tokens)
            {
                for (std::size_t i = 0; i < tokens.size() && !out.failed;)
                {
                    const auto& token = tokens[i];
                    if (token.text == "#")
                    {
                        std::size_t end = i + 1;
                        while (end < tokens.size() && tokens[end].line == token.line)
                        {
                            ++end;
                        }
                        if (end > i + 1
                            && (tokens[i + 1].text == "if" || tokens[i + 1].text == "ifdef" || tokens[i + 1].text == "ifndef"))
                        {
                            int depth = 1;
                            std::size_t next = end;
                            for (; next + 1 < tokens.size() && depth; ++next)
                            {
                                if (tokens[next].text != "#")
                                {
                                    continue;
                                }
                                const auto& directive = tokens[next + 1].text;
                                if (directive == "if" || directive == "ifdef" || directive == "ifndef")
                                {
                                    ++depth;
                                }
                                else if (directive == "endif")
                                {
                                    --depth;
                                }
                            }
                            references.clear();
                            values = constants;
                            i = depth ? tokens.size() : next + 1;
                            continue;
                        }
                        else if (end > i + 3 && tokens[i + 1].text == "define")
                        {
                            const auto number = Expression(tokens.subspan(i + 3, end - i - 3), values).Read();
                            if (number)
                            {
                                values[tokens[i + 2].text] = *number;
                                constants[tokens[i + 2].text] = *number;
                            }
                            else
                            {
                                values.erase(tokens[i + 2].text);
                                constants.erase(tokens[i + 2].text);
                            }
                        }
                        i = end;
                        continue;
                    }
                    if (token.kind != Kind::Identifier)
                    {
                        ++i;
                        continue;
                    }
                    if (ControlFlow(token.text))
                    {
                        const auto end = SkipStatement(tokens, i);
                        references.clear();
                        values = constants;
                        i = std::max(i + 1, end);
                        continue;
                    }
                    if (token.text == "new" || token.text == "const" || token.text == "static")
                    {
                        std::size_t name = i + 1;
                        if (name + 1 < tokens.size() && tokens[name + 1].text == ":")
                        {
                            name += 2;
                        }
                        if (name < tokens.size() && tokens[name].kind == Kind::Identifier)
                        {
                            const auto prefix = tokens[name].text + "[";
                            std::erase_if(references,
                                [&](const auto& entry)
                                {
                                    return entry.first == tokens[name].text || entry.first.starts_with(prefix);
                                });
                            values.erase(tokens[name].text);
                        }
                        ++i;
                        continue;
                    }
                    if (IsFunction(token.text) && i + 1 < tokens.size() && tokens[i + 1].text == "(")
                    {
                        auto call = ReadCall(tokens, i);
                        Process(call, "");
                        i = std::max(i + 1, call.end);
                        continue;
                    }
                    std::size_t end = i;
                    const std::string key = Reference(tokens, end);
                    if (!key.empty() && end < tokens.size() && tokens[end].text == "=" && end + 1 < tokens.size()
                        && tokens[end + 1].text != "=")
                    {
                        const auto rhs = end + 1;
                        if (rhs + 1 < tokens.size() && IsFunction(tokens[rhs].text) && tokens[rhs + 1].text == "(")
                        {
                            references.erase(key);
                            values.erase(key);
                            auto call = ReadCall(tokens, rhs);
                            Process(call, key);
                            i = std::max(i + 1, call.end);
                            continue;
                        }
                        end = rhs;
                        while (end < tokens.size() && tokens[end].text != ";" && tokens[end].text != ","
                            && tokens[end].text != "}" && tokens[end].line == tokens[rhs].line)
                        {
                            ++end;
                        }
                        const auto number = Expression(tokens.subspan(rhs, end - rhs), values).Read();
                        std::size_t referenceEnd = rhs;
                        const auto alias = Reference(tokens, referenceEnd);
                        const auto item = references.find(alias);
                        std::optional<std::size_t> aliasItem;
                        if (referenceEnd == end && item != references.end())
                        {
                            aliasItem = item->second;
                        }
                        references.erase(key);
                        values.erase(key);
                        if (number)
                        {
                            values[key] = *number;
                        }
                        else if (aliasItem)
                        {
                            references[key] = *aliasItem;
                        }
                        i = std::max(i + 1, end);
                        continue;
                    }
                    if (i + 1 < tokens.size() && tokens[i + 1].text == "("
                        && (token.text.starts_with("SetObject") || token.text.starts_with("SetDynamicObject")
                            || token.text.starts_with("SetActor") || token.text.starts_with("SetDynamicActor")
                            || token.text.starts_with("MoveObject") || token.text.starts_with("MoveDynamicObject")
                            || token.text.starts_with("AttachObject") || token.text.starts_with("AttachDynamicObject")
                            || token.text.starts_with("Create")))
                    {
                        const auto call = ReadCall(tokens, i);
                        i = std::max(i + 1, call.end);
                        continue;
                    }
                    ++i;
                }
                int order = 0;
                for (auto& item : items)
                {
                    if (item.destroyed)
                    {
                        continue;
                    }
                    item.snapshot.id = order + 1;
                    item.snapshot.order = order++;
                    out.items.push_back(std::move(item.snapshot));
                }
                if (out.items.size() > ProjectDocument::MaxItems)
                {
                    out.failed = true;
                    out.error = "Import exceeds the scene item limit.";
                }
                if (out.failed)
                {
                    out.items.clear();
                }
            }

            private:
            std::string Reference(Tokens tokens, std::size_t& pos) const
            {
                if (pos + 1 < tokens.size() && tokens[pos + 1].text == ":")
                {
                    pos += 2;
                }
                if (pos >= tokens.size() || tokens[pos].kind != Kind::Identifier)
                {
                    return {};
                }
                std::string key = tokens[pos++].text;
                while (pos < tokens.size() && tokens[pos].text == "[")
                {
                    const auto start = ++pos;
                    while (pos < tokens.size() && tokens[pos].text != "]" && pos - start < 64)
                    {
                        ++pos;
                    }
                    if (pos == tokens.size() || tokens[pos].text != "]")
                    {
                        return {};
                    }
                    const auto index = Expression(tokens.subspan(start, pos - start), values).Read();
                    if (!index || *index < 0 || *index > 1000000 || std::trunc(*index) != *index)
                    {
                        return {};
                    }
                    key += "[" + std::to_string(static_cast<int>(*index)) + "]";
                    ++pos;
                }
                return key;
            }

            std::optional<double> Number(const Call& call, std::size_t index) const
            {
                if (index >= call.args.size())
                {
                    return std::nullopt;
                }
                return Expression(call.args[index], values).Read();
            }
            bool Integer(const Call& call, std::size_t index, int& value, int minimum, int maximum) const
            {
                const auto number = Number(call, index);
                if (!number || *number < minimum || *number > maximum || std::trunc(*number) != *number)
                {
                    return false;
                }
                value = static_cast<int>(*number);
                return true;
            }
            bool Float(const Call& call, std::size_t index, float& value) const
            {
                const auto number = Number(call, index);
                if (!number || std::abs(*number) > std::numeric_limits<float>::max())
                {
                    return false;
                }
                value = static_cast<float>(*number);
                return true;
            }
            bool Color(const Call& call, std::size_t index, unsigned int& value) const
            {
                const auto number = Number(call, index);
                if (!number || *number < -2147483648.0 || *number > 4294967295.0 || std::trunc(*number) != *number)
                {
                    return false;
                }
                value = static_cast<unsigned int>(static_cast<std::int64_t>(*number));
                return true;
            }

            void Distance(const Call& call, std::size_t index, float& value)
            {
                if (index >= call.args.size())
                {
                    return;
                }
                float parsed = 0;
                if (Float(call, index, parsed) && parsed >= 0)
                {
                    value = parsed;
                }
            }

            bool DefaultArgument(const Call& call, std::size_t index) const
            {
                return index >= call.args.size() || (call.args[index].size() == 1 && call.args[index][0].text == "_");
            }

            void Process(const Call& call, const std::string& assignment);
            void RemoveSlot(ItemData& d, int index);
            void Material(const Call& call, ItemData& d);
            void MaterialText(const Call& call, ItemData& d, bool dynamic);

            const Environment& env;
            Result& out;
            Numbers values;
            Numbers constants;
            std::unordered_map<std::string, std::size_t> references;
            std::vector<ImportedItem> items;
        };

#include "pawn_import_entities.inl"
#include "pawn_import_materials.inl"
    }

    Result Parse(std::string_view source, const Environment& environment)
    {
        Result result;
        if (source.size() > MaxSourceBytes)
        {
            result.failed = true;
            result.error = "Clipboard code exceeds 4 MiB.";
            return result;
        }
        auto tokens = Tokenize(source, result);
        if (!result.failed)
        {
            Parser(environment, result).Run(tokens);
        }
        return result;
    }
}
