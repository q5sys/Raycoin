//Copyright (C) 2018 NewGamePlus Inc.
//Distributed under the MIT software license

#pragma once

#include <chrono>
#include <array>
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CameraController.h"
#include "Camera.h"
#include "univalue.h"

#define RAYCOIN_VERSION_MAJOR 1
#define RAYCOIN_VERSION_MINOR 0

#define RAYCOIN_TOSTR2(s) #s
#define RAYCOIN_TOSTR(s) RAYCOIN_TOSTR2(s)
#define RAYCOIN_VERSION_STRING "v" RAYCOIN_TOSTR(RAYCOIN_VERSION_MAJOR) "." RAYCOIN_TOSTR(RAYCOIN_VERSION_MINOR)

class RaycoinViewer : public GameCore::IGameApp
{
public:
    typedef std::array<UINT, 8> Hash;
    struct ScreenCoord { union { struct { int x, y; }; int a[2]; }; };
    typedef std::chrono::system_clock::time_point Time;

    struct TraceResult
    {
        bool success;
        int blockHeight;
        UINT blockNonce;
        UINT blockExtraNonce;
        Hash seed;
        Hash target;
        Hash hash;
        ScreenCoord pos;
        float depth;
        Time timestamp;
    };

    static RaycoinViewer inst;

    RaycoinViewer() {}

    void parseCommandLineArgs(const WCHAR* argv[], int argc);
    void start();
#ifdef COMPUTE_ONLY
    void update();
    void terminate();
#endif
    virtual void Startup() override;
    virtual void Cleanup() override;
    virtual bool IsDone();

    virtual void Update(float deltaT) override;
    virtual void RenderScene() override;
    virtual void RenderUI(class GraphicsContext&) override;

    bool hasAdapter() const;
    bool bestHash() const                       { return _bestHash; }
    void bestHash(bool enable)                  { _bestHash = enable; }
    void resetBestHash()                        { _result.hash.fill(~0); } 
    void blockInfo(int blockHeight, UINT blockNonce, UINT blockExtraNonce)
                                                { _blockHeight = blockHeight; _blockNonce = blockNonce; _blockExtraNonce = blockExtraNonce; }
    void seed(const Hash& seed)                 { _seed = seed; }
    void target(const Hash& target)             { _target = target; }
    void verifyPos(const ScreenCoord& pos)      { _verifyPos = pos; }
    
    TraceResult& traceResult()                  { return _result; }
    const std::string& traceError() const       { return _error; }
    float tracePerSec() const                   { return _tracePerSec; }
    void resetTracePerSec();

    TraceResult loadTrace(const UniValue& val);
    UniValue saveTrace(const TraceResult& res);

    std::vector<TraceResult>& loadTraceLog();
    void saveTraceLog();

private:
    struct CameraPreset
    {
        Math::Vector3 pos;
        float heading;
        float pitch;
    };

    void setCameraPreset(const CameraPreset& cameraPreset);
    void buildAccelStruct();
    void buildPath();

    Math::Camera    _camera;
    std::auto_ptr<GameCore::CameraController> _cameraController;
    static CameraPreset s_cameraPresets[];
    int             _cameraPresetLast;
    CameraPreset    _cameraPresetSave;
    int             _cameraHitLast;

    static const int framesAvgDim = 10;
    float           _frameRates[framesAvgDim][framesAvgDim];
    float           _frameRateTotals[framesAvgDim];
    int             _frameCount;
    float           _tracePerSec;

    RootSignature   _rootSig;
    ComputePSO      _initFieldPSO;
    bool            _bestHash;
    int             _blockHeight;
    UINT            _blockNonce;
    UINT            _blockExtraNonce;
    Hash            _seed;
    Hash            _target;
    ScreenCoord     _verifyPos;
    TraceResult     _result;
    std::string     _error;
    std::vector<TraceResult> _traceLog;
    std::string     _traceLogStatus;
    float           _traceLogStatusTime;
    int             _traceLogCurLast;
    int             _modeLast;
    bool            _revertBestHash;
    bool            _resetAccelStruct;
    bool            _vsyncSave;
};

#ifdef SHARED_LIB
#define DllExport   extern "C" __declspec(dllexport)
DllExport void raycoinViewer_start();
DllExport bool raycoinViewer_hasAdapter();
DllExport void raycoinViewer_getMinedBlockHash(const char* header_hex, bool& valid, char hash_hex[65]);
DllExport void raycoinViewer_terminate();
#endif