#pragma once

#include "../util/util_math.h"
#include "../util/config/config.h"

#include "../dxgi/dxgi_options.h"

#include "../dxvk/dxvk_device.h"

#include "d3d11_include.h"

namespace dxvk {

  struct D3D11Options {
    D3D11Options(const Config& config);

    // Shader-related options
    bool forceVolatileTgsmAccess;
    bool forceComputeUavBarriers;
    bool relaxedBarriers;
    bool relaxedGraphicsBarriers;
    int32_t maxTessFactor;
    int32_t samplerAnisotropy;
    float samplerLodBias;
    bool clampNegativeLodBias;
    bool invariantPosition;
    bool floatControls;
    bool forceSampleRateShading;
    bool disableMsaa;
    bool enableContextLock;
    bool deferSurfaceCreation;
    int32_t maxFrameLatency;
    bool exposeDriverCommandLists;
    bool reproducibleCommandStream;
    bool disableDirectImageMapping;
    Tristate sincosEmulation;

    // Memory management
    uint32_t cachedDynamicResources;
    std::string shaderDumpPath;

    // Aplicar otimizações específicas para Adreno 6xx
    void applyAdrenoOptimizations();
  };
  
}
