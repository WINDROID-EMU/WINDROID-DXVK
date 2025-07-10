#include "../util/util_math.h"

#include "d3d11_options.h"

namespace dxvk {
  
  static bool IsAPITracingDXGI() {
#ifdef _WIN32
    return !!::GetModuleHandle("dxgitrace.dll");
#else
    return false;
#endif
  }

  // Detecção automática de GPUs Adreno 6xx
  static bool IsAdreno6xx(const std::string& deviceName) {
    return deviceName.find("Adreno 6") != std::string::npos ||
           deviceName.find("Adreno 610") != std::string::npos ||
           deviceName.find("Adreno 620") != std::string::npos ||
           deviceName.find("Adreno 630") != std::string::npos ||
           deviceName.find("Adreno 640") != std::string::npos ||
           deviceName.find("Adreno 650") != std::string::npos ||
           deviceName.find("Adreno 660") != std::string::npos ||
           deviceName.find("Adreno 680") != std::string::npos ||
           deviceName.find("Adreno 690") != std::string::npos;
  }

  D3D11Options::D3D11Options(const Config& config) {
    this->forceVolatileTgsmAccess = config.getOption<bool>("d3d11.forceVolatileTgsmAccess", false);
    this->forceComputeUavBarriers = config.getOption<bool>("d3d11.forceComputeUavBarriers", false);
    this->relaxedBarriers       = config.getOption<bool>("d3d11.relaxedBarriers", false);
    this->relaxedGraphicsBarriers = config.getOption<bool>("d3d11.relaxedGraphicsBarriers", false);
    this->maxTessFactor         = config.getOption<int32_t>("d3d11.maxTessFactor", 0);
    this->samplerAnisotropy     = config.getOption<int32_t>("d3d11.samplerAnisotropy", -1);
    this->samplerLodBias        = config.getOption<float>("d3d11.samplerLodBias", 0.0f);
    this->clampNegativeLodBias  = config.getOption<bool>("d3d11.clampNegativeLodBias", false);
    this->invariantPosition     = config.getOption<bool>("d3d11.invariantPosition", true);
    this->floatControls         = config.getOption<bool>("d3d11.floatControls", true);
    this->forceSampleRateShading = config.getOption<bool>("d3d11.forceSampleRateShading", false);
    this->disableMsaa           = config.getOption<bool>("d3d11.disableMsaa", false);
    this->enableContextLock     = config.getOption<bool>("d3d11.enableContextLock", false);
    this->deferSurfaceCreation  = config.getOption<bool>("dxgi.deferSurfaceCreation", false);
    this->maxFrameLatency       = config.getOption<int32_t>("dxgi.maxFrameLatency", 0);
    this->exposeDriverCommandLists = config.getOption<bool>("d3d11.exposeDriverCommandLists", true);
    this->reproducibleCommandStream = config.getOption<bool>("d3d11.reproducibleCommandStream", false);
    this->disableDirectImageMapping = config.getOption<bool>("d3d11.disableDirectImageMapping", false);
    this->sincosEmulation       = config.getOption<Tristate>("d3d11.sincosEmulation", Tristate::Auto);

    // Clamp LOD bias so that people don't abuse this in unintended ways
    this->samplerLodBias = dxvk::fclamp(this->samplerLodBias, -2.0f, 1.0f);

    auto cachedDynamicResources = config.getOption<std::string>("d3d11.cachedDynamicResources", std::string());

    if (IsAPITracingDXGI()) {
      // apitrace reads back all mapped resources on the CPU, so
      // allocating everything in cached memory is necessary to
      // achieve acceptable performance
      this->cachedDynamicResources = ~0u;
    } else {
      this->cachedDynamicResources = 0u;

      for (char c : cachedDynamicResources) {
        switch (c) {
          case 'c': this->cachedDynamicResources |= D3D11_BIND_CONSTANT_BUFFER;   break;
          case 'v': this->cachedDynamicResources |= D3D11_BIND_VERTEX_BUFFER;     break;
          case 'i': this->cachedDynamicResources |= D3D11_BIND_INDEX_BUFFER;      break;
          case 'r': this->cachedDynamicResources |= D3D11_BIND_SHADER_RESOURCE;   break;
          case 'a': this->cachedDynamicResources  = ~0u;                          break;
          default:  Logger::warn(str::format("Unknown flag for d3d11.cachedDynamicResources option: ", c));
        }
      }
    }

    // Shader dump path is only available via an environment variable
    this->shaderDumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");
    
    // Aplicar otimizações Adreno automaticamente se detectado
    applyAdrenoOptimizations();
  }

  // Aplicar otimizações específicas para Adreno 6xx
  void D3D11Options::applyAdrenoOptimizations() {
    // Detectar GPU Adreno através de variável de ambiente ou configuração
    std::string deviceName = env::getEnvVar("DXVK_GPU_NAME");
    if (deviceName.empty()) {
      // Se não especificado, aplicar otimizações por padrão para mobile
      deviceName = "Adreno 6xx";
    }
    
    if (!IsAdreno6xx(deviceName))
      return;

    Logger::info("DXVK: Applying Adreno 6xx optimizations");

    // Otimizações de memória e cache
    this->cachedDynamicResources = ~0u;  // Cache todos os recursos dinâmicos
    
    // Otimizações de pipeline
    this->relaxedBarriers = true;
    this->relaxedGraphicsBarriers = true;
    this->invariantPosition = true;
    this->floatControls = true;
    
    // Limitar tessellation para mobile
    if (this->maxTessFactor == 0) {
      this->maxTessFactor = 8;
    }
    
    // Otimizações de latência
    if (this->maxFrameLatency == 0) {
      this->maxFrameLatency = 1;
    }
    
    // Desabilitar features pesadas em mobile
    this->forceSampleRateShading = false;
    this->exposeDriverCommandLists = false;
    this->reproducibleCommandStream = false;
    
    Logger::info("DXVK: Adreno 6xx optimizations applied");
  }
  
}
