#include "dxvk_pipeline_cache.h"
#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_shader.h"

namespace dxvk {

  DxvkDynamicPipelineCache::DxvkDynamicPipelineCache(DxvkDevice* device)
  : m_device(device) {
    initializeVulkanCache();
    
    Logger::info("DXVK: Dynamic pipeline cache initialized");
  }

  DxvkDynamicPipelineCache::~DxvkDynamicPipelineCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Destroy all cached pipelines
    for (auto& entry : m_pipelineCache) {
      if (entry.second.pipeline != VK_NULL_HANDLE) {
        auto vk = m_device->vkd();
        vk->vkDestroyPipeline(vk->device(), entry.second.pipeline, nullptr);
      }
    }
    
    // Destroy all cached libraries
    for (auto& entry : m_libraryCache) {
      if (entry.second.library != VK_NULL_HANDLE) {
        auto vk = m_device->vkd();
        vk->vkDestroyPipeline(vk->device(), entry.second.library, nullptr);
      }
    }
    
    destroyVulkanCache();
  }

  VkPipeline DxvkDynamicPipelineCache::getPipeline(
    const DxvkDynamicPipelineStateKey& state,
    const DxvkGraphicsPipelineShaders& shaders) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Try to find existing pipeline
    auto it = m_pipelineCache.find(state);
    if (it != m_pipelineCache.end()) {
      updateUsageStats(it->second);
      m_cacheHits++;
      return it->second.pipeline;
    }
    
    m_cacheMisses++;
    
    // Create new pipeline
    VkPipeline pipeline = createPipeline(state, shaders);
    if (pipeline != VK_NULL_HANDLE) {
      DxvkDynamicPipelineCacheEntry entry;
      entry.pipeline = pipeline;
      entry.lastUsed = m_currentFrame;
      entry.useCount = 1;
      
      m_pipelineCache.emplace(state, std::move(entry));
    }
    
    return pipeline;
  }

  void DxvkDynamicPipelineCache::precompilePipelines(
    const std::vector<DxvkGraphicsPipelineShaders>& shaders) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& shaderSet : shaders) {
      // Create common pipeline state combinations
      std::vector<DxvkDynamicPipelineStateKey> commonStates;
      
      // Basic opaque pipeline
      DxvkDynamicPipelineStateKey opaqueState;
      opaqueState.vertexShaderHash = shaderSet.vs ? shaderSet.vs->getHash() : 0;
      opaqueState.fragmentShaderHash = shaderSet.fs ? shaderSet.fs->getHash() : 0;
      opaqueState.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
      opaqueState.depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
      opaqueState.enableDepthTest = true;
      opaqueState.enableDepthWrite = true;
      commonStates.push_back(opaqueState);
      
      // Alpha blended pipeline
      DxvkDynamicPipelineStateKey blendState = opaqueState;
      blendState.enableBlending = true;
      blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      commonStates.push_back(blendState);
      
      // Pre-compile libraries for these shaders
      createPipelineLibraries(shaderSet);
      
      // Pre-compile common pipeline combinations
      for (const auto& state : commonStates) {
        if (m_pipelineCache.find(state) == m_pipelineCache.end()) {
          VkPipeline pipeline = createPipeline(state, shaderSet);
          if (pipeline != VK_NULL_HANDLE) {
            DxvkDynamicPipelineCacheEntry entry;
            entry.pipeline = pipeline;
            entry.lastUsed = m_currentFrame;
            entry.useCount = 0;
            
            m_pipelineCache.emplace(state, std::move(entry));
          }
        }
      }
    }
  }

  void DxvkDynamicPipelineCache::optimizeCache(uint32_t maxEntries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_pipelineCache.size() <= maxEntries)
      return;
    
    // Sort entries by usage (LRU)
    std::vector<std::pair<const DxvkDynamicPipelineStateKey*, const DxvkDynamicPipelineCacheEntry*>> entries;
    entries.reserve(m_pipelineCache.size());
    
    for (const auto& entry : m_pipelineCache) {
      entries.emplace_back(&entry.first, &entry.second);
    }
    
    std::sort(entries.begin(), entries.end(),
      [](const auto& a, const auto& b) {
        return a.second->lastUsed < b.second->lastUsed;
      });
    
    // Remove least used entries
    uint32_t entriesToRemove = m_pipelineCache.size() - maxEntries;
    auto vk = m_device->vkd();
    
    for (uint32_t i = 0; i < entriesToRemove; i++) {
      const auto& entry = entries[i];
      if (entry.second->pipeline != VK_NULL_HANDLE) {
        vk->vkDestroyPipeline(vk->device(), entry.second->pipeline, nullptr);
      }
      m_pipelineCache.erase(*entry.first);
    }
    
    Logger::info(str::format("DXVK: Optimized pipeline cache, removed ", entriesToRemove, " entries"));
  }

  DxvkDynamicPipelineCache::CacheStats DxvkDynamicPipelineCache::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    CacheStats stats;
    stats.totalPipelines = m_pipelineCache.size();
    stats.totalLibraries = m_libraryCache.size();
    stats.cacheHits = m_cacheHits.load();
    stats.cacheMisses = m_cacheMisses.load();
    
    // Estimate memory usage (rough approximation)
    stats.totalMemoryUsage = (stats.totalPipelines + stats.totalLibraries) * 1024; // ~1KB per pipeline
    
    return stats;
  }

  VkPipeline DxvkDynamicPipelineCache::createPipeline(
    const DxvkDynamicPipelineStateKey& state,
    const DxvkGraphicsPipelineShaders& shaders) {
    
    auto vk = m_device->vkd();
    
    // Create pipeline libraries for shader stages
    std::vector<VkPipeline> libraries = createPipelineLibraries(shaders);
    
    // If we have libraries, link them together
    if (!libraries.empty() && m_device->canUseGraphicsPipelineLibrary()) {
      return linkPipelineLibraries(libraries, state);
    }
    
    // Fallback to monolithic pipeline creation
    DxvkShaderStageInfo stageInfo(m_device);
    
    if (shaders.vs) {
      stageInfo.addStage(VK_SHADER_STAGE_VERTEX_BIT, 
        shaders.vs->getCode(DxvkBindingMap(), DxvkShaderModuleCreateInfo()));
    }
    
    if (shaders.fs) {
      stageInfo.addStage(VK_SHADER_STAGE_FRAGMENT_BIT,
        shaders.fs->getCode(DxvkBindingMap(), DxvkShaderModuleCreateInfo()));
    }
    
    // Create pipeline state
    VkPipelineVertexInputStateCreateInfo viInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo iaInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaInfo.topology = state.topology;
    
    VkPipelineRasterizationStateCreateInfo rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsInfo.polygonMode = state.polygonMode;
    rsInfo.cullMode = state.cullMode;
    rsInfo.frontFace = state.frontFace;
    rsInfo.lineWidth = 1.0f;
    
    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.rasterizationSamples = state.sampleCount;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsInfo.depthTestEnable = state.enableDepthTest;
    dsInfo.depthWriteEnable = state.enableDepthWrite;
    dsInfo.depthCompareOp = state.depthCompareOp;
    
    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable = state.enableBlending;
    cbAttachment.srcColorBlendFactor = state.srcColorBlendFactor;
    cbAttachment.dstColorBlendFactor = state.dstColorBlendFactor;
    cbAttachment.colorBlendOp = state.colorBlendOp;
    cbAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbInfo.attachmentCount = 1;
    cbInfo.pAttachments = &cbAttachment;
    
    // Dynamic rendering info
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    if (state.colorFormat != VK_FORMAT_UNDEFINED) {
      rtInfo.colorAttachmentCount = 1;
      rtInfo.pColorAttachmentFormats = &state.colorFormat;
    }
    if (state.depthFormat != VK_FORMAT_UNDEFINED) {
      rtInfo.depthAttachmentFormat = state.depthFormat;
    }
    
    VkPipelineCreateFlags2CreateInfo flags = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, &rtInfo };
    
    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &flags };
    info.stageCount = stageInfo.getStageCount();
    info.pStages = stageInfo.getStageInfos();
    info.pVertexInputState = &viInfo;
    info.pInputAssemblyState = &iaInfo;
    info.pRasterizationState = &rsInfo;
    info.pMultisampleState = &msInfo;
    info.pDepthStencilState = &dsInfo;
    info.pColorBlendState = &cbInfo;
    info.layout = m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlags(), 0, 0, 0, nullptr)->getPipelineLayout();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), m_vulkanCache, 1, &info, nullptr, &pipeline);
    
    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkDynamicPipelineCache: Failed to create pipeline: ", vr));
      return VK_NULL_HANDLE;
    }
    
    return pipeline;
  }

  std::vector<VkPipeline> DxvkDynamicPipelineCache::createPipelineLibraries(
    const DxvkGraphicsPipelineShaders& shaders) {
    
    std::vector<VkPipeline> libraries;
    
    if (shaders.vs) {
      uint64_t vsHash = shaders.vs->getHash();
      VkPipeline vsLibrary = getPipelineLibrary(vsHash, VK_SHADER_STAGE_VERTEX_BIT, shaders.vs);
      if (vsLibrary != VK_NULL_HANDLE) {
        libraries.push_back(vsLibrary);
      }
    }
    
    if (shaders.fs) {
      uint64_t fsHash = shaders.fs->getHash();
      VkPipeline fsLibrary = getPipelineLibrary(fsHash, VK_SHADER_STAGE_FRAGMENT_BIT, shaders.fs);
      if (fsLibrary != VK_NULL_HANDLE) {
        libraries.push_back(fsLibrary);
      }
    }
    
    return libraries;
  }

  VkPipeline DxvkDynamicPipelineCache::getPipelineLibrary(
    uint64_t shaderHash,
    VkShaderStageFlagBits stage,
    const Rc<DxvkShader>& shader) {
    
    auto it = m_libraryCache.find(shaderHash);
    if (it != m_libraryCache.end()) {
      updateUsageStats(it->second);
      return it->second.library;
    }
    
    VkPipeline library = createPipelineLibrary(stage, shader);
    if (library != VK_NULL_HANDLE) {
      DxvkPipelineLibraryCacheEntry entry;
      entry.library = library;
      entry.stages = stage;
      entry.lastUsed = m_currentFrame;
      entry.useCount = 1;
      entry.shaderHashes.push_back(static_cast<uint32_t>(shaderHash));
      
      m_libraryCache.emplace(shaderHash, std::move(entry));
    }
    
    return library;
  }

  VkPipeline DxvkDynamicPipelineCache::createPipelineLibrary(
    VkShaderStageFlagBits stage,
    const Rc<DxvkShader>& shader) {
    
    if (!m_device->canUseGraphicsPipelineLibrary()) {
      return VK_NULL_HANDLE;
    }
    
    auto vk = m_device->vkd();
    
    DxvkShaderStageInfo stageInfo(m_device);
    stageInfo.addStage(stage, shader->getCode(DxvkBindingMap(), DxvkShaderModuleCreateInfo()));
    
    VkPipelineCreateFlags2CreateInfo flags = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO };
    flags.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR;
    
    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &flags };
    
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
        break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
        libInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
        break;
      default:
        return VK_NULL_HANDLE;
    }
    
    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.stageCount = stageInfo.getStageCount();
    info.pStages = stageInfo.getStageInfos();
    info.layout = m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlags(), 0, 0, 0, nullptr)->getPipelineLayout();
    
    VkPipeline library = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), m_vulkanCache, 1, &info, nullptr, &library);
    
    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkDynamicPipelineCache: Failed to create pipeline library: ", vr));
      return VK_NULL_HANDLE;
    }
    
    return library;
  }

  VkPipeline DxvkDynamicPipelineCache::linkPipelineLibraries(
    const std::vector<VkPipeline>& libraries,
    const DxvkDynamicPipelineStateKey& state) {
    
    auto vk = m_device->vkd();
    
    VkPipelineCreateFlags2CreateInfo flags = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO };
    
    VkPipelineLibraryCreateInfoKHR libInfo = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, &flags };
    libInfo.libraryCount = libraries.size();
    libInfo.pLibraries = libraries.data();
    
    // Create pipeline state for linking
    VkPipelineVertexInputStateCreateInfo viInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo iaInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaInfo.topology = state.topology;
    
    VkPipelineRasterizationStateCreateInfo rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsInfo.polygonMode = state.polygonMode;
    rsInfo.cullMode = state.cullMode;
    rsInfo.frontFace = state.frontFace;
    rsInfo.lineWidth = 1.0f;
    
    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.rasterizationSamples = state.sampleCount;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsInfo.depthTestEnable = state.enableDepthTest;
    dsInfo.depthWriteEnable = state.enableDepthWrite;
    dsInfo.depthCompareOp = state.depthCompareOp;
    
    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable = state.enableBlending;
    cbAttachment.srcColorBlendFactor = state.srcColorBlendFactor;
    cbAttachment.dstColorBlendFactor = state.dstColorBlendFactor;
    cbAttachment.colorBlendOp = state.colorBlendOp;
    cbAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbInfo.attachmentCount = 1;
    cbInfo.pAttachments = &cbAttachment;
    
    // Dynamic rendering info
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    if (state.colorFormat != VK_FORMAT_UNDEFINED) {
      rtInfo.colorAttachmentCount = 1;
      rtInfo.pColorAttachmentFormats = &state.colorFormat;
    }
    if (state.depthFormat != VK_FORMAT_UNDEFINED) {
      rtInfo.depthAttachmentFormat = state.depthFormat;
    }
    
    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.pVertexInputState = &viInfo;
    info.pInputAssemblyState = &iaInfo;
    info.pRasterizationState = &rsInfo;
    info.pMultisampleState = &msInfo;
    info.pDepthStencilState = &dsInfo;
    info.pColorBlendState = &cbInfo;
    info.layout = m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlags(), 0, 0, 0, nullptr)->getPipelineLayout();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), m_vulkanCache, 1, &info, nullptr, &pipeline);
    
    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkDynamicPipelineCache: Failed to link pipeline libraries: ", vr));
      return VK_NULL_HANDLE;
    }
    
    return pipeline;
  }

  void DxvkDynamicPipelineCache::updateUsageStats(DxvkDynamicPipelineCacheEntry& entry) {
    entry.lastUsed = m_currentFrame;
    entry.useCount++;
  }

  void DxvkDynamicPipelineCache::updateUsageStats(DxvkPipelineLibraryCacheEntry& entry) {
    entry.lastUsed = m_currentFrame;
    entry.useCount++;
  }

  void DxvkDynamicPipelineCache::cleanupUnusedEntries() {
    const uint64_t maxAge = 1000; // Remove entries older than 1000 frames
    
    auto it = m_pipelineCache.begin();
    while (it != m_pipelineCache.end()) {
      if (m_currentFrame - it->second.lastUsed > maxAge && it->second.useCount < 5) {
        if (it->second.pipeline != VK_NULL_HANDLE) {
          auto vk = m_device->vkd();
          vk->vkDestroyPipeline(vk->device(), it->second.pipeline, nullptr);
        }
        it = m_pipelineCache.erase(it);
      } else {
        ++it;
      }
    }
    
    auto libIt = m_libraryCache.begin();
    while (libIt != m_libraryCache.end()) {
      if (m_currentFrame - libIt->second.lastUsed > maxAge && libIt->second.useCount < 3) {
        if (libIt->second.library != VK_NULL_HANDLE) {
          auto vk = m_device->vkd();
          vk->vkDestroyPipeline(vk->device(), libIt->second.library, nullptr);
        }
        libIt = m_libraryCache.erase(libIt);
      } else {
        ++libIt;
      }
    }
  }

  void DxvkDynamicPipelineCache::initializeVulkanCache() {
    auto vk = m_device->vkd();
    
    VkPipelineCacheCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    createInfo.initialDataSize = 0;
    createInfo.pInitialData = nullptr;
    
    VkResult vr = vk->vkCreatePipelineCache(vk->device(), &createInfo, nullptr, &m_vulkanCache);
    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkDynamicPipelineCache: Failed to create Vulkan pipeline cache: ", vr));
    }
  }

  void DxvkDynamicPipelineCache::destroyVulkanCache() {
    if (m_vulkanCache != VK_NULL_HANDLE) {
      auto vk = m_device->vkd();
      vk->vkDestroyPipelineCache(vk->device(), m_vulkanCache, nullptr);
      m_vulkanCache = VK_NULL_HANDLE;
    }
  }

} 