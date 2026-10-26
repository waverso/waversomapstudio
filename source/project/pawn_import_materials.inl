// Source Repository: https://github.com/waverso/waversomapstudio

inline void Parser::RemoveSlot(ItemData& d, int index)
{
    for (int i = d.MaterialCount() - 1; i >= 0; --i)
    {
        if (d.materials[i].materialIndex != index)
        {
            continue;
        }
        for (int j = i; j + 1 < d.MaterialCount(); ++j)
        {
            d.materials[j] = d.materials[j + 1];
        }
        d.materials.pop_back();
    }
    for (int i = d.MaterialTextCount() - 1; i >= 0; --i)
    {
        if (d.materialTexts[i].materialIndex != index)
        {
            continue;
        }
        for (int j = i; j + 1 < d.MaterialTextCount(); ++j)
        {
            d.materialTexts[j] = d.materialTexts[j + 1];
        }
        d.materialTexts.pop_back();
    }
}

inline void Parser::Material(const Call& call, ItemData& d)
{
    MaterialData material;
    material.colorArgb = 0;
    if (call.args.size() < 5 || !Integer(call, 1, material.materialIndex, 0, 15)
        || !Integer(call, 2, material.textureModelId, -1, 19999) || !CopyString(call.args[3], material.txdName)
        || !CopyString(call.args[4], material.textureName) || (call.args.size() > 5 && !Color(call, 5, material.colorArgb)))
    {
        return;
    }
    RemoveSlot(d, material.materialIndex);
    if (d.MaterialCount() >= ItemData::MaxMaterials)
    {
        return;
    }
    d.materials.emplace_back();
    d.materials.back() = material;
    d.materials.back().sequence = d.nextSeq++;
}

inline void Parser::MaterialText(const Call& call, ItemData& d, bool dynamic)
{
    MaterialTextData material;
    material.backgroundColorArgb = 0;
    const std::size_t textIndex = dynamic ? 2 : 1;
    const std::size_t slotIndex = dynamic ? 1 : 2;
    bool valid = call.args.size() > textIndex && CopyString(call.args[textIndex], material.text);
    if (call.args.size() > slotIndex)
    {
        valid = Integer(call, slotIndex, material.materialIndex, 0, 15) && valid;
    }
    if (call.args.size() > 3)
    {
        valid = Integer(call, 3, material.materialSize, 10, 140) && material.materialSize % 10 == 0 && valid;
    }
    if (call.args.size() > 4)
    {
        valid = CopyString(call.args[4], material.fontFace) && valid;
    }
    if (call.args.size() > 5)
    {
        valid = Integer(call, 5, material.fontSize, 1, 255) && valid;
    }
    if (call.args.size() > 6)
    {
        int bold = 0;
        valid = Integer(call, 6, bold, 0, 1) && valid;
        material.bold = bold != 0;
    }
    if (call.args.size() > 7)
    {
        valid = Color(call, 7, material.fontColorArgb) && valid;
    }
    if (call.args.size() > 8)
    {
        valid = Color(call, 8, material.backgroundColorArgb) && valid;
    }
    if (call.args.size() > 9)
    {
        valid = Integer(call, 9, material.alignment, 0, 2) && valid;
    }
    if (call.args.size() > 10)
    {
        valid = false;
    }
    if (!valid)
    {
        return;
    }
    RemoveSlot(d, material.materialIndex);
    if (d.MaterialTextCount() >= ItemData::MaxMaterials)
    {
        return;
    }
    d.materialTexts.emplace_back();
    d.materialTexts.back() = material;
    d.materialTexts.back().sequence = d.nextSeq++;
}
