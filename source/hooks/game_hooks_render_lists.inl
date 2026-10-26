// Source Repository: https://github.com/waverso/waversomapstudio

enum : unsigned int
{
    RenderListCapacity = 4096
};
static CEntity* g_visibleEntities[RenderListCapacity];
static CEntity* g_visibleLods[RenderListCapacity];

static const struct RenderListReference
{
    uintptr_t address;
    uint8_t opcode[3];
    CEntity** original;
    CEntity** expanded;
} g_renderListReferences[] = {
    { 0x553529, { 0x89, 0x34, 0x85 }, (CEntity**)0xB75898, g_visibleEntities },
    { 0x553944, { 0x8B, 0x0C, 0xB5 }, (CEntity**)0xB75898, g_visibleEntities },
    { 0x553A53, { 0x8B, 0x34, 0xBD }, (CEntity**)0xB75898, g_visibleEntities },
    { 0x553B03, { 0x8B, 0x34, 0xBD }, (CEntity**)0xB75898, g_visibleEntities },
    { 0x5534F5, { 0x89, 0x34, 0x85 }, (CEntity**)0xB748F8, g_visibleLods },
    { 0x553923, { 0x8B, 0x0C, 0xB5 }, (CEntity**)0xB748F8, g_visibleLods },
    { 0x553CB3, { 0x8B, 0x0C, 0xB5 }, (CEntity**)0xB748F8, g_visibleLods },
};

static bool ValidateRenderLists()
{
    const uint8_t entityLoad[] = { 0xA1, 0x44, 0x68, 0xB7, 0x00 };
    const uint8_t lodLoad[] = { 0xA1, 0x40, 0x68, 0xB7, 0x00 };
    const uint8_t epilogue[] = { 0x5E, 0xC3 };
    if (memcmp((void*)0x553521, entityLoad, sizeof(entityLoad)) != 0 || memcmp((void*)0x5534ED, lodLoad, sizeof(lodLoad)) != 0
        || memcmp((void*)0x553533, epilogue, sizeof(epilogue)) != 0 || memcmp((void*)0x5534FF, epilogue, sizeof(epilogue)) != 0)
    {
        return false;
    }

    for (const auto& ref : g_renderListReferences)
    {
        if (memcmp((void*)(ref.address - 3), ref.opcode, sizeof(ref.opcode)) != 0
            || plugin::patch::GetPointer(ref.address) != ref.original)
        {
            return false;
        }
    }

    return CRenderer::ms_nNoOfVisibleEntities <= 1000 && CRenderer::ms_nNoOfVisibleLods <= 1000;
}

static void SetRenderLists(bool expanded)
{
    auto transfer = [expanded](CEntity** original, CEntity** buffer, unsigned int& count)
    {
        count = std::min(count, 1000u);
        memcpy(expanded ? buffer : original, expanded ? original : buffer, count * sizeof(CEntity*));
    };
    transfer((CEntity**)0xB75898, g_visibleEntities, CRenderer::ms_nNoOfVisibleEntities);
    transfer((CEntity**)0xB748F8, g_visibleLods, CRenderer::ms_nNoOfVisibleLods);

    for (const auto& ref : g_renderListReferences)
    {
        plugin::patch::SetPointer(ref.address, expanded ? ref.expanded : ref.original);
    }

    CRenderer::ms_aVisibleEntityPtrs = expanded ? g_visibleEntities : (CEntity**)0xB75898;
    CRenderer::ms_aVisibleLodPtrs = expanded ? g_visibleLods : (CEntity**)0xB748F8;
    MAX_VISIBLE_ENTITY_PTRS = MAX_VISIBLE_LOD_PTRS = expanded ? RenderListCapacity : 1000;
    FlushInstructionCache(GetCurrentProcess(), (void*)0x5534ED, 0x553CB7 - 0x5534ED);
}

static void __declspec(naked) HookAppendVisibleEntity()
{
    __asm
    {
		mov eax, dword ptr ds:[0xB76844]
		cmp eax, RenderListCapacity
		jae clamp
		mov dword ptr [g_visibleEntities + eax * 4], esi
		inc eax
		mov dword ptr ds:[0xB76844], eax
		jmp done
	clamp:
		mov eax, RenderListCapacity
		mov dword ptr ds:[0xB76844], eax
	done:
		push 0x553533
		retn
    }
}

static void __declspec(naked) HookAppendVisibleLod()
{
    __asm
    {
		mov eax, dword ptr ds:[0xB76840]
		cmp eax, RenderListCapacity
		jae clamp
		mov dword ptr [g_visibleLods + eax * 4], esi
		inc eax
		mov dword ptr ds:[0xB76840], eax
		jmp done
	clamp:
		mov eax, RenderListCapacity
		mov dword ptr ds:[0xB76840], eax
	done:
		push 0x5534FF
		retn
    }
}
