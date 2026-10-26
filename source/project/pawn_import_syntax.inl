// Source Repository: https://github.com/waverso/waversomapstudio

bool IsFunction(std::string_view name)
{
    return name == "CreateVehicle" || name == "AddStaticVehicle" || name == "SetVehicleHealth" || name == "AddStaticVehicleEx"
        || name == "CreateObject" || name == "CreateDynamicObject" || name == "CreateDynamicObjectEx" || name == "DestroyObject"
        || name == "DestroyDynamicObject" || name == "SetObjectMaterial" || name == "SetDynamicObjectMaterial"
        || name == "SetObjectMaterialText" || name == "SetDynamicObjectMaterialText" || name == "RemoveBuildingForPlayer"
        || name == "CreatePickup" || name == "AddStaticPickup" || name == "CreateDynamicPickup" || name == "CreateDynamicPickupEx"
        || name == "DestroyPickup" || name == "DestroyDynamicPickup" || name == "CreateActor" || name == "CreateDynamicActor"
        || name == "CreateDynamicActorEx" || name == "DestroyActor" || name == "DestroyDynamicActor" || name == "SetActorHealth"
        || name == "SetDynamicActorHealth" || name == "SetActorInvulnerable" || name == "SetDynamicActorInvulnerable"
        || name == "SetActorPos" || name == "SetDynamicActorPos" || name == "SetActorFacingAngle"
        || name == "SetDynamicActorFacingAngle" || name == "ApplyActorAnimation" || name == "ApplyDynamicActorAnimation"
        || name == "ClearActorAnimations" || name == "ClearDynamicActorAnimations";
}

struct Call
{
    std::string name;
    std::vector<Tokens> args;
    std::size_t end = 0;
};

Call ReadCall(Tokens tokens, std::size_t begin)
{
    Call call;
    call.name = tokens[begin].text;
    std::size_t start = begin + 2;
    std::size_t i = start;
    int parens = 0, brackets = 0, braces = 0;
    bool closed = false;
    for (; i < tokens.size(); ++i)
    {
        const auto& t = tokens[i];
        if (parens == 0 && brackets == 0 && braces == 0)
        {
            if (t.text == ")")
            {
                closed = true;
                break;
            }
            const bool nextCall
                = t.kind == Kind::Identifier && IsFunction(t.text) && i + 1 < tokens.size() && tokens[i + 1].text == "(";
            bool nextAssignment
                = t.kind == Kind::Identifier && i + 1 < tokens.size() && (tokens[i + 1].text == "=" || t.text == "new");
            if (t.kind == Kind::Identifier && i + 1 < tokens.size() && tokens[i + 1].text == "[")
            {
                std::size_t end = i + 2;
                while (end < tokens.size() && end - i < 64 && tokens[end].text != "]")
                {
                    ++end;
                }
                nextAssignment = end + 1 < tokens.size() && tokens[end].text == "]" && tokens[end + 1].text == "=";
            }
            if (t.text == ";" || t.text == "}" || nextCall || (i > start && t.line > tokens[i - 1].line && nextAssignment))
            {
                break;
            }
            if (t.text == ",")
            {
                call.args.push_back(tokens.subspan(start, i - start));
                start = i + 1;
                continue;
            }
        }
        if (t.text == "(")
        {
            ++parens;
        }
        if (t.text == ")")
        {
            --parens;
        }
        if (t.text == "[")
        {
            ++brackets;
        }
        if (t.text == "]")
        {
            --brackets;
        }
        if (t.text == "{")
        {
            ++braces;
        }
        if (t.text == "}")
        {
            --braces;
        }
        if (parens < 0 || brackets < 0 || braces < 0 || parens + brackets + braces > 32)
        {
            break;
        }
    }
    if (i > start)
    {
        call.args.push_back(tokens.subspan(start, i - start));
    }
    call.end = i + (closed ? 1 : 0);
    return call;
}

std::size_t SkipBalanced(Tokens tokens, std::size_t pos, std::string_view open, std::string_view close)
{
    int depth = 0;
    do
    {
        if (tokens[pos].text == open)
        {
            ++depth;
        }
        if (tokens[pos].text == close)
        {
            --depth;
        }
        ++pos;
    } while (pos < tokens.size() && depth > 0);
    return pos;
}

bool ControlFlow(std::string_view text)
{
    return text == "if" || text == "else" || text == "for" || text == "while" || text == "switch" || text == "do";
}

std::size_t SkipStatement(Tokens tokens, std::size_t pos, int depth = 0)
{
    if (pos >= tokens.size() || depth > 32)
    {
        return tokens.size();
    }
    const auto first = tokens[pos].text;
    if (ControlFlow(first))
    {
        ++pos;
        if (pos < tokens.size() && tokens[pos].text == "(")
        {
            pos = SkipBalanced(tokens, pos, "(", ")");
        }
        pos = SkipStatement(tokens, pos, depth + 1);
        if (first == "if" && pos < tokens.size() && tokens[pos].text == "else")
        {
            pos = SkipStatement(tokens, pos + 1, depth + 1);
        }
        return pos;
    }
    if (first == "{")
    {
        return SkipBalanced(tokens, pos, "{", "}");
    }
    const auto line = tokens[pos].line;
    while (pos < tokens.size() && tokens[pos].text != ";" && tokens[pos].text != "}")
    {
        if (tokens[pos].text == "(")
        {
            pos = SkipBalanced(tokens, pos, "(", ")");
        }
        else if (tokens[pos].line > line)
        {
            break;
        }
        else
        {
            ++pos;
        }
    }
    return pos + (pos < tokens.size() && tokens[pos].text == ";" ? 1 : 0);
}

template <std::size_t N>
bool CopyString(Tokens tokens, char (&destination)[N])
{
    if (tokens.size() != 1 || tokens[0].kind != Kind::String || !tokens[0].valid || tokens[0].text.size() >= N)
    {
        return false;
    }
    std::memset(destination, 0, N);
    std::memcpy(destination, tokens[0].text.data(), tokens[0].text.size());
    return true;
}
