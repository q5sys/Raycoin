//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#define NOMINMAX

#include "d3d12.h"
#include "d3d12video.h"
#include "d3d12_1.h"
#include "dxgi1_3.h"
#include "GameCore.h"
#include "BufferManager.h"
#include "VectorMath.h"
#include "ReadbackBuffer.h"
#include "CommandContext.h"
#include "SamplerManager.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include <atlbase.h>
#include <atlbase.h>
#include <array>
#include <unordered_map>
#include <ctime>
#include "blake2.h"
#include <fstream>
#include <iomanip>
#include <Shlobj.h>

#include "CompiledShaders/rayGenerationShader.h"
#include "CompiledShaders/hitShader.h"
#include "CompiledShaders/missShader.h"
#ifndef COMPUTE_ONLY
    #include "CompiledShaders/lineHitShader.h"
#endif
#include "CompiledShaders/initFieldCS.h"

#include "D3D12RaytracingFallback.h"

using namespace std;
using namespace GameCore;
using namespace Graphics;
using namespace Math;

#include "RaycoinViewerRayTracing.h"
#include "RaycoinViewerCompute.h"
#include "RaycoinViewer.h"


static const int g_fov = 120;
static const float g_zRange[2] = {0.1, 1000};
static const int g_maxRayRecursion = 2;
static const string g_traceLogFile = "raytraces.log";

CComPtr<ID3D12RaytracingFallbackDevice> g_pRaytracingDevice;

HitShaderConstants          g_hitShaderConstants;
ByteAddressBuffer           g_hitShaderConstantBuffer;
ByteAddressBuffer           g_initFieldConstantBuffer;

StructuredBuffer            g_indexBuffer;
StructuredBuffer            g_vertexBuffer;
StructuredBuffer            g_attribBuffer;

StructuredBuffer            g_lineIndexBuffer;
StructuredBuffer            g_lineVertexBuffer;
StructuredBuffer            g_lineAttribBuffer;

D3D12_GPU_DESCRIPTOR_HANDLE g_OutputUAV;

vector<ByteAddressBuffer>   g_bottomLevelAccels;
ByteAddressBuffer           g_topLevelAccel;
WRAPPED_GPU_POINTER         g_topLevelAccelPointer;
D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC* g_instanceDescs = nullptr;
ByteAddressBuffer           g_instanceDescsBuffer;
ReadbackBuffer              g_instanceDescsRead;
ByteAddressBuffer           g_scratchBuffer;

CComPtr<ID3D12RootSignature> g_GlobalRaytracingRootSignature;
CComPtr<ID3D12RootSignature> g_LocalRaytracingRootSignature;

StructuredBuffer            g_traceResultBuffer;
ReadbackBuffer              g_traceResultRead;
TraceResult                 g_traceResult;

float                       g_rayLen = 0;
float                       g_time = 0;
multimap<int, int>          g_rayHitMap;
array<int, g_hitCount>      g_rayHitInsts = {};

NumVar g_diffuseColor_r("Application/Lighting/Diffuse Color \200R", 1, 0, 1, 0.01);
NumVar g_diffuseColor_g("Application/Lighting/Diffuse Color \201G", 1, 0, 1, 0.01);
NumVar g_diffuseColor_b("Application/Lighting/Diffuse Color \202B", 1, 0, 1, 0.01);
ExpVar g_diffuse("Application/Lighting/Diffuse Intensity", 0.1, -20, 13, 0.1);
ExpVar g_gloss("Application/Lighting/Specular Gloss", 128, -20, 13, 0.1);
ExpVar g_specular("Application/Lighting/Specular Intensity", 1, -20, 13, 0.1);
NumVar g_sunColor_r("Application/Lighting/Sun Color \200R", 1, 0, 1, 0.01);
NumVar g_sunColor_g("Application/Lighting/Sun Color \201G", 1, 0, 1, 0.01);
NumVar g_sunColor_b("Application/Lighting/Sun Color \202B", 1, 0, 1, 0.01);
ExpVar g_sun("Application/Lighting/Sun Intensity", 4, -20, 13, 0.1);
NumVar g_sunOrientation("Application/Lighting/Sun \200Orientation", -0.5, -XM_PI, XM_PI, 0.1);
NumVar g_sunInclination("Application/Lighting/Sun \201Inclination", 0.75, 0, 1, 0.01);
NumVar g_ambientColor_r("Application/Lighting/Ambient Color \200R", 1, 0, 1, 0.01);
NumVar g_ambientColor_g("Application/Lighting/Ambient Color \201G", 1, 0, 1, 0.01);
NumVar g_ambientColor_b("Application/Lighting/Ambient Color \202B", 1, 0, 1, 0.01);
ExpVar g_ambient("Application/Lighting/Ambient Intensity", 0.1, -20, 13, 0.1);
NumVar g_bgColor_r("Application/Lighting/Background Color \200R", 1, 0, 1, 0.01);
NumVar g_bgColor_g("Application/Lighting/Background Color \201G", 0, 0, 1, 0.01);
NumVar g_bgColor_b("Application/Lighting/Background Color \202B", 1, 0, 1, 0.01);
IntVar g_shadePathMax("Application/Lighting/Reflections", 4, 0, g_pathMax-1, 1);
ExpVar g_reflect("Application/Lighting/Reflectivity", 1, -20, 0, 0.1);
ExpVar g_reflectFull("Application/Lighting/Reflectivity Full Surface", 0, -20, 0, 0.1);
BoolVar g_shadeLastMissOnly("Application/Lighting/Shade Last Miss Only", true);

NumVar g_rayColorStart_r("Application/Ray/Color/\200Start \200R", 1, 0, 1, 0.01);
NumVar g_rayColorStart_g("Application/Ray/Color/\200Start \201G", 1, 0, 1, 0.01);
NumVar g_rayColorStart_b("Application/Ray/Color/\200Start \202B", 0, 0, 1, 0.01);
NumVar g_rayColorMid_r("Application/Ray/Color/\201Middle \200R", 0.5, 0, 1, 0.01);
NumVar g_rayColorMid_g("Application/Ray/Color/\201Middle \201G", 1, 0, 1, 0.01);
NumVar g_rayColorMid_b("Application/Ray/Color/\201Middle \202B", 0.5, 0, 1, 0.01);
NumVar g_rayColorEnd_r("Application/Ray/Color/\202End \200R", 0, 0, 1, 0.01);
NumVar g_rayColorEnd_g("Application/Ray/Color/\202End \201G", 1, 0, 1, 0.01);
NumVar g_rayColorEnd_b("Application/Ray/Color/\202End \202B", 0.1, 0, 1, 0.01);
NumVar g_rayFailColorStart_r("Application/Ray/Fail Color/\200Start \200R", 1, 0, 1, 0.01);
NumVar g_rayFailColorStart_g("Application/Ray/Fail Color/\200Start \201G", 0, 0, 1, 0.01);
NumVar g_rayFailColorStart_b("Application/Ray/Fail Color/\200Start \202B", 0, 0, 1, 0.01);
NumVar g_rayFailColorMid_r("Application/Ray/Fail Color/\201Middle \200R", 1, 0, 1, 0.01);
NumVar g_rayFailColorMid_g("Application/Ray/Fail Color/\201Middle \201G", 0.3, 0, 1, 0.01);
NumVar g_rayFailColorMid_b("Application/Ray/Fail Color/\201Middle \202B", 0, 0, 1, 0.01);
NumVar g_rayFailColorEnd_r("Application/Ray/Fail Color/\202End \200R", 0.1, 0, 1, 0.01);
NumVar g_rayFailColorEnd_g("Application/Ray/Fail Color/\202End \201G", 0.1, 0, 1, 0.01);
NumVar g_rayFailColorEnd_b("Application/Ray/Fail Color/\202End \202B", 1, 0, 1, 0.01);
NumVar g_raySize("Application/Ray/Size", 0.04, 0.01, 1, 0.01);
NumVar g_rayArrowSize("Application/Ray/Size Arrow", 0.6, 0.1, 10, 0.1);
ExpVar g_rayMin("Application/Ray/Intensity Min", 1, -20, 13, 0.1);
ExpVar g_rayMax("Application/Ray/Intensity Max", 100, -20, 13, 0.1);
ExpVar g_rayHitMin("Application/Ray/Hit Intensity Min", 0, -20, 13, 0.1);
ExpVar g_rayHitMax("Application/Ray/Hit Intensity Max", 1, -20, 13, 0.1);

BoolVar g_showInfo("Application/UI/Show Info", true);
BoolVar g_showLabels("Application/UI/Show Labels", true);
BoolVar g_showHelp("Application/UI/Show Help", true);
NumVar g_textColor_r("Application/UI/Text Color \200R", 1, 0, 1, 0.01);
NumVar g_textColor_g("Application/UI/Text Color \201G", 1, 0, 1, 0.01);
NumVar g_textColor_b("Application/UI/Text Color \202B", 0, 0, 1, 0.01);
NumVar g_textColor_a("Application/UI/Text Color \203A", 1, 0, 1, 0.01);
NumVar g_textHiColor_r("Application/UI/Text Highlight \200R", 1, 0, 1, 0.01);
NumVar g_textHiColor_g("Application/UI/Text Highlight \201G", 0.25, 0, 1, 0.01);
NumVar g_textHiColor_b("Application/UI/Text Highlight \202B", 0.25, 0, 1, 0.01);
NumVar g_textHiColor_a("Application/UI/Text Highlight \203A", 1, 0, 1, 0.01);
NumVar g_labelColor_r("Application/UI/Label Color \200R", 1, 0, 1, 0.01);
NumVar g_labelColor_g("Application/UI/Label Color \201G", 1, 0, 1, 0.01);
NumVar g_labelColor_b("Application/UI/Label Color \202B", 0, 0, 1, 0.01);
NumVar g_labelColor_a("Application/UI/Label Color \203A", 0.7, 0, 1, 0.01);
NumVar g_labelHiColor_r("Application/UI/Label Highlight \200R", 1, 0, 1, 0.01);
NumVar g_labelHiColor_g("Application/UI/Label Highlight \201G", 0.25, 0, 1, 0.01);
NumVar g_labelHiColor_b("Application/UI/Label Highlight \202B", 0.25, 0, 1, 0.01);
NumVar g_labelHiColor_a("Application/UI/Label Highlight \203A", 0.7, 0, 1, 0.01);
NumVar g_labelSize("Application/UI/Label Size", 8, 4, 20, 0.2);
NumVar g_labelSizeScale("Application/UI/Label Size Scale", 2, 1, 4, 0.1);

BoolVar g_hashing("Application/Hashing", true);
IntVar g_difficulty("Application/Difficulty", 24, 0, 256, 1);
IntVar g_traceLogCur("Application/Trace Log #", 0, 0, 0, 1);
NumVar g_moveSpeed("Application/Move Speed", 50, 5, 500, 5);

enum Mode
{
    MODE_COMPUTE,
    MODE_DIFFUSE_REFLECT,
    MODE_DIFFUSE
};
const char* g_modes[] = {
    "Compute",
    "Diffuse & Reflect",
    "Diffuse"
};
EnumVar g_mode("Application/Mode", MODE_COMPUTE, _countof(g_modes), g_modes);

enum CameraPresetType
{
    CAMERAPRESET_ORIGIN,
    CAMERAPRESET_RIGHT,
    CAMERAPRESET_TOP,
    CAMERAPRESET_BEHIND,
    CAMERAPRESET_CORNER,
    CAMERAPRESET_HIT
};
const char* g_cameraPresetStrs[] = {
    "Origin",
    "Right",
    "Top",
    "Behind",
    "Corner",
    "Hit"
};
static const int g_cameraPresetCount = _countof(g_cameraPresetStrs);
RaycoinViewer::CameraPreset RaycoinViewer::s_cameraPresets[g_cameraPresetCount];
EnumVar g_cameraPresetCur("Application/Camera Preset", CAMERAPRESET_ORIGIN, g_cameraPresetCount, g_cameraPresetStrs);
IntVar g_cameraHitCur("Application/Camera Hit #", 1, 1, g_hitCount, 1);

struct RaytracingDispatchRayInputs
{
    RaytracingDispatchRayInputs() {}
    RaytracingDispatchRayInputs(
        ID3D12RaytracingFallbackDevice &device,
        ID3D12RaytracingFallbackStateObject *pPSO,
        void *pHitGroupShaderTable,
        UINT HitGroupStride,
        UINT HitGroupTableSize,
        LPCWSTR rayGenExportName,
        LPCWSTR missExportName) : _pPSO(pPSO)
    {
        const UINT shaderTableSize = device.GetShaderIdentifierSize();
        void *pRayGenShaderData = pPSO->GetShaderIdentifier(rayGenExportName);
        void *pMissShaderData = pPSO->GetShaderIdentifier(missExportName);

        _hitGroupStride = HitGroupStride;

        // MiniEngine requires that all initial data be aligned to 16 bytes
        UINT alignment = 16;
        vector<BYTE> alignedShaderTableData(shaderTableSize + alignment - 1);
        BYTE *pAlignedShaderTableData = alignedShaderTableData.data() + ((UINT64)alignedShaderTableData.data() % alignment);
        memcpy(pAlignedShaderTableData, pRayGenShaderData, shaderTableSize);
        _rayGenShaderTable.Create(L"Ray Gen Shader Table", 1, shaderTableSize, alignedShaderTableData.data());
        
        memcpy(pAlignedShaderTableData, pMissShaderData, shaderTableSize);
        _missShaderTable.Create(L"Miss Shader Table", 1, shaderTableSize, alignedShaderTableData.data());
        
        _hitShaderTable.Create(L"Hit Shader Table", 1, HitGroupTableSize, pHitGroupShaderTable);
        _hitShaderTableRead.Create(L"Hit Shader Table Read", 1, HitGroupTableSize);
    }

    D3D12_DISPATCH_RAYS_DESC GetDispatchRayDesc(UINT DispatchWidth, UINT DispatchHeight)
    {
        D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};

        dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = _rayGenShaderTable.GetGpuVirtualAddress();
        dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = _rayGenShaderTable.GetBufferSize();
        dispatchRaysDesc.HitGroupTable.StartAddress = _hitShaderTable.GetGpuVirtualAddress();
        dispatchRaysDesc.HitGroupTable.SizeInBytes = _hitShaderTable.GetBufferSize();
        dispatchRaysDesc.HitGroupTable.StrideInBytes = _hitGroupStride;
        dispatchRaysDesc.MissShaderTable.StartAddress = _missShaderTable.GetGpuVirtualAddress();
        dispatchRaysDesc.MissShaderTable.SizeInBytes = _missShaderTable.GetBufferSize();
        dispatchRaysDesc.MissShaderTable.StrideInBytes = dispatchRaysDesc.MissShaderTable.SizeInBytes; // Only one entry
        dispatchRaysDesc.Width = DispatchWidth;
        dispatchRaysDesc.Height = DispatchHeight;
        dispatchRaysDesc.Depth = 1;
        return dispatchRaysDesc;
    }

    UINT _hitGroupStride;
    CComPtr<ID3D12RaytracingFallbackStateObject> _pPSO;
    ByteAddressBuffer   _rayGenShaderTable;
    ByteAddressBuffer   _missShaderTable;
    ByteAddressBuffer   _hitShaderTable;
    ReadbackBuffer      _hitShaderTableRead;
};

enum RaytracingTypes
{
    Diffuse = 0,
    NumTypes
};
RaytracingDispatchRayInputs g_RaytracingInputs[RaytracingTypes::NumTypes];
byte* g_hitShaderTable = nullptr;
UINT g_offsetToMaterialConstants;
UINT g_shaderRecordSizeInBytes;

namespace GameCore
{
    bool UpdateApplication(IGameApp& app);
    void TerminateApplication(IGameApp& app);
    extern bool g_computeOnly;
}

namespace Graphics
{
    extern int g_gpuSelect;
    extern int g_skipPresent;
    extern EnumVar DebugZoom;
}

namespace EngineTuning
{
    extern bool sm_IsVisible;
    void StartSave(void*);
    void StartLoad(void*);
    extern std::wstring g_settingsPath;
}

namespace EngineProfiling
{
    extern BoolVar DrawFrameRate;
    extern BoolVar DrawProfiler;
}

namespace TextRenderer
{
    extern Color g_color;
    extern Color g_hiColor;
}

bool comparei(string& str, string &str2)
{
    return str.size() == str2.size() && equal(str.begin(), str.end(), str2.begin(),
        [](char& c1, char& c2)
        {
            return c1 == c2 || toupper(c1) == toupper(c2);
        });
}

bool comparei(wstring& str, wstring &str2)
{
    return str.size() == str2.size() && equal(str.begin(), str.end(), str2.begin(),
        [](WCHAR& c1, WCHAR& c2)
        {
            return c1 == c2 || toupper(c1) == toupper(c2);
        });
}

RaycoinViewer RaycoinViewer::inst;

void RaycoinViewer::parseCommandLineArgs(const WCHAR* argv[] , int argc)
{
    int consumed;
    for (int argi = 1; argi < argc; argi+=consumed)
    {
        consumed = 1;
        if (argi+1 < argc && comparei(wstring(argv[argi]), wstring(L"-gpu")))
        {
            wstringstream s(argv[argi+1]);
            s >> Graphics::g_gpuSelect;
            consumed = 2;
        }
        else if (argi+1 < argc && comparei(wstring(argv[argi]), wstring(L"-datadir")))
        {
            EngineTuning::g_settingsPath = argv[argi+1];
            consumed = 2;
        }
    }
}

int wmain(int argc, const WCHAR* argv[])
{
    WCHAR defaultPath[MAX_PATH] = L"";
    if (SHGetSpecialFolderPathW(nullptr, defaultPath, CSIDL_APPDATA, true))
        EngineTuning::g_settingsPath = wstring(defaultPath) + L"\\" + L"Raycoin";

    RaycoinViewer::inst.parseCommandLineArgs(argv, argc);

    WCHAR absPath[MAX_PATH] = L"";
    if (GetFullPathNameW(EngineTuning::g_settingsPath.c_str(), MAX_PATH, absPath, nullptr))
        EngineTuning::g_settingsPath = absPath;
    SHCreateDirectoryExW(nullptr, EngineTuning::g_settingsPath.c_str(), nullptr);

    RaycoinViewer::inst.start();
    return 0;
}

void RaycoinViewer::start()
{
    TargetResolution = kCustom;
    g_NativeWidthCustom = g_screenDim;
    g_NativeHeightCustom = g_screenDim;
    g_DisplayWidth = g_screenDim;
    g_DisplayHeight = g_screenDim;
    s_EnableVSync = false;
#ifdef COMPUTE_ONLY
    GameCore::g_computeOnly = true;
#endif
    GameCore::RunApplication(*this, L"Raycoin Viewer " RAYCOIN_VERSION_STRING);
}

#ifdef COMPUTE_ONLY
void RaycoinViewer::update()
{
    GameCore::UpdateApplication(*this);
}

void RaycoinViewer::terminate()
{
    GameCore::TerminateApplication(*this);
}
#endif

class DescriptorHeapStack
{
public:
    DescriptorHeapStack(ID3D12Device &device, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT NodeMask) :
        m_device(device)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = numDescriptors;
        desc.Type = type;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.NodeMask = NodeMask;
        device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap));

        m_descriptorSize = device.GetDescriptorHandleIncrementSize(type);
        m_descriptorHeapCpuBase = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        reset();
    }

    ID3D12DescriptorHeap &GetDescriptorHeap() { return *m_pDescriptorHeap; }

    void AllocateDescriptor(_Out_ D3D12_CPU_DESCRIPTOR_HANDLE &cpuHandle, _Out_ UINT &descriptorHeapIndex)
    {
        descriptorHeapIndex = m_descriptorsAllocated;
        cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptorHeapCpuBase, descriptorHeapIndex, m_descriptorSize);
        m_descriptorsAllocated++;
    }

    UINT AllocateBufferSrv(_In_ ID3D12Resource &resource)
    {
        UINT descriptorHeapIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        AllocateDescriptor(cpuHandle, descriptorHeapIndex);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = (UINT)(resource.GetDesc().Width / sizeof(UINT32));
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        m_device.CreateShaderResourceView(&resource, &srvDesc, cpuHandle);
        return descriptorHeapIndex;
    }

    UINT AllocateBufferUav(_In_ ID3D12Resource &resource)
    {
        UINT descriptorHeapIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        AllocateDescriptor(cpuHandle, descriptorHeapIndex);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = (UINT)(resource.GetDesc().Width / sizeof(UINT32));
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;

        m_device.CreateUnorderedAccessView(&resource, nullptr, &uavDesc, cpuHandle);
        return descriptorHeapIndex;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(UINT descriptorIndex)
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, m_descriptorSize);
    }

    void reset() { m_descriptorsAllocated = 0; }

private:
    ID3D12Device & m_device;
    CComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
    UINT m_descriptorsAllocated;
    UINT m_descriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapCpuBase;
};

unique_ptr<DescriptorHeapStack> g_pRaytracingDescriptorHeap;

static void InitializeViews()
{
#ifndef COMPUTE_ONLY
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle;
    UINT uavDescriptorIndex;
    g_pRaytracingDescriptorHeap->AllocateDescriptor(uavHandle, uavDescriptorIndex);
    Graphics::g_Device->CopyDescriptorsSimple(1, uavHandle, g_SceneColorBuffer.GetUAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_OutputUAV = g_pRaytracingDescriptorHeap->GetGpuHandle(uavDescriptorIndex);
#endif
}

D3D12_STATE_SUBOBJECT CreateDxilLibrary(LPCWSTR entrypoint, const void *pShaderByteCode, SIZE_T bytecodeLength, D3D12_DXIL_LIBRARY_DESC &dxilLibDesc, D3D12_EXPORT_DESC &exportDesc)
{
    exportDesc = { entrypoint, nullptr, D3D12_EXPORT_FLAG_NONE };
    D3D12_STATE_SUBOBJECT dxilLibSubObject = {};
    dxilLibDesc.DXILLibrary.pShaderBytecode = pShaderByteCode;
    dxilLibDesc.DXILLibrary.BytecodeLength = bytecodeLength;
    dxilLibDesc.NumExports = 1;
    dxilLibDesc.pExports = &exportDesc;
    dxilLibSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    dxilLibSubObject.pDesc = &dxilLibDesc;
    return dxilLibSubObject;
}

void SetPipelineStateStackSize(LPCWSTR raygen, LPCWSTR closestHit, LPCWSTR miss, UINT maxRecursion, ID3D12RaytracingFallbackStateObject *pStateObject)
{
    UINT64 closestHitStackSize = pStateObject->GetShaderStackSize(closestHit);
    UINT64 missStackSize = pStateObject->GetShaderStackSize(miss);
    UINT64 raygenStackSize = pStateObject->GetShaderStackSize(raygen);

    UINT64 totalStackSize = raygenStackSize + max(missStackSize, closestHitStackSize) * maxRecursion;
    pStateObject->SetPipelineStackSize(totalStackSize);
}

void InitializeRaytracingStateObjects()
{
    D3D12_DESCRIPTOR_RANGE1 uavDescriptorRange = {};
    uavDescriptorRange.BaseShaderRegister = 0;
    uavDescriptorRange.NumDescriptors = 1;
    uavDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavDescriptorRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

    CD3DX12_ROOT_PARAMETER1 globalRootSignatureParameters[6];
    globalRootSignatureParameters[0].InitAsDescriptorTable(1, &uavDescriptorRange);
    globalRootSignatureParameters[1].InitAsShaderResourceView(0);
    globalRootSignatureParameters[2].InitAsConstantBufferView(0);
    globalRootSignatureParameters[3].InitAsShaderResourceView(1);
    globalRootSignatureParameters[4].InitAsShaderResourceView(2);
    globalRootSignatureParameters[5].InitAsUnorderedAccessView(1);
    
    auto globalRootSignatureDesc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(ARRAYSIZE(globalRootSignatureParameters), globalRootSignatureParameters, 0, nullptr);

    CComPtr<ID3DBlob> pGlobalRootSignatureBlob;
    CComPtr<ID3DBlob> pErrorBlob;
    if (FAILED(g_pRaytracingDevice->D3D12SerializeVersionedRootSignature(&globalRootSignatureDesc, &pGlobalRootSignatureBlob, &pErrorBlob)))
    {
        OutputDebugStringA((LPCSTR)pErrorBlob->GetBufferPointer());
    }
    g_pRaytracingDevice->CreateRootSignature(0, pGlobalRootSignatureBlob->GetBufferPointer(), pGlobalRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_GlobalRaytracingRootSignature));

    CD3DX12_ROOT_PARAMETER1 localRootSignatureParameters[1];
    UINT sizeOfRootConstantInDwords = (sizeof(MaterialRootConstants) - 1) / sizeof(DWORD) + 1;
    localRootSignatureParameters[0].InitAsConstants(sizeOfRootConstantInDwords, 1);
    auto localRootSignatureDesc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(ARRAYSIZE(localRootSignatureParameters), localRootSignatureParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    CComPtr<ID3DBlob> pLocalRootSignatureBlob;
    g_pRaytracingDevice->D3D12SerializeVersionedRootSignature(&localRootSignatureDesc, &pLocalRootSignatureBlob, nullptr);
    g_pRaytracingDevice->CreateRootSignature(0, pLocalRootSignatureBlob->GetBufferPointer(), pLocalRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_LocalRaytracingRootSignature));

    vector<D3D12_STATE_SUBOBJECT> subObjects;
    D3D12_STATE_SUBOBJECT nodeMaskSubObject;
    UINT nodeMask = 1;
    nodeMaskSubObject.pDesc = &nodeMask;
    nodeMaskSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK;
    subObjects.push_back(nodeMaskSubObject);

    D3D12_STATE_SUBOBJECT rootSignatureSubObject;
    rootSignatureSubObject.pDesc = &g_GlobalRaytracingRootSignature.p;
    rootSignatureSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subObjects.push_back(rootSignatureSubObject);

    D3D12_STATE_SUBOBJECT configurationSubObject;
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;
    pipelineConfig.MaxTraceRecursionDepth = g_maxRayRecursion;
    configurationSubObject.pDesc = &pipelineConfig;
    configurationSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subObjects.push_back(configurationSubObject);

    // Ray Gen shader stuff
    // ----------------------------------------------------------------//
    LPCWSTR rayGenShaderExportName = L"RayGen";
    D3D12_EXPORT_DESC rayGenExportDesc;
    D3D12_DXIL_LIBRARY_DESC rayGenLibDesc = {};
    D3D12_STATE_SUBOBJECT rayGenLibSubobject = CreateDxilLibrary(rayGenShaderExportName, g_pRayGenerationShader, sizeof(g_pRayGenerationShader), rayGenLibDesc, rayGenExportDesc);
    subObjects.push_back(rayGenLibSubobject);

    // Hit Group shader stuff
    // ----------------------------------------------------------------//
#ifndef COMPUTE_ONLY
    const int hitGroupCount = 2;
#else
    const int hitGroupCount = 1;
#endif
    vector<LPCWSTR> hitGroupExportNames(hitGroupCount);
    vector<LPCWSTR> closestHitExportNames(hitGroupCount);
    vector<pair<const unsigned char*, SIZE_T>> hitShaders(hitGroupCount);
    vector<D3D12_EXPORT_DESC> hitGroupExportDescs(hitGroupCount);
    vector<D3D12_DXIL_LIBRARY_DESC> hitGroupLibDescs(hitGroupCount);
    vector<D3D12_HIT_GROUP_DESC> hitGroupDescs(hitGroupCount);

    hitGroupExportNames[0] = L"HitGroup";
    closestHitExportNames[0] = L"Hit";
    hitShaders[0] = make_pair(g_phitShader, sizeof(g_phitShader));

#ifndef COMPUTE_ONLY
    hitGroupExportNames[1] = L"LineHitGroup";
    closestHitExportNames[1] = L"LineHit";
    hitShaders[1] = make_pair(g_plineHitShader, sizeof(g_plineHitShader));
#endif

    for (int i = 0; i < hitGroupCount; ++i)
    {
        D3D12_STATE_SUBOBJECT hitGroupLibSubobject = CreateDxilLibrary(closestHitExportNames[i], hitShaders[i].first, hitShaders[i].second, hitGroupLibDescs[i], hitGroupExportDescs[i]);
        subObjects.push_back(hitGroupLibSubobject);

        hitGroupDescs[i].ClosestHitShaderImport = closestHitExportNames[i];
        hitGroupDescs[i].HitGroupExport = hitGroupExportNames[i];
        D3D12_STATE_SUBOBJECT hitGroupSubobject = {};
        hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobject.pDesc = &hitGroupDescs[i];
        subObjects.push_back(hitGroupSubobject);
    }

    LPCWSTR missExportName = L"Miss";
    D3D12_EXPORT_DESC missExportDesc;
    D3D12_DXIL_LIBRARY_DESC missLibDesc = {};
    D3D12_STATE_SUBOBJECT missLibSubobject = CreateDxilLibrary(missExportName, g_pmissShader, sizeof(g_pmissShader), missLibDesc, missExportDesc);
    subObjects.push_back(missLibSubobject);

    D3D12_STATE_SUBOBJECT shaderConfigStateObject;
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
    shaderConfig.MaxAttributeSizeInBytes = 8;
#ifndef COMPUTE_ONLY
    shaderConfig.MaxPayloadSizeInBytes = 64;
#else
    shaderConfig.MaxPayloadSizeInBytes = 32;
#endif
    shaderConfigStateObject.pDesc = &shaderConfig;
    shaderConfigStateObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subObjects.push_back(shaderConfigStateObject);

    D3D12_STATE_SUBOBJECT localRootSignatureSubObject;
    localRootSignatureSubObject.pDesc = &g_LocalRaytracingRootSignature.p;
    localRootSignatureSubObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    subObjects.push_back(localRootSignatureSubObject);

    D3D12_STATE_OBJECT_DESC stateObject;
    stateObject.NumSubobjects = (UINT)subObjects.size();
    stateObject.pSubobjects = subObjects.data();
    stateObject.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

    const UINT shaderIdentifierSize = g_pRaytracingDevice->GetShaderIdentifierSize();
#define ALIGN(alignment, num) ((((num) + alignment - 1) / alignment) * alignment)
    g_offsetToMaterialConstants = ALIGN(sizeof(UINT32), shaderIdentifierSize);
    g_shaderRecordSizeInBytes = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, g_offsetToMaterialConstants + sizeof(MaterialRootConstants));
    
#ifndef COMPUTE_ONLY
    bool hasLine = true;
#else
    bool hasLine = false;
#endif
    vector<byte> pHitShaderTable(g_shaderRecordSizeInBytes * (g_sphereCount + hasLine));
    auto GetShaderTable = [=](ID3D12RaytracingFallbackStateObject *pPSO, byte *pShaderTable)
    {
        void *pHitGroupIdentifierData = pPSO->GetShaderIdentifier(hitGroupExportNames[0]);
        for (int i = 0; i < g_sphereCount; ++i)
        {
            byte *pShaderRecord = i * g_shaderRecordSizeInBytes + pShaderTable;
            memcpy(pShaderRecord, pHitGroupIdentifierData, shaderIdentifierSize);
            
#ifndef COMPUTE_ONLY
            MaterialRootConstants material = {{1,1,1}, (UINT)i};
#else
            MaterialRootConstants material = {(UINT)i};
#endif
            memcpy(pShaderRecord + g_offsetToMaterialConstants, &material, sizeof(material));
        }

#ifndef COMPUTE_ONLY
        {
            int lineIdx = g_sphereCount;
            byte *pShaderRecord = lineIdx * g_shaderRecordSizeInBytes + pShaderTable;
            memcpy(pShaderRecord, pPSO->GetShaderIdentifier(hitGroupExportNames[1]), shaderIdentifierSize);

            MaterialRootConstants material = {{1,1,1}, (UINT)lineIdx};
            memcpy(pShaderRecord + g_offsetToMaterialConstants, &material, sizeof(material));
        }
#endif
    };

    {
        CComPtr<ID3D12RaytracingFallbackStateObject> pPSO;
        g_pRaytracingDevice->CreateStateObject(&stateObject, IID_PPV_ARGS(&pPSO));
        GetShaderTable(pPSO, pHitShaderTable.data());
        g_RaytracingInputs[Diffuse] = RaytracingDispatchRayInputs(*g_pRaytracingDevice, pPSO, pHitShaderTable.data(), g_shaderRecordSizeInBytes, (UINT)pHitShaderTable.size(), rayGenShaderExportName, missExportName);
    }

   for (auto &raytracingPipelineState : g_RaytracingInputs)
   {
       SetPipelineStateStackSize(rayGenShaderExportName, (wstring(hitGroupExportNames[0]) + L"::closesthit").c_str(), missExportName, g_maxRayRecursion, raytracingPipelineState._pPSO);
   }
}

typedef Vector3 Vertex;
typedef UINT16 Index;

struct Triangle
{
    Index v[3];
};

using TriangleList = vector<Triangle>;
using VertexList = vector<Vertex>;

namespace icosahedron
{
    const float X = .525731112119133606f;
    const float Z = .850650808352039932f;
    const float N = 0.f;

    static const VertexList vertices =
    {
        {-X,N,Z}, {X,N,Z}, {-X,N,-Z}, {X,N,-Z},
        {N,Z,X}, {N,Z,-X}, {N,-Z,X}, {N,-Z,-X},
        {Z,X,N}, {-Z,X,N}, {Z,-X,N}, {-Z,-X,N}
    };

    static const TriangleList triangles =
    {
        {0,1,4},{0,4,9},{9,4,5},{4,8,5},{4,1,8},
        {8,1,10},{8,10,3},{5,8,3},{5,3,2},{2,3,7},
        {7,3,10},{7,10,6},{7,6,11},{11,6,0},{0,6,1},
        {6,10,1},{9,11,0},{9,2,11},{9,5,2},{7,11,2}
    };
}

using Lookup = unordered_map<UINT32, Index>;

Index vertex_for_edge(Lookup& lookup, VertexList& vertices, Index first, Index second)
{
    Lookup::key_type key = first <= second ? (first << 16) | second : (second << 16) | first;
    auto inserted = lookup.insert({ key, (Index)vertices.size() });
    if (inserted.second)
    {
        auto& edge0 = vertices[first];
        auto& edge1 = vertices[second];
        auto point = Normalize(edge0 + edge1);
        vertices.push_back(point);
    }

    return inserted.first->second;
}

TriangleList subdivide(VertexList& vertices, TriangleList triangles)
{
    Lookup lookup;
    TriangleList result;

    for (auto&& each : triangles)
    {
        array<Index, 3> mid;
        for (int edge = 0; edge < 3; ++edge)
        {
            mid[edge] = vertex_for_edge(lookup, vertices,
                each.v[edge], each.v[(edge + 1) % 3]);
        }

        result.push_back({ each.v[0], mid[0], mid[2] });
        result.push_back({ each.v[1], mid[1], mid[0] });
        result.push_back({ each.v[2], mid[2], mid[1] });
        result.push_back({ mid[0], mid[1], mid[2] });
    }

    return result;
}

using IndexedMesh = pair<VertexList, TriangleList>;

IndexedMesh make_icosphere(int subdivisions)
{
    VertexList vertices = icosahedron::vertices;
    TriangleList triangles = icosahedron::triangles;

    for (int i = 0; i < subdivisions; ++i)
    {
        triangles = subdivide(vertices, triangles);
    }

    return{ vertices, triangles };
}

IndexedMesh make_cylinder(float height, float bottomRadius, float topRadius, int nbSides)
{
    int nbVerticesCap = nbSides + 1;

    // bottom + top + sides
    VertexList vertices(nbVerticesCap + nbVerticesCap + nbSides * 2 + 2);
    int vert = 0;
    float _2pi = XM_PI * 2;

    // Bottom cap
    vertices[vert++] = Vector3(0, 0, 0);
    while( vert <= nbSides )
    {
        float rad = (float)vert / nbSides * _2pi;
        vertices[vert] = Vector3(cos(rad) * bottomRadius, 0, sin(rad) * bottomRadius);
        vert++;
    }

    // Top cap
    vertices[vert++] = Vector3(0, height, 0);
    while (vert <= nbSides * 2 + 1)
    {
        float rad = (float)(vert - nbSides - 1)  / nbSides * _2pi;
        vertices[vert] = Vector3(cos(rad) * topRadius, height, sin(rad) * topRadius);
        vert++;
    }

    // Sides
    int v = 0;
    while (vert <= vertices.size() - 4 )
    {
        float rad = (float)v / nbSides * _2pi;
        vertices[vert] = Vector3(cos(rad) * topRadius, height, sin(rad) * topRadius);
        vertices[vert + 1] = Vector3(cos(rad) * bottomRadius, 0, sin(rad) * bottomRadius);
        vert+=2;
        v++;
    }
    vertices[vert] = vertices[ nbSides * 2 + 2 ];
    vertices[vert + 1] = vertices[nbSides * 2 + 3 ];

    int nbTriangles = nbSides + nbSides + nbSides*2;
    TriangleList triangles(nbTriangles + 1);

    // Bottom cap
    int tri = 0;
    int i = 0;
    while (tri < nbSides - 1)
    {
        triangles[i].v[0] = 0;
        triangles[i].v[1] = tri + 1;
        triangles[i].v[2] = tri + 2;
        tri++;
        i++;
    }
    triangles[i].v[0] = 0;
    triangles[i].v[1] = tri + 1;
    triangles[i].v[2] = 1;
    tri++;
    i++;

    // Top cap
    //tri++;
    while (tri < nbSides*2)
    {
        triangles[i].v[0] = tri + 2;
        triangles[i].v[1] = tri + 1;
        triangles[i].v[2] = nbVerticesCap;
        tri++;
        i++;
    }

    triangles[i].v[0] = nbVerticesCap + 1;
    triangles[i].v[1] = tri + 1;
    triangles[i].v[2] = nbVerticesCap;		
    tri++;
    i++;
    tri++;

    // Sides
    while( tri <= nbTriangles )
    {
        triangles[i].v[0] = tri + 2;
        triangles[i].v[1] = tri + 1;
        triangles[i].v[2] = tri + 0;
        tri++;
        i++;

        triangles[i].v[0] = tri + 1;
        triangles[i].v[1] = tri + 2;
        triangles[i].v[2] = tri + 0;
        tri++;
        i++;
    }

    return make_pair(vertices, triangles);
}

static const int g_lineMeshLOD = 16;
IndexedMesh g_lineMesh;
IndexedMesh g_arrowHeadMesh;

bool isNearZero(float val, float tol = 1e-06f)  { return abs(val) <= tol; }

bool isInRange(float val, float min, float max) { return val >= min && val <= max; }
bool isInRange(const Vector3& v, const Vector3& min, const Vector3& max)
{
    return isInRange((float)v[0], min[0], max[0]) && isInRange((float)v[1], min[1], max[1]) && isInRange((float)v[2], min[2], max[2]);
}

float clamp(float val, float min, float max)    { return val < min ? min : val > max ? max : val; }
Vector3 clamp(const Vector3& v, const Vector3& min, const Vector3& max)
{
    return {clamp((float)v[0], min[0], max[0]), clamp((float)v[1], min[1], max[1]), clamp((float)v[2], min[2], max[2])};
}

Quaternion quatFromAlign(const Vector3& v1, const Vector3& v2)
{
    Vector3 q;
    Vector3 bisector = Normalize(v1 + v2);
    float cosHalfAngle = Dot(v1, bisector);
    float w = cosHalfAngle;

    if (!isNearZero(cosHalfAngle))
    {
        q = Cross(v1, bisector);
    }
    else
    {
        //If v1 is zero then there is no rotation
        if (isNearZero(LengthSquare(v1)))
            return Quaternion();

        float invLength;
        if (abs(v1[0]) >= abs(v1[1]))
        {
            // V1.x or V1.z is the largest magnitude component.
            invLength = 1 / sqrt(v1[0]*v1[0] + v1[2]*v1[2]);
            q = {-v1[2]*invLength, 0, v1[0]*invLength};
        }
        else
        {
            // V1.y or V1.z is the largest magnitude component.
            invLength = 1 / sqrt(v1[1]*v1[1] + v1[2]*v1[2]);
            q = {0, v1[2]*invLength, -v1[1]*invLength};
        }
    }

    return Quaternion(Vector4(q, w));
}

struct Plane    { Vector3 normal; float dist; };
struct Ray      { Vector3 origin, dir; };

pair<bool, Vector3> intersect(const Plane& plane, const Ray& ray)
{
    float dot = Dot(ray.dir, plane.normal);

    // ray is parallel to plane?
    if(isNearZero(dot))
    {
        // ray lies in plane?
        float distance = Dot(plane.normal, ray.origin) - plane.dist;
        if (isNearZero(distance))
        {
            return make_pair(true, ray.origin);
        }
        return make_pair(false, Vector3());
    }

    //distance from ray to intersection point
    float t = ( plane.dist - Dot(ray.origin, plane.normal) ) / dot;

    if (t < 0) return make_pair(false, Vector3()); //Intersection behind ray
    return make_pair(true, ray.origin + ray.dir*t);
}

bool RaycoinViewer::hasAdapter() const { return g_Device; }

void RaycoinViewer::Startup( void )
{
    if (!hasAdapter()) return;

    D3D12CreateRaytracingFallbackDevice(g_Device, CreateRaytracingFallbackDeviceFlags::None, 0, IID_PPV_ARGS(&g_pRaytracingDevice));

    g_pRaytracingDescriptorHeap = make_unique<DescriptorHeapStack>(*g_Device, 1024, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 0);

    auto mesh = make_icosphere(g_sphereLOD);
    vector<Attrib> attribs;
    attribs.reserve(mesh.second.size());
    for (auto& tri : mesh.second)
    {
        auto v = Normalize(Cross(mesh.first[tri.v[1]] - mesh.first[tri.v[0]], mesh.first[tri.v[2]] - mesh.first[tri.v[0]]));
        attribs.push_back({{v[0], v[1], v[2]}});
    }
    g_vertexBuffer.Create(L"Vertex Buffer", (int)mesh.first.size(), sizeof(Vertex), mesh.first.data());
    g_indexBuffer.Create(L"Index Buffer", (int)mesh.second.size(), sizeof(Triangle), mesh.second.data());
    g_attribBuffer.Create(L"Attributes Buffer", (int)attribs.size(), sizeof(Attrib), attribs.data());

#ifndef COMPUTE_ONLY
    g_lineMesh = make_cylinder(1,1,1,g_lineMeshLOD);
    g_arrowHeadMesh = make_cylinder(1,1,0,g_lineMeshLOD);
#endif
    g_hitShaderConstants.resolution = { g_screenDim, g_screenDim };
    g_hitShaderConstantBuffer.Create(L"Hit Constant Buffer", 1, sizeof(HitShaderConstants));
    g_initFieldConstantBuffer.Create(L"Init Field Constant Buffer", 1, sizeof(InitFieldConstants));

    g_traceResultBuffer.Create(L"Trace Result Buffer", 2, sizeof(::TraceResult));
    g_traceResultRead.Create(L"Trace Result Read", 2, sizeof(::TraceResult));

    _rootSig.Reset(4); 
    _rootSig[0].InitAsConstantBuffer(0);
    _rootSig[1].InitAsBufferSRV(0);
    _rootSig[2].InitAsBufferUAV(0);
    _rootSig[3].InitAsBufferUAV(1);
    _rootSig.Finalize(L"RaycoinViewer", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    _initFieldPSO.SetRootSignature(_rootSig);
    _initFieldPSO.SetComputeShader(g_pinitFieldCS, sizeof(g_pinitFieldCS));
    _initFieldPSO.Finalize();

    buildAccelStruct();
    InitializeRaytracingStateObjects();

    _camera.SetPerspectiveMatrix(g_fov * XM_PI / 180, 1, g_zRange[0], g_zRange[1]);
    _camera.SetEyeAtUp(Vector3(kZero), Vector3(kZero), Vector3(kYUnitVector));
    s_cameraPresets[CAMERAPRESET_ORIGIN] = { Vector3(kZero), 0.f, 0.f };
    s_cameraPresets[CAMERAPRESET_RIGHT] = { Vector3(g_sphereDim/2 * 4 * 1.5f, 0, 0), XM_PIDIV2, 0 };
    s_cameraPresets[CAMERAPRESET_TOP] = { Vector3(0, s_cameraPresets[CAMERAPRESET_RIGHT].pos[0], 0), 0, -XM_PIDIV2 };
    s_cameraPresets[CAMERAPRESET_BEHIND] = { Vector3(0, 0, s_cameraPresets[CAMERAPRESET_RIGHT].pos[0]+2), 0, 0 };
    s_cameraPresets[CAMERAPRESET_CORNER] =
    {
        Vector3(s_cameraPresets[CAMERAPRESET_RIGHT].pos[0], s_cameraPresets[CAMERAPRESET_TOP].pos[1], s_cameraPresets[CAMERAPRESET_BEHIND].pos[2]) / sqrt(2.f),
        XM_PIDIV4, -XM_PIDIV4
    };
    s_cameraPresets[CAMERAPRESET_HIT] = s_cameraPresets[CAMERAPRESET_ORIGIN];
    _cameraController.reset(new CameraController(_camera, Vector3(kYUnitVector)));
#ifndef RELEASE
    _cameraController->SlowRotation(true);
#endif
    _cameraPresetLast = -1;
    _cameraPresetSave = {};
    setCameraPreset(s_cameraPresets[CAMERAPRESET_ORIGIN]);
#ifdef COMPUTE_ONLY
    auto mat = Transpose(Invert(_camera.GetViewProjMatrix()));
    memcpy(&g_hitShaderConstants.cameraToWorld, &mat, sizeof(g_hitShaderConstants.cameraToWorld));
    g_hitShaderConstants.worldCameraPosition = _camera.GetPosition();
#endif

    _result = {};
    resetBestHash();
    g_traceResult = {};
    _bestHash = true;
    _blockHeight = 0;
    _blockNonce = 0;
    _blockExtraNonce = 0;
    _seed = {};
    srand((UINT)time(0));
    _target = {};
    _verifyPos = {-1,-1};
    _tracePerSec = 0;
    _modeLast = -1;
    _traceLogCurLast = 0;
    _revertBestHash = false;
    _resetAccelStruct = false;
#ifndef COMPUTE_ONLY
    EngineTuning::StartLoad(nullptr);
#endif
    _cameraHitLast = g_cameraHitCur;
    _vsyncSave = s_EnableVSync;

#ifndef COMPUTE_ONLY
    loadTraceLog();
    g_traceLogCur = (int)_traceLog.size();
    if (!g_traceLogCur) g_mode = MODE_COMPUTE;
#endif
}

void RaycoinViewer::buildAccelStruct()
{
    g_pRaytracingDescriptorHeap->reset();
    InitializeViews();

#ifndef COMPUTE_ONLY
    bool hasLine = g_lineIndexBuffer.GetGpuVirtualAddress();
#else
    bool hasLine = false;
#endif
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelAccelDesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs = topLevelAccelDesc.Inputs;
    topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    topLevelInputs.NumDescs = g_sphereCount+hasLine;
    topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    g_pRaytracingDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);

    g_topLevelAccel.Create(L"TopLevelAccelerationStructure", (int)topLevelPrebuildInfo.ResultDataMaxSizeInBytes, 1, nullptr, g_pRaytracingDevice->GetAccelerationStructureResourceState());
    g_topLevelAccelPointer = g_pRaytracingDevice->GetWrappedPointerSimple(
        g_pRaytracingDescriptorHeap->AllocateBufferUav(*g_topLevelAccel.GetResource()),
        g_topLevelAccel->GetGPUVirtualAddress());

    UINT64 scratchBufferSizeNeeded = topLevelPrebuildInfo.ScratchDataSizeInBytes;

    vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs(2);
    geometryDescs[0].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometryDescs[0].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geometryDescs[0].Triangles.IndexBuffer = g_indexBuffer.GetGpuVirtualAddress();
    geometryDescs[0].Triangles.IndexCount = (UINT)g_indexBuffer->GetDesc().Width / sizeof(Index);
    geometryDescs[0].Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
    geometryDescs[0].Triangles.Transform3x4 = 0;
    geometryDescs[0].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometryDescs[0].Triangles.VertexCount = (UINT)g_vertexBuffer->GetDesc().Width / sizeof(Vertex);
    geometryDescs[0].Triangles.VertexBuffer.StartAddress = g_vertexBuffer.GetGpuVirtualAddress();
    geometryDescs[0].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

    if (hasLine)
    {
        geometryDescs[1].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDescs[1].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geometryDescs[1].Triangles.IndexBuffer = g_lineIndexBuffer.GetGpuVirtualAddress();
        geometryDescs[1].Triangles.IndexCount = g_lineIndexBuffer.GetGpuVirtualAddress() ? (UINT)g_lineIndexBuffer->GetDesc().Width / sizeof(Index) : 0;
        geometryDescs[1].Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDescs[1].Triangles.Transform3x4 = 0;
        geometryDescs[1].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDescs[1].Triangles.VertexCount = g_lineVertexBuffer.GetGpuVirtualAddress() ? (UINT)g_lineVertexBuffer->GetDesc().Width / sizeof(Vertex) : 0;
        geometryDescs[1].Triangles.VertexBuffer.StartAddress = g_lineVertexBuffer.GetGpuVirtualAddress();
        geometryDescs[1].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    }

    const int numBottomLevels = 1+hasLine;
    vector<UINT64> bottomLevelAccelSize(numBottomLevels);
    vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> bottomLevelAccelDescs(numBottomLevels);
    for (int i = 0; i < numBottomLevels; ++i)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC &bottomLevelAccelDesc = bottomLevelAccelDescs[i];
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &bottomLevelInputs = bottomLevelAccelDesc.Inputs;
        bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelInputs.NumDescs = 1;
        bottomLevelInputs.pGeometryDescs = &geometryDescs[i];
        bottomLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelprebuildInfo;
        g_pRaytracingDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelprebuildInfo);

        bottomLevelAccelSize[i] = bottomLevelprebuildInfo.ResultDataMaxSizeInBytes;
        scratchBufferSizeNeeded = max(bottomLevelprebuildInfo.ScratchDataSizeInBytes, scratchBufferSizeNeeded);
    }

    g_scratchBuffer.Create(L"Acceleration Structure Scratch Buffer", (int)scratchBufferSizeNeeded, 1);

    topLevelAccelDesc.DestAccelerationStructureData = g_topLevelAccel->GetGPUVirtualAddress();
    topLevelAccelDesc.ScratchAccelerationStructureData = g_scratchBuffer.GetGpuVirtualAddress();

    g_bottomLevelAccels.clear();
    g_bottomLevelAccels.resize(numBottomLevels);
    vector<WRAPPED_GPU_POINTER> bottomLevelPtrs(numBottomLevels);
    for (int i = 0; i < bottomLevelAccelDescs.size(); ++i)
    {
        auto& bottomLevelAccel = g_bottomLevelAccels[i];
        bottomLevelAccel.Create(L"bottomLevelAccel", (int)bottomLevelAccelSize[i], 1, nullptr, g_pRaytracingDevice->GetAccelerationStructureResourceState());
        bottomLevelPtrs[i] = g_pRaytracingDevice->GetWrappedPointerSimple(
            g_pRaytracingDescriptorHeap->AllocateBufferUav(*bottomLevelAccel.GetResource()),
            bottomLevelAccel->GetGPUVirtualAddress());
        bottomLevelAccelDescs[i].DestAccelerationStructureData = bottomLevelAccel->GetGPUVirtualAddress();
        bottomLevelAccelDescs[i].ScratchAccelerationStructureData = g_scratchBuffer.GetGpuVirtualAddress();
    }

    vector<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC> instanceDescs(g_sphereCount+hasLine);
    for (int i = 0; i < g_sphereCount; ++i)
    {
        auto &instanceDesc = instanceDescs[i];
        if (g_instanceDescs)
            memcpy(instanceDesc.Transform, g_instanceDescs[i].Transform, sizeof(instanceDesc.Transform));
        else
        {
            memset(instanceDesc.Transform, 0, sizeof(instanceDesc.Transform));
            instanceDesc.Transform[0][0] = 1;
            instanceDesc.Transform[1][1] = 1;
            instanceDesc.Transform[2][2] = 1;
            instanceDesc.Transform[0][3] = (float)(i%g_sphereDim - g_sphereDim/2) * 4;
            instanceDesc.Transform[1][3] = (float)((i%(g_sphereDim*g_sphereDim))/g_sphereDim - g_sphereDim/2) * 4;
            instanceDesc.Transform[2][3] = (float)(i/(g_sphereDim*g_sphereDim) - g_sphereDim/2) * 4 + 2;
        }
        instanceDesc.AccelerationStructure = bottomLevelPtrs[0];
        instanceDesc.Flags = 0;
        instanceDesc.InstanceID = i;
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = i;
    }

    if (hasLine)
    {
        int lineIdx = g_sphereCount;
        auto& instanceDesc = instanceDescs[lineIdx];
        memset(instanceDesc.Transform, 0, sizeof(instanceDesc.Transform));
        instanceDesc.Transform[0][0] = 1;
        instanceDesc.Transform[1][1] = 1;
        instanceDesc.Transform[2][2] = 1;
        instanceDesc.AccelerationStructure = bottomLevelPtrs[1];
        instanceDesc.Flags = 0;
        instanceDesc.InstanceID = lineIdx;
        instanceDesc.InstanceMask = -g_instLine;
        instanceDesc.InstanceContributionToHitGroupIndex = lineIdx;
    }

    g_instanceDescsBuffer.Create(L"Instance Descs Buffer", (int)instanceDescs.size(), sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC), instanceDescs.data());
    topLevelInputs.InstanceDescs = g_instanceDescsBuffer.GetGpuVirtualAddress();
    if (!g_instanceDescs) g_instanceDescsRead.Create(L"Instance Descs Read", (int)instanceDescs.size(), sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC));

    auto& context = GraphicsContext::Begin();
    ID3D12GraphicsCommandList *pCommandList = context.GetCommandList();

    CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
    g_pRaytracingDevice->QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));

    ID3D12DescriptorHeap *descriptorHeaps[] = { &g_pRaytracingDescriptorHeap->GetDescriptorHeap() };
    pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

    for (int i = 0; i < bottomLevelAccelDescs.size(); ++i)
    {
        pRaytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelAccelDescs[i], 0, nullptr);
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(g_bottomLevelAccels[i].GetResource()));
    }
    pRaytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelAccelDesc, 0, nullptr);
    context.Finish(true);
}

void RaycoinViewer::Cleanup( void )
{
    if (!hasAdapter()) return;
#ifndef COMPUTE_ONLY
    EngineTuning::StartSave(nullptr);
#endif
}

void HandleDigitalButtonPress(GameInput::DigitalInput button, float timeDelta, function<void()> action);

void RaycoinViewer::Update(float deltaT)
{
#ifndef COMPUTE_ONLY
    ScopedTimer _prof(L"Update State");

    if (GameInput::IsFirstPressed(GameInput::kKey_1) ||
        (g_mode != MODE_COMPUTE && (GameInput::IsFirstPressed(GameInput::kKey_space) || GameInput::IsFirstPressed(GameInput::kXButton))))
    {
        g_mode = MODE_COMPUTE;
        g_traceLogCur = 0;
    }
    else if (GameInput::IsFirstPressed(GameInput::kKey_2) ||
        (g_mode == MODE_COMPUTE && (GameInput::IsFirstPressed(GameInput::kKey_space) || GameInput::IsFirstPressed(GameInput::kXButton))))
        g_mode = MODE_DIFFUSE_REFLECT;
    else if (GameInput::IsFirstPressed(GameInput::kKey_3))
        g_mode = MODE_DIFFUSE;
    else if (GameInput::IsFirstPressed(GameInput::kKey_i))
        g_showInfo = !g_showInfo;
    else if (GameInput::IsFirstPressed(GameInput::kKey_l))
        g_showLabels = !g_showLabels;
    else if (GameInput::IsFirstPressed(GameInput::kKey_h))
        g_showHelp = !g_showHelp;
    else if (GameInput::IsFirstPressed(GameInput::kMouse1))
        GameInput::UnaquireKbm(true);
    else if ((GameInput::IsFirstPressed(GameInput::kKey_t) && (GameInput::IsPressed(GameInput::kKey_lshift) || GameInput::IsPressed(GameInput::kKey_rshift))) ||
            (GameInput::IsFirstPressed(GameInput::kBButton) && GameInput::IsPressed(GameInput::kAButton)))
    {
        loadTraceLog();
        _traceLog.push_back(_result);
        saveTraceLog();
        g_traceLogCur.SetMaxValue((int)_traceLog.size());
        g_traceLogCur = (int)_traceLog.size();
    }
    else if ((GameInput::IsFirstPressed(GameInput::kKey_t) && !GameInput::IsPressed(GameInput::kKey_lshift) && !GameInput::IsPressed(GameInput::kKey_rshift)) ||
            (GameInput::IsFirstPressed(GameInput::kBButton) && !GameInput::IsPressed(GameInput::kAButton)))
    {
        loadTraceLog();
        if (!g_traceLogCur) g_traceLogCur = (int)_traceLog.size();
        _traceLogCurLast = 0;
    }

    HandleDigitalButtonPress(GameInput::kKey_minus, deltaT, [&]{ DebugZoom.Decrement(); });
    HandleDigitalButtonPress(GameInput::kKey_equals, deltaT, [&]{ DebugZoom.Increment(); });
    HandleDigitalButtonPress(GameInput::kRThumbClick, deltaT, [&]{ DebugZoom.Increment(); });
    if (!g_traceLogCur)
    {
        HandleDigitalButtonPress(GameInput::kKey_comma, deltaT, [&]{ g_difficulty.Decrement(); });
        HandleDigitalButtonPress(GameInput::kLShoulder, deltaT, [&]{ g_difficulty.Decrement(); });
        HandleDigitalButtonPress(GameInput::kKey_period, deltaT, [&]{ g_difficulty.Increment(); });
        HandleDigitalButtonPress(GameInput::kRShoulder, deltaT, [&]{ g_difficulty.Increment(); });
    }
    if (!EngineTuning::sm_IsVisible && !EngineProfiling::DrawProfiler)
    {
        if (g_mode != MODE_COMPUTE)
        {
            if (!GameInput::IsPressed(GameInput::kKey_lshift) && !GameInput::IsPressed(GameInput::kKey_rshift) && !GameInput::IsPressed(GameInput::kAButton))
            {
                HandleDigitalButtonPress(GameInput::kKey_left, deltaT, [&]{ g_cameraPresetCur.Decrement(); });
                HandleDigitalButtonPress(GameInput::kKey_right, deltaT, [&]{ g_cameraPresetCur.Increment(); });
                HandleDigitalButtonPress(GameInput::kDPadLeft, deltaT, [&]{ g_cameraPresetCur.Decrement(); });
                HandleDigitalButtonPress(GameInput::kDPadRight, deltaT, [&]{ g_cameraPresetCur.Increment(); });
            }
            else
            {
                HandleDigitalButtonPress(GameInput::kKey_left, deltaT, [&]{ if (g_cameraPresetCur == CAMERAPRESET_HIT) g_cameraHitCur.Decrement(); else _cameraHitLast = -1; });
                HandleDigitalButtonPress(GameInput::kKey_right, deltaT, [&]{ if (g_cameraPresetCur == CAMERAPRESET_HIT) g_cameraHitCur.Increment(); else _cameraHitLast = -1; });
                HandleDigitalButtonPress(GameInput::kDPadLeft, deltaT, [&]{ if (g_cameraPresetCur == CAMERAPRESET_HIT) g_cameraHitCur.Decrement(); else _cameraHitLast = -1; });
                HandleDigitalButtonPress(GameInput::kDPadRight, deltaT, [&]{ if (g_cameraPresetCur == CAMERAPRESET_HIT) g_cameraHitCur.Increment(); else _cameraHitLast = -1; });
            }
        }

        if (!GameInput::IsPressed(GameInput::kKey_lshift) && !GameInput::IsPressed(GameInput::kKey_rshift) && !GameInput::IsPressed(GameInput::kAButton))
        {
            HandleDigitalButtonPress(GameInput::kKey_up, deltaT, [&]{ g_traceLogCur.Increment(); if (!g_traceLogCur) g_traceLogCur = 1; });
            HandleDigitalButtonPress(GameInput::kDPadUp, deltaT, [&]{ g_traceLogCur.Increment(); if (!g_traceLogCur) g_traceLogCur = 1; });
            HandleDigitalButtonPress(GameInput::kKey_down, deltaT, [&]{ g_traceLogCur.Decrement(); if (!g_traceLogCur) g_traceLogCur = g_traceLogCur.GetMaxValue(); });
            HandleDigitalButtonPress(GameInput::kDPadDown, deltaT, [&]{ g_traceLogCur.Decrement(); if (!g_traceLogCur) g_traceLogCur = g_traceLogCur.GetMaxValue(); });
        }
        else
        {
            auto up = [&]
            {
                int count = 0, start = !g_traceLogCur ? 1 : g_traceLogCur;
                do
                {
                    g_traceLogCur.Increment();
                    if (!g_traceLogCur) g_traceLogCur = 1;
                } while (g_traceLogCur && !_traceLog[g_traceLogCur-1].success && (!count++ || g_traceLogCur != start));
            };
            HandleDigitalButtonPress(GameInput::kKey_up, deltaT, up);
            HandleDigitalButtonPress(GameInput::kDPadUp, deltaT, up);

            auto down = [&]
            {
                int count = 0, start = !g_traceLogCur ? g_traceLogCur.GetMaxValue() : g_traceLogCur;
                do
                {
                    g_traceLogCur.Decrement();
                    if (!g_traceLogCur) g_traceLogCur = g_traceLogCur.GetMaxValue();
                } while (g_traceLogCur && !_traceLog[g_traceLogCur-1].success && (!count++ || g_traceLogCur != start));
            };
            HandleDigitalButtonPress(GameInput::kKey_down, deltaT, down);
            HandleDigitalButtonPress(GameInput::kDPadDown, deltaT, down);
        }
    }    

    if (g_mode != MODE_COMPUTE)
    {
        if (g_cameraPresetCur != _cameraPresetLast || g_cameraHitCur != _cameraHitLast)
        {
            if (g_cameraHitCur != _cameraHitLast)
            {
                g_cameraPresetCur = CAMERAPRESET_HIT;
                _cameraHitLast = g_cameraHitCur;
            }
            if (g_cameraPresetCur == CAMERAPRESET_HIT && g_instanceDescs)
            {
                int index = g_rayHitInsts[g_cameraHitCur-1];
                Vector3 pos = {g_instanceDescs[index].Transform[0][3], g_instanceDescs[index].Transform[1][3], g_instanceDescs[index].Transform[2][3]};
                s_cameraPresets[g_cameraPresetCur].pos = pos + Vector3(0,0,2);
            }
            _cameraPresetSave = s_cameraPresets[g_cameraPresetCur];
            setCameraPreset(_cameraPresetSave);
            _cameraPresetLast = g_cameraPresetCur;
        }
    }

    if (g_traceLogCur != _traceLogCurLast)
    {
        if (g_traceLogCur)
        {
            _seed = _traceLog[g_traceLogCur-1].seed;
            _target = _traceLog[g_traceLogCur-1].target;
            _verifyPos = _traceLog[g_traceLogCur-1].pos;
        }
        else
            _verifyPos = {-1,-1};
        g_mode = MODE_COMPUTE;
        _traceLogCurLast = g_traceLogCur;
    }

    if (g_mode != MODE_COMPUTE && !g_traceLogCur && _seed != _result.seed)
    {
        _revertBestHash = true;
        _seed = _result.seed;
        _target = _result.target;
        _verifyPos = {_result.pos.x, _result.pos.y};
        g_mode = MODE_COMPUTE;
    }

    if (g_mode != _modeLast)
    {
        if (g_mode == MODE_COMPUTE)
        {
            _result = {};
            resetBestHash();
            g_traceResult = {};
            g_rayLen = 0;
            g_rayHitInsts = {};
            g_rayHitMap.clear();
            setCameraPreset(s_cameraPresets[CAMERAPRESET_ORIGIN]);
            _vsyncSave = s_EnableVSync;
            s_EnableVSync = false;
        }
        else if (_modeLast == MODE_COMPUTE)
        {
            _verifyPos = {-1,-1};
            buildPath();

            if (g_cameraPresetCur == CAMERAPRESET_HIT && g_instanceDescs)
            {
                int index = g_rayHitInsts[g_cameraHitCur-1];
                Vector3 pos = {g_instanceDescs[index].Transform[0][3], g_instanceDescs[index].Transform[1][3], g_instanceDescs[index].Transform[2][3]};
                s_cameraPresets[g_cameraPresetCur].pos = pos + Vector3(0,0,2);
                _cameraPresetSave = s_cameraPresets[g_cameraPresetCur];
            }
            setCameraPreset(_cameraPresetSave);

            s_EnableVSync = _vsyncSave;
        }
        _modeLast = g_mode;
    }

    if (g_mode != MODE_COMPUTE)
    {
        _cameraController->Update(deltaT);
        _cameraPresetSave = { _camera.GetPosition(), _cameraController->GetCurrentHeading(), _cameraController->GetCurrentPitch() };
    }

    if (!g_traceLogCur && !_revertBestHash)
    {
        if (g_mode == MODE_COMPUTE) for (auto& e : _seed) e = (rand() << 16) | rand();
        for (int i = 0; i < _target.size(); ++i) _target[i] = ~(INT64(~0) << max(32 - max((int)g_difficulty - 32*i, 0), 0));
    }
#endif
    const int framesAvgCount = 20;
    static float frameRates[framesAvgCount] = {};
    frameRates[Graphics::GetFrameCount() % framesAvgCount] = Graphics::GetFrameRate();
    float frameRateAvg = 0.0;
    for (auto frameRate : frameRates) frameRateAvg += frameRate / framesAvgCount;
    _tracePerSec = g_screenDim*g_screenDim*frameRateAvg;
#ifndef COMPUTE_ONLY
    EngineProfiling::DrawFrameRate = (bool)g_showInfo;
    _traceLogStatusTime = max(_traceLogStatusTime - min(deltaT, 1.f), 0.f);
    
    _cameraController->SetMoveSpeed(g_moveSpeed);
    TextRenderer::g_color = {g_textColor_r, g_textColor_g, g_textColor_b, g_textColor_a};
    TextRenderer::g_hiColor = {g_textHiColor_r, g_textHiColor_g, g_textHiColor_b, g_textHiColor_a};

    g_time = fmod(g_time+deltaT, 10000.f);
#endif
}

void RaycoinViewer::setCameraPreset(const CameraPreset& cameraPreset)
{    
    _cameraController->SetCurrentHeading(cameraPreset.heading);
    _cameraController->SetCurrentPitch(cameraPreset.pitch);

    Matrix3 neworientation = Matrix3(_cameraController->GetWorldEast(), _cameraController->GetWorldUp(), -_cameraController->GetWorldNorth()) 
                           * Matrix3::MakeYRotation(_cameraController->GetCurrentHeading())
                           * Matrix3::MakeXRotation(_cameraController->GetCurrentPitch());
    _camera.SetTransform(AffineTransform(neworientation, cameraPreset.pos));
    _camera.Update();
}

void RaycoinViewer::RenderScene(void)
{
    if (g_mode == MODE_COMPUTE)
    {
        if (_resetAccelStruct)
        {
            auto& context = ComputeContext::Begin();
            context.Finish(true);
            if (g_instanceDescs)
            {
                g_instanceDescsRead.Unmap();
                g_instanceDescs = nullptr;
            }
            g_lineIndexBuffer.Destroy();
            g_lineVertexBuffer.Destroy();
            g_lineAttribBuffer.Destroy();

            buildAccelStruct();
            _resetAccelStruct = false;
        }
        auto& context = ComputeContext::Begin(L"Init Field");
        auto* pCommandList = context.GetCommandList();
        auto& hitShaderTable = g_RaytracingInputs[Diffuse]._hitShaderTable;
        auto& hitShaderTableRead = g_RaytracingInputs[Diffuse]._hitShaderTableRead;

        InitFieldConstants initFieldConstants;
        memcpy(initFieldConstants.seed, _seed.data(), sizeof(_seed));
        context.WriteBuffer(g_initFieldConstantBuffer, 0, &initFieldConstants, sizeof(initFieldConstants));
        context.TransitionResource(g_initFieldConstantBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        context.TransitionResource(hitShaderTable, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.TransitionResource(g_instanceDescsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.FlushResourceBarriers();

        context.SetPipelineState(_initFieldPSO);
        context.SetRootSignature(_rootSig);
        context.SetConstantBuffer(0, g_initFieldConstantBuffer.GetGpuVirtualAddress());
        context.SetBufferUAV(2, hitShaderTable);
        context.SetBufferUAV(3, g_instanceDescsBuffer);
        context.Dispatch();
        context.InsertUAVBarrier(hitShaderTable);
        context.InsertUAVBarrier(g_instanceDescsBuffer);
#ifndef COMPUTE_ONLY
        if (g_instanceDescs) g_instanceDescsRead.Unmap();
        context.CopyBuffer(g_instanceDescsRead, g_instanceDescsBuffer);
        if (g_hitShaderTable) hitShaderTableRead.Unmap();
        context.CopyBuffer(hitShaderTableRead, hitShaderTable);
        context.Finish(true);
        g_instanceDescs = static_cast<D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC*>(g_instanceDescsRead.Map());
        g_hitShaderTable = static_cast<byte*>(hitShaderTableRead.Map());
#else
        context.Finish();
#endif
    }

    if (g_mode == MODE_COMPUTE)
    {
        auto& context = GraphicsContext::Begin(L"Update Accel Struct");
        auto* pCommandList = context.GetCommandList();
        CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
        g_pRaytracingDevice->QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));

        ID3D12DescriptorHeap *pDescriptorHeaps[] = { &g_pRaytracingDescriptorHeap->GetDescriptorHeap() };
        pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelAccelDesc = {};
        topLevelAccelDesc.SourceAccelerationStructureData = g_topLevelAccel->GetGPUVirtualAddress();
        topLevelAccelDesc.DestAccelerationStructureData = g_topLevelAccel->GetGPUVirtualAddress();
        topLevelAccelDesc.ScratchAccelerationStructureData = g_scratchBuffer.GetGpuVirtualAddress();

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS &topLevelInputs = topLevelAccelDesc.Inputs;
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        topLevelInputs.NumDescs = g_sphereCount;
        topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
        topLevelInputs.pGeometryDescs = nullptr;
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.InstanceDescs = g_instanceDescsBuffer.GetGpuVirtualAddress();

        pRaytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelAccelDesc, 0, nullptr);
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(g_topLevelAccel.GetResource());
        pCommandList->ResourceBarrier(1, &uavBarrier);
        context.Finish();
    }

    {
        auto& context = GraphicsContext::Begin(L"Ray Trace");
        auto* pCommandList = context.GetCommandList();
        CComPtr<ID3D12RaytracingFallbackCommandList> pRaytracingCommandList;
        g_pRaytracingDevice->QueryRaytracingCommandList(pCommandList, IID_PPV_ARGS(&pRaytracingCommandList));

        HitShaderConstants inputs = g_hitShaderConstants;
#ifndef COMPUTE_ONLY
        auto mat = Transpose(Invert(_camera.GetViewProjMatrix()));
        memcpy(&inputs.cameraToWorld, &mat, sizeof(inputs.cameraToWorld));
        inputs.worldCameraPosition = _camera.GetPosition();
        inputs.bgColor = { g_bgColor_r, g_bgColor_g, g_bgColor_b };
        inputs.diffuseColor = {g_diffuseColor_r, g_diffuseColor_g, g_diffuseColor_b};
        inputs.ambientColor = Vector3(g_ambientColor_r, g_ambientColor_g, g_ambientColor_b) * g_ambient;
        inputs.sunColor = Vector3(g_sunColor_r, g_sunColor_g, g_sunColor_b) * g_sun;
        inputs.rayColorStart = _result.success ?    Vector3(g_rayColorStart_r, g_rayColorStart_g, g_rayColorStart_b) :
                                                    Vector3(g_rayFailColorStart_r, g_rayFailColorStart_g, g_rayFailColorStart_b);
        inputs.rayColorMid = _result.success ?      Vector3(g_rayColorMid_r, g_rayColorMid_g, g_rayColorMid_b) :
                                                    Vector3(g_rayFailColorMid_r, g_rayFailColorMid_g, g_rayFailColorMid_b);
        inputs.rayColorEnd = _result.success ?      Vector3(g_rayColorEnd_r, g_rayColorEnd_g, g_rayColorEnd_b) :
                                                    Vector3(g_rayFailColorEnd_r, g_rayFailColorEnd_g, g_rayFailColorEnd_b);
#endif
        memcpy(inputs.targetHash, _target.data(), sizeof(_target));
        inputs.verifyPos = { _verifyPos.x, _verifyPos.y };
#ifndef COMPUTE_ONLY
        inputs.diffuse = g_diffuse;
        inputs.gloss = g_gloss;
        inputs.specular = g_specular;
        inputs.shadePathMax = g_shadePathMax+1;
        inputs.reflect = g_reflect;
        inputs.reflectFull = g_reflectFull;
        inputs.ray = {g_rayMin, g_rayMax};
        inputs.rayHit = {g_rayHitMin, g_rayHitMax};
        inputs.rayLen = g_rayLen;
        inputs.time = g_time;
        inputs.shadeLastMissOnly = g_shadeLastMissOnly;
        inputs.trace = g_mode != MODE_DIFFUSE;
        inputs.compute = g_mode == MODE_COMPUTE;
        inputs.hashing = g_hashing;
#endif
        inputs.bestHash = _bestHash;
#ifndef COMPUTE_ONLY
        float costheta = cosf(g_sunOrientation);
        float sintheta = sinf(g_sunOrientation);
        float cosphi = cosf(g_sunInclination * 3.14159f * 0.5f);
        float sinphi = sinf(g_sunInclination * 3.14159f * 0.5f);
        inputs.sunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
#endif
        context.WriteBuffer(g_hitShaderConstantBuffer, 0, &inputs, sizeof(inputs));
        context.TransitionResource(g_hitShaderConstantBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        ::TraceResult traceResult[2] = {};
        memcpy(traceResult[0].hash, _result.hash.data(), sizeof(_result.hash));
        context.WriteBuffer(g_traceResultBuffer, 0, &traceResult, sizeof(traceResult));
        context.TransitionResource(g_traceResultBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#ifndef COMPUTE_ONLY
        context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#endif
        context.FlushResourceBarriers();

        ID3D12DescriptorHeap *pDescriptorHeaps[] = { &g_pRaytracingDescriptorHeap->GetDescriptorHeap() };
        pRaytracingCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);

        pCommandList->SetComputeRootSignature(g_GlobalRaytracingRootSignature);
#ifndef COMPUTE_ONLY
        pCommandList->SetComputeRootDescriptorTable(0, g_OutputUAV);
#endif
        pRaytracingCommandList->SetTopLevelAccelerationStructure(1, g_topLevelAccelPointer);
        pCommandList->SetComputeRootConstantBufferView(2, g_hitShaderConstantBuffer.GetGpuVirtualAddress());
        pCommandList->SetComputeRootShaderResourceView(3, g_attribBuffer.GetGpuVirtualAddress());
#ifndef COMPUTE_ONLY
        pCommandList->SetComputeRootShaderResourceView(4, g_lineAttribBuffer.GetGpuVirtualAddress());
#endif
        pCommandList->SetComputeRootUnorderedAccessView(5, g_traceResultBuffer.GetGpuVirtualAddress());

        int dispatchDim = _verifyPos.x < 0 ? g_screenDim : 1;
        D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = g_RaytracingInputs[Diffuse].GetDispatchRayDesc(dispatchDim, dispatchDim);
        pRaytracingCommandList->SetPipelineState1(g_RaytracingInputs[Diffuse]._pPSO);
        pRaytracingCommandList->DispatchRays(&dispatchRaysDesc);

        // Clear the context's descriptor heap since ray tracing changes this underneath the sheets
        context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nullptr);

        if (g_mode == MODE_COMPUTE)
        {
            context.CopyBuffer(g_traceResultRead, g_traceResultBuffer);
            context.Finish(true);
            auto traceResults = static_cast<::TraceResult*>(g_traceResultRead.Map());
            auto& traceResult = traceResults[(bool)traceResults[1].success];
            if (traceResult.success || g_traceLogCur || memcmp(traceResult.hash, _result.hash.data(), sizeof(_result.hash)))
            {
                _result.success = traceResult.success;
                _result.seed = _seed;
                _result.target = _target;
                memcpy(_result.hash.data(), traceResult.hash, sizeof(_result.hash));
                _result.pos = { traceResult.pos.x, traceResult.pos.y };
                _result.depth = traceResult.depth;
                _result.timestamp = g_traceLogCur ? _traceLog[g_traceLogCur-1].timestamp : chrono::system_clock::now();
#ifndef COMPUTE_ONLY
                g_traceResult = traceResult;
#endif
            }
#ifndef COMPUTE_ONLY
            if (traceResult.success || g_traceLogCur || _revertBestHash)
            {
                g_mode = MODE_DIFFUSE_REFLECT;
                _revertBestHash = false;
            }
#endif
            g_traceResultRead.Unmap();   
        }
        else
            context.Finish();
#ifndef COMPUTE_ONLY
        if (_verifyPos.x >= 0) Graphics::g_skipPresent = 1;
#endif
    }
}

void RaycoinViewer::buildPath()
{
#ifndef COMPUTE_ONLY
    vector<Vertex> verts;
    vector<Triangle> indices;
    vector<LineAttrib> attribs;
    verts.reserve(g_lineMesh.first.size()*g_pathMax);
    indices.reserve(g_lineMesh.second.size()*g_pathMax);
    attribs.reserve(g_lineMesh.second.size()*3*g_pathMax);

    const Vector3 arrowHeadSizeLarge = {g_rayArrowSize/3, g_rayArrowSize, g_rayArrowSize/3};
    const Vector3 arrowHeadSizeSmall = arrowHeadSizeLarge/2;

    g_rayLen = 0;
    g_rayHitInsts = {};
    float rayHitT[g_hitCount] = {};
    Vector3 rayHitPos[g_hitCount] = {};
    g_rayHitMap.clear();
    int hit = 0;
    for (int i = 0; i < (int)g_traceResult.pathCount; ++i)
    {
        auto& path = g_traceResult.path[i];
        Vector3 start = {path.pos.x, path.pos.y, path.pos.z};
        Vector3 dir = {path.dir.x, path.dir.y, path.dir.z};
        Vector3 end;
        float len;

        if (path.instId >= 0)
        {
            //ray hit sphere
            end = start + dir*path.rayT;
            g_rayHitInsts[hit] = path.instId;
            len = Length(end-start);
            rayHitT[hit] = g_rayLen + len;
            rayHitPos[hit] = end;
            g_rayHitMap.insert(make_pair(path.instId, hit++));
        }
        else
        {
            //ray missed, clip ray against field bounds
            Ray ray = {start, dir};
            float dim = (g_sphereDim/2)*4;
            Vector3 bounds[2] = {{-(dim+4), -(dim+4), -(dim+2)}, {dim, dim, dim+2}};
            pair<bool, Vector3> res = make_pair(false, start);
            if      ((res = intersect({{ 1, 0, 0}, bounds[0][0]}, ray)).first && isInRange({0,             res.second[1], res.second[2]}, bounds[0], bounds[1])) {}
            else if ((res = intersect({{-1, 0, 0},-bounds[1][0]}, ray)).first && isInRange({0,             res.second[1], res.second[2]}, bounds[0], bounds[1])) {}
            else if ((res = intersect({{ 0, 1, 0}, bounds[0][1]}, ray)).first && isInRange({res.second[0], 0,             res.second[2]}, bounds[0], bounds[1])) {}
            else if ((res = intersect({{ 0,-1, 0},-bounds[1][1]}, ray)).first && isInRange({res.second[0], 0,             res.second[2]}, bounds[0], bounds[1])) {}
            else if ((res = intersect({{ 0, 0, 1}, bounds[0][2]}, ray)).first && isInRange({res.second[0], res.second[1], 0},             bounds[0], bounds[1])) {}
            else if ((res = intersect({{ 0, 0,-1},-bounds[1][2]}, ray)).first && isInRange({res.second[0], res.second[1], 0},             bounds[0], bounds[1])) {}
            end = res.second;
            len = Length(end-start);
        }

        //draw ray segment
        for (auto& e: g_lineMesh.second)
            indices.push_back({
            (Index)(verts.size() + e.v[0]),
            (Index)(verts.size() + e.v[1]),
            (Index)(verts.size() + e.v[2])
                });

        Vector3 pos = start;
        auto rot = quatFromAlign(Vector3(0,1,0), dir); //mesh is oriented vertically
        Vector3 scale = Vector3(g_raySize/2, len, g_raySize/2);
        for (auto& e: g_lineMesh.first)
            verts.push_back(pos + rot*(e*scale));

        for (auto& e: g_lineMesh.second) //calc t for each vertex using scaled mesh height
            for (int i = 0; i < 3; ++i)
                attribs.push_back({g_rayLen + g_lineMesh.first[e.v[i]][1]*len});

        //if we are at origin
        if (i == 0 || g_traceResult.path[i-1].instId == g_instMiss)
        {
            //use larger arrow for start, smaller for miss continuation
            Vector3 arrowHeadSize = i == 0 ? arrowHeadSizeLarge : arrowHeadSizeSmall;

            //draw in-arrow
            for (auto& e: g_arrowHeadMesh.second)
                indices.push_back({
                (Index)(e.v[0] + verts.size()),
                (Index)(e.v[1] + verts.size()),
                (Index)(e.v[2] + verts.size())
                    });

            Vector3 pos = start - dir*arrowHeadSize[1];
            auto rot = quatFromAlign(Vector3(0,1,0), dir);
            Vector3 scale = arrowHeadSize;
            for (auto& e: g_arrowHeadMesh.first)
                verts.push_back(pos + rot*(e*scale));

            for (auto& e: g_arrowHeadMesh.second)
                for (int i = 0; i < 3; ++i)
                    attribs.push_back({g_rayLen});
        }

        if (path.instId == g_instMiss)
        {
            //draw out-arrow
            for (auto& e: g_arrowHeadMesh.second)
                indices.push_back({
                (Index)(e.v[0] + verts.size()),
                (Index)(e.v[1] + verts.size()),
                (Index)(e.v[2] + verts.size())
                    });

            Vector3 pos = end;
            auto rot = quatFromAlign(Vector3(0,1,0), dir);
            Vector3 scale = arrowHeadSizeSmall;
            for (auto& e: g_arrowHeadMesh.first)
                verts.push_back(pos + rot*(e*scale));

            for (auto& e: g_arrowHeadMesh.second)
                for (int i = 0; i < 3; ++i)
                    attribs.push_back({g_rayLen + len});
        }

        if (i == g_traceResult.pathCount-1)
        {
            //draw terminal
            for (auto& e: g_lineMesh.second)
                indices.push_back({
                (Index)(e.v[0] + verts.size()),
                (Index)(e.v[1] + verts.size()),
                (Index)(e.v[2] + verts.size())
                    });

            Vector3 pos = end - dir*arrowHeadSizeLarge[1]/2;
            auto rot = quatFromAlign(Vector3(0,1,0), dir);
            Vector3 scale = arrowHeadSizeLarge;
            for (auto& e: g_lineMesh.first)
                verts.push_back(pos + rot*(e*scale));

            for (auto& e: g_lineMesh.second)
                for (int i = 0; i < 3; ++i)
                    attribs.push_back({g_rayLen + len});
        }

        g_rayLen += len;
    }

    if (g_instanceDescs && g_hitShaderTable)
    {
        for (auto it = g_rayHitMap.begin(); it != g_rayHitMap.end();)
        {
            int index = it->first;
            auto mat = reinterpret_cast<MaterialRootConstants*>(g_hitShaderTable + index*g_shaderRecordSizeInBytes + g_offsetToMaterialConstants);
            Vector3 pos = {g_instanceDescs[index].Transform[0][3], g_instanceDescs[index].Transform[1][3], g_instanceDescs[index].Transform[2][3]};
            mat->hitCenter = {pos[0], pos[1], pos[2]};

            int i = 0;
            for (auto itEnd = g_rayHitMap.upper_bound(index); it != itEnd && i < g_sphereHitMax; ++it, ++i)
            {
                mat->hitT[i] = rayHitT[it->second];
                Vector3 dir = rayHitPos[it->second] - pos;
                mat->hitPolar[i] = {acos(dir[1]), atan2(dir[2],dir[0]), 0};
            }
        }

        auto& context = ComputeContext::Begin();
        auto& hitShaderTable = g_RaytracingInputs[Diffuse]._hitShaderTable;
        context.WriteBuffer(hitShaderTable, 0, g_hitShaderTable, hitShaderTable.GetBufferSize());
        context.TransitionResource(hitShaderTable, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.FlushResourceBarriers();
        context.InsertUAVBarrier(hitShaderTable);
        context.Finish(true);
    }

    if (indices.size())
    {
        g_lineVertexBuffer.Create(L"Line Vertex Buffer", (int)verts.size(), sizeof(Vertex), verts.data());
        g_lineIndexBuffer.Create(L"Line Index Buffer", (int)indices.size(), sizeof(Triangle), indices.data());
        g_lineAttribBuffer.Create(L"Line Attributes Buffer", (int)attribs.size(), sizeof(LineAttrib), attribs.data());
    }
    buildAccelStruct();
    _resetAccelStruct = true;
#endif
}

void RaycoinViewer::RenderUI(class GraphicsContext& context)
{
#ifndef COMPUTE_ONLY
    const Color color = TextRenderer::g_color;
    const Color hiColor = TextRenderer::g_hiColor;

    TextContext text(context);
    text.Begin();

    if (g_showLabels && g_instanceDescs && g_hitShaderTable)
    {
        static const int g_labelDim = 5;
        static const int g_labelCount = g_labelDim*g_labelDim*g_labelDim;
        static const float g_labelFadeDistMin = 4;
        static const float g_labelFadeDistMax = (g_labelDim/2)*4;

        for (int i = 0; i < g_labelCount; ++i)
        {
            int index = (int)(_camera.GetPosition()[0]/4.f) + g_sphereDim/2 - g_labelDim/2 + i%g_labelDim +
                ((int)(_camera.GetPosition()[1]/4.f) + g_sphereDim/2 - g_labelDim/2 + (i/g_labelDim)%g_labelDim)*g_sphereDim +
                ((int)((_camera.GetPosition()[2]-2.f)/4.f) + g_sphereDim/2 - g_labelDim/2 + i/(g_labelDim*g_labelDim))*g_sphereDim*g_sphereDim;
            if (index < 0 || index >= g_sphereCount) continue;

            Vector3 pos = {g_instanceDescs[index].Transform[0][3], g_instanceDescs[index].Transform[1][3], g_instanceDescs[index].Transform[2][3]};
            Vector3 camToPos = pos - _camera.GetPosition();
            if (Length(camToPos) > 1.f && Dot(Normalize(camToPos), _camera.GetForwardVec()) > 0.f)
            {
                auto hit = g_rayHitMap.equal_range(index);
                auto mat = reinterpret_cast<MaterialRootConstants*>(g_hitShaderTable + index*g_shaderRecordSizeInBytes + g_offsetToMaterialConstants);
                float alpha = 1-min(max((Length(camToPos) - g_labelFadeDistMin) / (g_labelFadeDistMax-g_labelFadeDistMin), 0), 1);
                text.SetColor(hit.first != hit.second ? Color(g_labelHiColor_r, g_labelHiColor_g, g_labelHiColor_b, g_labelHiColor_a*alpha) :
                                                        Color(g_labelColor_r, g_labelColor_g, g_labelColor_b, g_labelColor_a*alpha));
                float lableSize = g_labelSize + g_labelSize*(g_labelSizeScale-1)*alpha;
                text.SetTextSize(lableSize);
                Vector4 screenPos = _camera.GetViewProjMatrix()*Vector4(pos, 1);
                screenPos /= screenPos.GetW();
                float labelChars = 5.5f + distance(hit.first, hit.second)*2.5f;
                text.ResetCursor(   ((1.f+screenPos[0])/2.f)*(float)g_DisplayWidth - lableSize*labelChars/2,
                                    ((1.f-screenPos[1])/2.f)*(float)g_DisplayHeight - lableSize/2);
                for (auto it = hit.first; it != hit.second; ++it)
                    text.DrawFormattedString("%2d: ", it->second+1);
                text.DrawFormattedString("%#08x", mat->label);
            }
        }
    }
    text.SetColor(color);
    text.SetTextSize(14);
    

    if (g_showInfo && !EngineTuning::sm_IsVisible && !EngineProfiling::DrawProfiler)
    {
        text.ResetCursor(0,0);
        text.DrawFormattedString("\nRT/s:      %5.2fM", _tracePerSec / 1000000.f);

        text.SetColor(  GameInput::IsPressed(GameInput::kKey_1) ||
                        GameInput::IsPressed(GameInput::kKey_2) ||
                        GameInput::IsPressed(GameInput::kKey_3) ||
                        GameInput::IsPressed(GameInput::kXButton)
                        ? hiColor : color);
        text.DrawString("\nMode:      "); g_mode.DisplayValue(text);

        text.SetColor(g_mode != MODE_COMPUTE &&
                        (GameInput::IsPressed(GameInput::kKey_a) ||
                        GameInput::IsPressed(GameInput::kKey_w) ||
                        GameInput::IsPressed(GameInput::kKey_s) ||
                        GameInput::IsPressed(GameInput::kKey_d) ||
                        GameInput::IsPressed(GameInput::kKey_e) || 
                        GameInput::IsPressed(GameInput::kKey_q) || 
                        GameInput::IsPressed(GameInput::kKey_left) ||
                        GameInput::IsPressed(GameInput::kKey_right) ||
                        GameInput::GetAnalogInput(GameInput::kAnalogLeftStickX) ||
                        GameInput::GetAnalogInput(GameInput::kAnalogLeftStickY) ||
                        GameInput::GetAnalogInput(GameInput::kAnalogLeftTrigger) ||
                        GameInput::GetAnalogInput(GameInput::kAnalogRightTrigger) ||
                        GameInput::IsPressed(GameInput::kDPadLeft) ||
                        GameInput::IsPressed(GameInput::kDPadRight)
                        ) ? hiColor : color);
        text.DrawFormattedString("\nCamera:    %7.2fm, %7.2fm, %7.2fm",
            (float)_camera.GetPosition()[0], (float)_camera.GetPosition()[1], (float)_camera.GetPosition()[2]);
        for (int i = 0; i < g_cameraPresetCount; ++i)
            if ((float)_camera.GetPosition()[0] == s_cameraPresets[i].pos[0] &&
                (float)_camera.GetPosition()[1] == s_cameraPresets[i].pos[1] &&
                (float)_camera.GetPosition()[2] == s_cameraPresets[i].pos[2])
            {
                    if (i == CAMERAPRESET_HIT)
                        text.DrawFormattedString(" (%s #%d)", g_cameraPresetStrs[i], (int)g_cameraHitCur);
                    else
                        text.DrawFormattedString(" (%s)", g_cameraPresetStrs[i]);
                    break;
            }

        text.SetColor(  GameInput::IsPressed(GameInput::kKey_t) ||
                        GameInput::IsPressed(GameInput::kKey_up) ||
                        GameInput::IsPressed(GameInput::kKey_down) ||
                        GameInput::IsPressed(GameInput::kBButton) ||
                        GameInput::IsPressed(GameInput::kDPadUp) ||
                        GameInput::IsPressed(GameInput::kDPadDown)
                        ? hiColor : color);
        if (!g_traceLogCur)
            text.DrawFormattedString("\nTrace Log: %d Loaded", g_traceLogCur.GetMaxValue());
        else
            text.DrawFormattedString("\nTrace Log: #%d of %d Loaded", (int)g_traceLogCur, g_traceLogCur.GetMaxValue());
        if (_traceLogStatusTime > 0) text.DrawFormattedString(" (%s)", _traceLogStatus.c_str());

        text.SetColor(color);
        text.DrawString("\nSeed:      0x");
        for (auto& e : _seed) text.DrawFormattedString("%08x", e);

        text.SetColor(!g_traceLogCur &&
                        (GameInput::IsPressed(GameInput::kKey_comma) ||
                        GameInput::IsPressed(GameInput::kKey_period) ||
                        GameInput::IsPressed(GameInput::kLShoulder) ||
                        GameInput::IsPressed(GameInput::kRShoulder)
                        ) ? hiColor : color);
        text.DrawString("\nTarget:    0x");
        for (auto& e : _target) text.DrawFormattedString("%08x", e);

        text.SetColor(_result.success ? color : hiColor);
        text.DrawFormattedString("\nResult:    %s", _result.success ?
                                    !g_traceLogCur ?    "Success! =) Press SPACEBAR to continue hashing or SHIFT-T to log this trace." :
                                                        "Success! =) Press UP or DOWN (+SHIFT) to navigate your log." :
                                    !g_traceLogCur ? g_mode != MODE_COMPUTE ?
                                                        "Fail... =( Press SPACEBAR to continue hashing." :
                                                        "Fail... =( Press SPACEBAR to stop hashing or < > to adjust difficulty." :
                                                        "Fail... =( Press UP or DOWN (+SHIFT) to navigate your log.");

        text.SetColor(_result.success ? color : hiColor);
        if (_result.success)
            text.DrawString("\nHash:      0x");
        else
            text.DrawString("\nBest Hash: 0x");
        for (auto& e : _result.hash) text.DrawFormattedString("%08x", e);

        text.DrawFormattedString("\nScreen Coords:  %d, %d", _result.pos.x, _result.pos.y);
        text.DrawFormattedString("\nDepth / Target: %.2fm / %.2fm", _result.depth, g_targetDepth);

        if (g_traceLogCur)
        {    
            text.SetColor(color);
            text.DrawFormattedString("\nBlock Height:   %d", _result.blockHeight);

            std::stringstream s;
            struct tm tm;
            auto time = std::chrono::system_clock::to_time_t(_result.timestamp);
            localtime_s(&tm, &time);
            s << put_time(&tm, "%F %T%z");
            text.DrawFormattedString("\nTimestamp: %s", s.str().c_str());
        }
    }

    if (g_showHelp)
    {
        text.ResetCursor(0, (float)g_DisplayHeight - 80);
        text.SetColor(color);
        text.DrawString("Help:");
        text.DrawString("\nMove: A W S D Q E (+SHIFT)");
        text.DrawString("\nLook: Mouse");
        text.DrawString("\nZoom: + -");
        text.DrawString("\nCameras: LEFT RIGHT (+SHIFT)");
        text.ResetCursor(256, (float)g_DisplayHeight - 80);
        text.DrawString("\nMode: Num 1-3 SPACEBAR");
        text.DrawString("\nDifficulty: < >");
        text.DrawString("\nLoad Log/Log Trace: T SHIFT-T");
        text.DrawString("\nTrace Log #: UP DOWN (+SHIFT)");
        text.ResetCursor(512, (float)g_DisplayHeight - 80);
        text.DrawString("\nEngine Tuning: BACKSPACE");
        text.DrawString("\nHide Info/Labels/Help: I L H");
        text.DrawString("\nExit: R-Click ESCAPE");
    }
    text.End();
#endif
}

RaycoinViewer::TraceResult RaycoinViewer::loadTrace(const UniValue& val)
{
    TraceResult res;
    res.success = val["success"].get_bool();
    res.blockHeight = val["blockHeight"].get_int();
    res.blockNonce = (UINT)val["blockNonce"].get_int64();
    res.blockExtraNonce = (UINT)val["blockExtraNonce"].get_int64();
    auto readHash = [&](string key, Hash& hash)
    {
        stringstream s(val[key].get_str());
        string str;
        str.resize(8);
        s.ignore(2);
        for (auto& e : hash)
        {
            s.read(&str[0], 8);
            stringstream(str) >> hex >> e;
        }
    };
    readHash("seed", res.seed);
    readHash("target", res.target);
    readHash("hash", res.hash);
    res.pos.x = val["pos"]["x"].get_int();
    res.pos.y = val["pos"]["y"].get_int();
    res.depth = (float)val["depth"].get_real();
    res.timestamp = Time(Time::duration(val["timestamp"].get_int64()));
    return res;
}

vector<RaycoinViewer::TraceResult>& RaycoinViewer::loadTraceLog()
{
    try
    {
        string str;
        ifstream is(EngineTuning::g_settingsPath + L"\\" + wstring(g_traceLogFile.begin(), g_traceLogFile.end()));
        is.exceptions(ifstream::failbit | ifstream::badbit);
        char buf[1024];
        auto& read = [&]{ str += string(buf, is.gcount()); };
        try { while (is.read(buf, sizeof(buf)).gcount()) read(); }
        catch (ios_base::failure& e) { if (is.eof()) read(); else throw e; }

        vector<TraceResult> results;
        try
        {
            _traceLogStatus = "";
            UniValue vals;
            vals.read(str);
            int idx = 0;
            for (auto& val : vals.getValues())
            {
                try
                {
                    results.push_back(loadTrace(val));
                }
                catch (exception& e)
                {
                    if (_traceLogStatus.size()) _traceLogStatus += "\n";
                    _traceLogStatus += "Error loading " + g_traceLogFile + " -- Trace #" + (stringstream() << idx).str() + ": " + e.what();
                }
                ++idx;
            }
            _traceLog = results;
            if (!_traceLogStatus.size()) _traceLogStatus = "Loaded " + g_traceLogFile;
            g_traceLogCur.SetMaxValue((int)_traceLog.size());
            g_traceLogCur = min((int)g_traceLogCur, (int)_traceLog.size());
        }
        catch (exception& e)
        {
            _traceLogStatus = "Error loading " + g_traceLogFile + ": " + e.what();
        }
    }
    catch (exception&)
    {
        char buf[128];
        strerror_s(buf, sizeof(buf), errno);
        _traceLogStatus = "Error loading " + g_traceLogFile + ": " + buf;
    }

    _traceLogStatusTime = 5;
    return _traceLog;
}

UniValue RaycoinViewer::saveTrace(const TraceResult& res)
{
    UniValue val(UniValue::VOBJ);
    val.pushKV("success", res.success);
    val.pushKV("blockHeight", res.blockHeight);
    val.pushKV("blockNonce", (int64_t)res.blockNonce);
    val.pushKV("blockExtraNonce", (int64_t)res.blockExtraNonce);
    auto writeHash = [&](string key, const Hash& hash)
    {
        stringstream s;
        s << "0x";
        for (auto& e : hash) s << hex << setw(8) << setfill('0') << e;
        val.pushKV(key, s.str());
    };
    writeHash("seed", res.seed);
    writeHash("target", res.target);
    writeHash("hash", res.hash);
    {
        UniValue vals(UniValue::VOBJ);
        vals.pushKV("x", res.pos.x);
        vals.pushKV("y", res.pos.y);
        val.pushKV("pos", vals);
    }
    val.pushKV("depth", res.depth);
    val.pushKV("timestamp", res.timestamp.time_since_epoch().count());
    {
        std::stringstream s;
        struct tm tm;
        auto time = std::chrono::system_clock::to_time_t(res.timestamp);
        localtime_s(&tm, &time);
        s << put_time(&tm, "%F %T%z");
        val.pushKV("localtime", s.str().c_str());
    }
    return val;
}

void RaycoinViewer::saveTraceLog()
{
    UniValue vals(UniValue::VARR);
    for (auto& res : _traceLog) vals.push_back(saveTrace(res));

    try
    {
        ofstream os(EngineTuning::g_settingsPath + L"\\" + wstring(g_traceLogFile.begin(), g_traceLogFile.end()));
        os.exceptions(ofstream::failbit | ofstream::badbit);
        os << vals.write(4);
        _traceLogStatus = "Saved " + g_traceLogFile;
    }
    catch (exception&)
    {
        char buf[128];
        strerror_s(buf, sizeof(buf), errno);
        _traceLogStatus = "Error saving " + g_traceLogFile + ": " + buf;
    }
    _traceLogStatusTime = 5;
}
