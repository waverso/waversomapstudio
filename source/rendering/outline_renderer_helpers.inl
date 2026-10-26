// Source Repository: https://github.com/waverso/waversomapstudio

namespace
{
    static const auto RwFrameGetLTMFn = (RwMatrix * (__cdecl*)(RwFrame*))0x7F0990;

    DWORD FloatBits(float value)
    {
        DWORD bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    D3DMATRIX ToD3DMatrix(const RwMatrix& matrix)
    {
        D3DMATRIX result = {};
        result._11 = matrix.right.x;
        result._12 = matrix.right.y;
        result._13 = matrix.right.z;
        result._21 = matrix.up.x;
        result._22 = matrix.up.y;
        result._23 = matrix.up.z;
        result._31 = matrix.at.x;
        result._32 = matrix.at.y;
        result._33 = matrix.at.z;
        result._41 = matrix.pos.x;
        result._42 = matrix.pos.y;
        result._43 = matrix.pos.z;
        result._44 = 1.0f;
        return result;
    }

    class DeviceStateGuard
    {
        public:
        explicit DeviceStateGuard(IDirect3DDevice9* device) : m_device(device)
        {
            if (!m_device)
            {
                return;
            }

            D3DCAPS9 caps = {};
            if (FAILED(m_device->GetDeviceCaps(&caps)))
            {
                return;
            }
            m_renderTargetCount = std::clamp<UINT>(caps.NumSimultaneousRTs, 1, MaxRenderTargets);
            for (UINT target = 0; target < m_renderTargetCount; ++target)
            {
                const HRESULT result = m_device->GetRenderTarget(target, &m_renderTargets[target]);
                if (FAILED(result) && result != D3DERR_NOTFOUND)
                {
                    return;
                }
            }
            if (!m_renderTargets[0])
            {
                return;
            }
            if (FAILED(m_device->GetViewport(&m_viewport)) || m_viewport.Width == 0 || m_viewport.Height == 0)
            {
                return;
            }

            const HRESULT depthResult = m_device->GetDepthStencilSurface(&m_depthStencil);
            if (FAILED(depthResult) && depthResult != D3DERR_NOTFOUND)
            {
                return;
            }

            if (FAILED(m_device->GetScissorRect(&m_scissor)))
            {
                return;
            }
            m_hasScissor = true;
            if (FAILED(m_device->CreateStateBlock(D3DSBT_ALL, &m_stateBlock)) || !m_stateBlock)
            {
                return;
            }
            if (FAILED(m_stateBlock->Capture()))
            {
                return;
            }
            m_valid = true;
        }

        ~DeviceStateGuard()
        {
            Restore();
            if (m_stateBlock)
            {
                m_stateBlock->Release();
            }
            if (m_depthStencil)
            {
                m_depthStencil->Release();
            }
            for (IDirect3DSurface9*& renderTarget : m_renderTargets)
            {
                if (renderTarget)
                {
                    renderTarget->Release();
                }
                renderTarget = nullptr;
            }
        }

        bool IsValid() const
        {
            return m_valid;
        }
        IDirect3DSurface9* DepthStencil() const
        {
            return m_depthStencil;
        }
        const D3DVIEWPORT9& Viewport() const
        {
            return m_viewport;
        }

        bool RestoreTargets()
        {
            if (!m_valid)
            {
                return false;
            }

            bool ok = SUCCEEDED(m_device->SetRenderTarget(0, m_renderTargets[0]));
            for (UINT target = 1; target < m_renderTargetCount; ++target)
            {
                ok = SUCCEEDED(m_device->SetRenderTarget(target, m_renderTargets[target])) && ok;
            }
            ok = SUCCEEDED(m_device->SetDepthStencilSurface(m_depthStencil)) && ok;
            ok = SUCCEEDED(m_device->SetViewport(&m_viewport)) && ok;
            if (m_hasScissor)
            {
                ok = SUCCEEDED(m_device->SetScissorRect(&m_scissor)) && ok;
            }
            return ok;
        }

        void Restore()
        {
            if (!m_valid || m_restored)
            {
                return;
            }
            if (m_stateBlock)
            {
                m_stateBlock->Apply();
            }
            RestoreTargets();
            m_restored = true;
        }

        private:
        static constexpr UINT MaxRenderTargets = 4;
        IDirect3DDevice9* m_device = nullptr;
        IDirect3DStateBlock9* m_stateBlock = nullptr;
        IDirect3DSurface9* m_renderTargets[MaxRenderTargets] = {};
        IDirect3DSurface9* m_depthStencil = nullptr;
        UINT m_renderTargetCount = 1;
        D3DVIEWPORT9 m_viewport = {};
        RECT m_scissor = {};
        bool m_hasScissor = false;
        bool m_valid = false;
        bool m_restored = false;
    };

    class MaskPassGuard
    {
        public:
        explicit MaskPassGuard(bool& active) : m_active(active)
        {
            m_active = true;
        }
        ~MaskPassGuard()
        {
            m_active = false;
        }

        private:
        bool& m_active;
    };

    bool DrawAtomicMaskInstanced(IDirect3DDevice9* device, RpAtomic* atomic)
    {
        if (!device || !atomic || !(RpAtomicGetFlags(atomic) & rpATOMICRENDER))
        {
            return false;
        }
        RwResEntry* resEntry = atomic->repEntry;
        if (!resEntry && atomic->geometry)
        {
            resEntry = atomic->geometry->repEntry;
        }
        if (!resEntry)
        {
            return false;
        }

        auto* header = reinterpret_cast<RxD3D9ResEntryHeader*>(resEntry + 1);
        auto* meshes = reinterpret_cast<RxD3D9InstanceData*>(header + 1);
        if (!header->numMeshes || !header->vertexDeclaration || !header->indexBuffer || !header->vertexStream[0].vertexBuffer)
        {
            return false;
        }

        RwFrame* frame = reinterpret_cast<RwFrame*>(reinterpret_cast<RwObject*>(atomic)->parent);
        if (!frame)
        {
            return false;
        }
        RwMatrix* ltm = RwFrameGetLTMFn(frame);
        if (!ltm)
        {
            return false;
        }

        const D3DMATRIX world = ToD3DMatrix(*ltm);
        if (FAILED(device->SetTransform(D3DTS_WORLD, &world)))
        {
            return false;
        }
        if (FAILED(device->SetVertexDeclaration(static_cast<IDirect3DVertexDeclaration9*>(header->vertexDeclaration))))
        {
            return false;
        }

        for (UINT stream = 0; stream < RWD3D9_MAX_VERTEX_STREAMS; ++stream)
        {
            const RxD3D9VertexStream& source = header->vertexStream[stream];
            auto* vertexBuffer = static_cast<IDirect3DVertexBuffer9*>(source.vertexBuffer);
            const UINT offset = header->useOffsets ? source.offset : 0;
            if (FAILED(
                    device->SetStreamSource(stream, vertexBuffer, vertexBuffer ? offset : 0, vertexBuffer ? source.stride : 0)))
            {
                return false;
            }
        }

        if (FAILED(device->SetIndices(static_cast<IDirect3DIndexBuffer9*>(header->indexBuffer))))
        {
            return false;
        }

        bool rendered = false;
        for (RwUInt32 mesh = 0; mesh < header->numMeshes; ++mesh)
        {
            const RxD3D9InstanceData& data = meshes[mesh];
            if (!data.numVertices || !data.numPrimitives)
            {
                continue;
            }

            const HRESULT result = device->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(header->primType),
                static_cast<INT>(data.baseIndex), data.minVert, data.numVertices, data.startIndex, data.numPrimitives);
            rendered = SUCCEEDED(result) || rendered;
        }
        return rendered;
    }

    bool DrawActorMask(IDirect3DDevice9* device, CEntity* entity)
    {
        const auto* meshes = g_actorRuntime ? g_actorRuntime->Meshes(entity) : nullptr;
        if (!meshes)
        {
            return false;
        }
        D3DXMATRIX identity;
        D3DXMatrixIdentity(&identity);
        device->SetTransform(D3DTS_WORLD, &identity);
        device->SetFVF(D3DFVF_XYZ);
        bool rendered = false;
        for (const auto& mesh : *meshes)
        {
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                continue;
            }
            rendered |= SUCCEEDED(device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(mesh.vertices.size()),
                static_cast<UINT>(mesh.indices.size() / 3), mesh.indices.data(), D3DFMT_INDEX16, mesh.vertices.data(),
                sizeof(CVector)));
        }
        return rendered;
    }

    bool IsVehicleAtomicInRange(const RpAtomic* atomic, float distance)
    {
        switch (reinterpret_cast<std::uintptr_t>(atomic->renderCallBack))
        {
            case 0x7331E0:
            case 0x7334F0:
                return distance >= CVisibilityPlugins::ms_vehicleLod0Dist;
            case 0x732820:
                return distance >= CVisibilityPlugins::ms_bigVehicleLod0Dist;
            case 0x733240:
            case 0x733F80:
            case 0x733550:
            case 0x7344A0:
            case 0x734170:
                return distance < CVisibilityPlugins::ms_vehicleLod0Dist;
            case 0x733420:
            case 0x734370:
            case 0x733330:
            case 0x734240:
            case 0x7340B0:
                return distance < CVisibilityPlugins::ms_bigVehicleLod0Dist;
            default:
                return true;
        }
    }

    struct ClumpMaskContext
    {
        IDirect3DDevice9* device = nullptr;
        float vehicleDistance = -1.0f;
        bool rendered = false;
    };

    RpAtomic* DrawClumpAtomicMask(RpAtomic* atomic, void* data)
    {
        ClumpMaskContext* context = static_cast<ClumpMaskContext*>(data);
        if (context && (context->vehicleDistance < 0.0f || IsVehicleAtomicInRange(atomic, context->vehicleDistance))
            && DrawAtomicMaskInstanced(context->device, atomic))
        {
            context->rendered = true;
        }
        return atomic;
    }

    bool DrawEntityMaskInstanced(IDirect3DDevice9* device, CEntity* entity)
    {
        if (!device || !entity || !entity->m_pRwObject)
        {
            return false;
        }

        const RwUInt8 type = RwObjectGetType(entity->m_pRwObject);
        if (type == rpATOMIC)
        {
            return DrawAtomicMaskInstanced(device, entity->m_pRwAtomic);
        }
        if (type != rpCLUMP)
        {
            return false;
        }

        ClumpMaskContext context = {};
        context.device = device;
        if (entity->m_nType == ENTITY_TYPE_VEHICLE)
        {
            context.vehicleDistance = CVisibilityPlugins::GetDistanceSquaredFromCamera(RpClumpGetFrame(entity->m_pRwClump));
        }
        RpClumpForAllAtomics(entity->m_pRwClump, DrawClumpAtomicMask, &context);
        return context.rendered;
    }
}
