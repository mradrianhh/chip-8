#ifndef GRAPHICS_VULKAN_H
#define GRAPHICS_VULKAN_H

#include <logger/logger.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include "cglm/cglm.h"

#ifdef CH8_SHADERS_DIR
#define SHADERS_BASE_PATH CH8_SHADERS_DIR
#else
#define SHADERS_BASE_PATH "../../assets/shaders/"
#endif

#define WINDOW_WIDTH (800)
#define WINDOW_HEIGHT (600)
#define MAX_FRAMES_IN_FLIGHT (2)

// Calls VK-function and checks VkResult. Raises SIGABRT if error.
#define CALL_VK(func, logger_ptr, fmt, ...)        \
    do                                 \
    {                                  \
        if ((func) != VK_SUCCESS)      \
        {                              \
            PANIC(logger_ptr, fmt, ##__VA_ARGS__); \
        }                              \
    } while (false)

#define PANIC(logger_ptr, fmt, ...)                           \
    do                                            \
    {                                             \
        logger_LogError((logger_ptr), fmt, ##__VA_ARGS__); \
        raise(SIGABRT);                           \
    } while (false)

typedef struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t formats_count;
    VkSurfaceFormatKHR *formats;
    uint32_t present_modes_count;
    VkPresentModeKHR *present_modes;
} SwapChainSupportDetails;

typedef struct GraphicsContext
{
    // Reference to logger in application.
    // We do not need to free this logger as we do not own it.
    Logger *logger;
    GLFWwindow *window;
    VkInstance instance;
    bool enable_validation_layers;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    uint32_t graphicsQueueFamilyIdx;
    VkQueue graphicsQueue;
    uint32_t presentQueueFamilyIdx;
    VkQueue presentQueue;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;
    VkImage *swapChainImages;
    uint32_t swapChainImages_count;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkImageView *swapChainImageViews;
    uint32_t swapChainImageViews_count;
    VkFramebuffer *swapChainFramebuffers;
    uint32_t swapChainFramebuffers_count;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;

    VkImage textureImage;
    VkImageView textureImageView;
    VkDeviceMemory textureImageMemory;
    VkSampler textureSampler;

    VkSemaphore *imageAvailableSemaphores;
    VkSemaphore *renderFinishedSemaphores;
    VkFence *inFlightFences;
    uint32_t currentFrame;

    bool frameBufferResized;
} GraphicsContext;

GraphicsContext *gfx_CreateGraphicsContext(Logger *logger);

void gfx_DestroyGraphicsContext(GraphicsContext *ctx);

void gfx_DrawFrame(GraphicsContext *ctx);

void gfx_StopGraphicsContext(GraphicsContext *ctx);

#endif
