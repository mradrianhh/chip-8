#include <signal.h>

#include "graphics/internal.h"

static void InitGLFW(GraphicsContext *ctx);
static void InitVulkan(GraphicsContext *ctx);

static void CleanUpSwapchain(GraphicsContext *ctx);
static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);

static void CreateInstance(GraphicsContext *ctx);
static void SetupDebugMessenger(GraphicsContext *ctx);
static void CreateSurface(GraphicsContext *ctx);
static void SelectPhysicalDevice(GraphicsContext *ctx);
static void CreateLogicalDevice(GraphicsContext *ctx);
static void CreateSwapChain(GraphicsContext *ctx);
static void CreateImageViews(GraphicsContext *ctx);
static void CreateRenderPass(GraphicsContext *ctx);
static void CreateGraphicsPipeline(GraphicsContext *ctx);
static void CreateFramebuffers(GraphicsContext *ctx);
static void CreateCommandPool(GraphicsContext *ctx);
static void CreateCommandBuffers(GraphicsContext *ctx);
static void CreateSyncObjects(GraphicsContext *ctx);

static void glfwErrorCallback(int error_code, const char *description);
static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

GraphicsContext *gfx_CreateGraphicsContext(Logger *logger)
{
    GraphicsContext *ctx = calloc(1, sizeof(GraphicsContext));
    ctx->logger = logger;

#ifdef NDEBUG
    ctx->enable_validation_layers = false;
#else
    ctx->enable_validation_layers = true;
#endif

    InitGLFW(ctx);
    InitVulkan(ctx);

    return ctx;
}

void gfx_DestroyGraphicsContext(GraphicsContext *ctx)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(ctx->device, ctx->imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(ctx->device, ctx->renderFinishedSemaphores[i], NULL);
        vkDestroyFence(ctx->device, ctx->inFlightFences[i], NULL);
    }

    vkDestroyCommandPool(ctx->device, ctx->commandPool, NULL);

    CleanUpSwapchain(ctx);

    vkDestroySampler(ctx->device, ctx->textureSampler, NULL);
    vkDestroyImageView(ctx->device, ctx->textureImageView, NULL);
    vkDestroyImage(ctx->device, ctx->textureImage, NULL);
    vkFreeMemory(ctx->device, ctx->textureImageMemory, NULL);

    vkDestroyPipeline(ctx->device, ctx->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(ctx->device, ctx->pipelineLayout, NULL);
    vkDestroyRenderPass(ctx->device, ctx->renderPass, NULL);

    vkDestroyDevice(ctx->device, NULL);

    if (ctx->enable_validation_layers)
    {
        DestroyDebugUtilsMessengerEXT(ctx->instance, ctx->debugMessenger, NULL);
    }

    vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    vkDestroyInstance(ctx->instance, NULL);
    glfwDestroyWindow(ctx->window);

    glfwTerminate();

    free(ctx);
}

void InitGLFW(GraphicsContext *ctx)
{
    glfwSetErrorCallback(glfwErrorCallback);

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    ctx->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Chip-8", NULL, NULL);
    glfwSetWindowUserPointer(ctx->window, ctx);
    glfwSetFramebufferSizeCallback(ctx->window, framebufferResizeCallback);
}

void InitVulkan(GraphicsContext *ctx)
{
    CreateInstance(ctx);
    SetupDebugMessenger(ctx);
    CreateSurface(ctx);
    SelectPhysicalDevice(ctx);
    CreateLogicalDevice(ctx);
    CreateSwapChain(ctx);
    CreateImageViews(ctx);
    CreateRenderPass(ctx);
    CreateGraphicsPipeline(ctx);
    CreateFramebuffers(ctx);
    CreateCommandPool(ctx);
    CreateCommandBuffers(ctx);
    CreateSyncObjects(ctx);
}

void CleanUpSwapchain(GraphicsContext *ctx)
{
    for (size_t i = 0; i < ctx->swapChainFramebuffers_count; i++)
    {
        vkDestroyFramebuffer(ctx->device, ctx->swapChainFramebuffers[i], NULL);
    }

    for (size_t i = 0; i < ctx->swapChainImageViews_count; i++)
    {
        vkDestroyImageView(ctx->device, ctx->swapChainImageViews[i], NULL);
    }

    vkDestroySwapchainKHR(ctx->device, ctx->swapChain, NULL);
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

void CreateInstance(GraphicsContext *ctx)
{
}

void SetupDebugMessenger(GraphicsContext *ctx)
{
}

void CreateSurface(GraphicsContext *ctx)
{
}

void SelectPhysicalDevice(GraphicsContext *ctx)
{
}

void CreateLogicalDevice(GraphicsContext *ctx)
{
}

void CreateSwapChain(GraphicsContext *ctx)
{
}

void CreateImageViews(GraphicsContext *ctx)
{
}

void CreateRenderPass(GraphicsContext *ctx)
{
}

void CreateGraphicsPipeline(GraphicsContext *ctx)
{
}

void CreateFramebuffers(GraphicsContext *ctx)
{
}

void CreateCommandPool(GraphicsContext *ctx)
{
}

void CreateCommandBuffers(GraphicsContext *ctx)
{
}

void CreateSyncObjects(GraphicsContext *ctx)
{
}

void glfwErrorCallback(int error_code, const char *description)
{
    GLFWwindow *window = glfwGetCurrentContext();
    GraphicsContext *ctx = glfwGetWindowUserPointer(window);
    PANIC(ctx->logger, "GLFW(RC=%d) - %s.", error_code, description);
}

void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    GraphicsContext *ctx = glfwGetWindowUserPointer(window);
    ctx->frameBufferResized = true;
}
