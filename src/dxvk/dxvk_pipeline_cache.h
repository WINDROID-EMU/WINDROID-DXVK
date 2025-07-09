#pragma once

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>

#include "dxvk_hash.h"
#include "dxvk_util.h"
#include "dxvk_device.h"
#include "dxvk_pipemanager.h"

namespace dxvk {

  /**
   * \brief Pipeline cache entry for dynamic rendering
   * 
   * Stores a cached pipeline with its associated state
   * and usage statistics for optimization.
   */
  struct DxvkDynamicPipelineCacheEntry {
    VkPipeline                    pipeline = VK_NULL_HANDLE;
    VkPipelineCache              cache = VK_NULL_HANDLE;
    uint64_t                     lastUsed = 0;
    uint32_t                     useCount = 0;
    bool                         isLibrary = false;
    std::vector<VkPipeline>      libraries;
  };

  /**
   * \brief Dynamic rendering pipeline state key
   * 
   * Compact representation of pipeline state for caching
   */
  struct DxvkDynamicPipelineStateKey {
    // Shader stages
    uint64_t                     vertexShaderHash = 0;
    uint64_t                     fragmentShaderHash = 0;
    uint64_t                     geometryShaderHash = 0;
    uint64_t                     tessellationShaderHash = 0;
    
    // Render state
    VkFormat                     colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat                     depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits        sampleCount = VK_SAMPLE_COUNT_1_BIT;
    
    // Pipeline state
    VkPrimitiveTopology          topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPolygonMode                polygonMode = VK_POLYGON_MODE_FILL;
    VkCullModeFlags              cullMode = VK_CULL_MODE_NONE_BIT;
    VkFrontFace                  frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    
    // Blend state
    VkBlendFactor                srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    VkBlendFactor                dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    VkBlendOp                    colorBlendOp = VK_BLEND_OP_ADD;
    bool                         enableBlending = false;
    
    // Depth/stencil state
    bool                         enableDepthTest = false;
    bool                         enableDepthWrite = false;
    VkCompareOp                  depthCompareOp = VK_COMPARE_OP_LESS;
    
    // Dynamic state flags
    uint32_t                     dynamicStateMask = 0;
    
    bool operator==(const DxvkDynamicPipelineStateKey& other) const {
      return vertexShaderHash == other.vertexShaderHash
          && fragmentShaderHash == other.fragmentShaderHash
          && geometryShaderHash == other.geometryShaderHash
          && tessellationShaderHash == other.tessellationShaderHash
          && colorFormat == other.colorFormat
          && depthFormat == other.depthFormat
          && sampleCount == other.sampleCount
          && topology == other.topology
          && polygonMode == other.polygonMode
          && cullMode == other.cullMode
          && frontFace == other.frontFace
          && srcColorBlendFactor == other.srcColorBlendFactor
          && dstColorBlendFactor == other.dstColorBlendFactor
          && colorBlendOp == other.colorBlendOp
          && enableBlending == other.enableBlending
          && enableDepthTest == other.enableDepthTest
          && enableDepthWrite == other.enableDepthWrite
          && depthCompareOp == other.depthCompareOp
          && dynamicStateMask == other.dynamicStateMask;
    }
    
    size_t hash() const {
      DxvkHashState hash;
      hash.add(vertexShaderHash);
      hash.add(fragmentShaderHash);
      hash.add(geometryShaderHash);
      hash.add(tessellationShaderHash);
      hash.add(uint32_t(colorFormat));
      hash.add(uint32_t(depthFormat));
      hash.add(uint32_t(sampleCount));
      hash.add(uint32_t(topology));
      hash.add(uint32_t(polygonMode));
      hash.add(uint32_t(cullMode));
      hash.add(uint32_t(frontFace));
      hash.add(uint32_t(srcColorBlendFactor));
      hash.add(uint32_t(dstColorBlendFactor));
      hash.add(uint32_t(colorBlendOp));
      hash.add(enableBlending);
      hash.add(enableDepthTest);
      hash.add(enableDepthWrite);
      hash.add(uint32_t(depthCompareOp));
      hash.add(dynamicStateMask);
      return hash;
    }
  };

  /**
   * \brief Pipeline library cache entry
   * 
   * Stores reusable pipeline libraries for dynamic rendering
   */
  struct DxvkPipelineLibraryCacheEntry {
    VkPipeline                    library = VK_NULL_HANDLE;
    VkShaderStageFlags            stages = 0;
    uint64_t                     lastUsed = 0;
    uint32_t                     useCount = 0;
    std::vector<uint32_t>        shaderHashes;
  };

  /**
   * \brief Optimized pipeline cache for dynamic rendering
   * 
   * Implements an advanced caching system that uses pipeline libraries
   * to minimize pipeline creation overhead and improve performance.
   */
  class DxvkDynamicPipelineCache {
  public:
    DxvkDynamicPipelineCache(DxvkDevice* device);
    ~DxvkDynamicPipelineCache();

    /**
     * \brief Gets or creates a pipeline for dynamic rendering
     * 
     * \param [in] state Pipeline state
     * \param [in] shaders Shader stages
     * \returns Pipeline handle
     */
    VkPipeline getPipeline(
      const DxvkDynamicPipelineStateKey& state,
      const DxvkGraphicsPipelineShaders& shaders);

    /**
     * \brief Pre-compiles common pipeline combinations
     * 
     * \param [in] shaders Common shader combinations
     */
    void precompilePipelines(
      const std::vector<DxvkGraphicsPipelineShaders>& shaders);

    /**
     * \brief Optimizes cache by removing least used entries
     * 
     * \param [in] maxEntries Maximum number of entries to keep
     */
    void optimizeCache(uint32_t maxEntries = 1000);

    /**
     * \brief Gets cache statistics
     * 
     * \returns Cache statistics
     */
    struct CacheStats {
      uint32_t totalPipelines = 0;
      uint32_t totalLibraries = 0;
      uint32_t cacheHits = 0;
      uint32_t cacheMisses = 0;
      uint64_t totalMemoryUsage = 0;
    };
    
    CacheStats getStats() const;

  private:
    DxvkDevice*                   m_device;
    mutable std::mutex            m_mutex;
    
    // Pipeline cache
    std::unordered_map<
      DxvkDynamicPipelineStateKey,
      DxvkDynamicPipelineCacheEntry,
      DxvkHash,
      DxvkEq>                     m_pipelineCache;
    
    // Library cache
    std::unordered_map<
      uint64_t,
      DxvkPipelineLibraryCacheEntry,
      DxvkHash,
      DxvkEq>                     m_libraryCache;
    
    // Statistics
    mutable std::atomic<uint32_t> m_cacheHits = { 0 };
    mutable std::atomic<uint32_t> m_cacheMisses = { 0 };
    
    // Pipeline cache object
    VkPipelineCache              m_vulkanCache = VK_NULL_HANDLE;
    
    // Current frame counter for LRU
    uint64_t                     m_currentFrame = 0;

    /**
     * \brief Creates a new pipeline
     * 
     * \param [in] state Pipeline state
     * \param [in] shaders Shader stages
     * \returns Pipeline handle
     */
    VkPipeline createPipeline(
      const DxvkDynamicPipelineStateKey& state,
      const DxvkGraphicsPipelineShaders& shaders);

    /**
     * \brief Creates pipeline libraries for shader stages
     * 
     * \param [in] shaders Shader stages
     * \returns Vector of pipeline libraries
     */
    std::vector<VkPipeline> createPipelineLibraries(
      const DxvkGraphicsPipelineShaders& shaders);

    /**
     * \brief Gets or creates a pipeline library
     * 
     * \param [in] shaderHash Shader hash
     * \param [in] stage Shader stage
     * \param [in] shader Shader object
     * \returns Pipeline library handle
     */
    VkPipeline getPipelineLibrary(
      uint64_t shaderHash,
      VkShaderStageFlagBits stage,
      const Rc<DxvkShader>& shader);

    /**
     * \brief Creates a pipeline library from shader
     * 
     * \param [in] stage Shader stage
     * \param [in] shader Shader object
     * \returns Pipeline library handle
     */
    VkPipeline createPipelineLibrary(
      VkShaderStageFlagBits stage,
      const Rc<DxvkShader>& shader);

    /**
     * \brief Links pipeline libraries into a complete pipeline
     * 
     * \param [in] libraries Pipeline libraries
     * \param [in] state Pipeline state
     * \returns Complete pipeline handle
     */
    VkPipeline linkPipelineLibraries(
      const std::vector<VkPipeline>& libraries,
      const DxvkDynamicPipelineStateKey& state);

    /**
     * \brief Updates usage statistics
     * 
     * \param [in] entry Cache entry to update
     */
    void updateUsageStats(DxvkDynamicPipelineCacheEntry& entry);

    /**
     * \brief Cleans up unused cache entries
     */
    void cleanupUnusedEntries();

    /**
     * \brief Initializes Vulkan pipeline cache
     */
    void initializeVulkanCache();

    /**
     * \brief Destroys Vulkan pipeline cache
     */
    void destroyVulkanCache();
  };

} 