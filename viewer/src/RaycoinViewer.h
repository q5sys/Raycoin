//Copyright (C) 2018 NewGamePlus Inc. - All Rights Reserved
//Unauthorized copying/distribution of this source code or any derived source code or binaries
//via any medium is strictly prohibited. Proprietary and confidential.

#pragma once

#include "GameCore.h"
#include "GraphicsCore.h"
#include "CameraController.h"
#include "Camera.h"
#include <chrono>

#define RAYCOIN_VERSION_MAJOR 0
#define RAYCOIN_VERSION_MINOR 5

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
        Hash seed;
        Hash target;
        Hash hash;
        ScreenCoord pos;
        float depth;
        Time timestamp;
    };

    RaycoinViewer() {}

    static void selectGPU(int gpu);
    static void start();

    virtual void Startup() override;
    virtual void Cleanup() override;

    virtual void Update(float deltaT) override;
    virtual void RenderScene() override;
    virtual void RenderUI(class GraphicsContext&) override;

    bool hasAdapter() const;
    void seed(const Hash& seed)                 { _seed = seed; }
    void targetHash(const Hash& target)         { _target = target; }
    void verifyPos(const ScreenCoord& pos)      { _verifyPos = pos; }

    const TraceResult& traceResult() const      { return _result; }
    float tracePerSec() const                   { return _tracePerSec; }

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

    RootSignature   _rootSig;
    ComputePSO      _initFieldPSO;
    Hash            _seed;
    Hash            _target;
    ScreenCoord     _verifyPos;
    TraceResult     _result;
    std::vector<TraceResult> _traceLog;
    std::string     _traceLogStatus;
    float           _traceLogStatusTime;
    int             _traceLogCurLast;
    float           _tracePerSec;
    int             _modeLast;
    bool            _revertBestHash;
    bool            _resetAccelStruct;
};
