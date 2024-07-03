#include <signal.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <maths/maths.h>
#include <core/keys.h>

#include "graphio/graphio.h"

#ifdef CH8_PNGS_DIR
#define PNGS_BASE_PATH CH8_PNGS_DIR
#else
#define PNGS_BASE_PATH "../../assets/pngs/"
#endif

static const uint32_t device_extensions_count = 1;
static const char *device_extensions[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static const uint32_t validation_layers_count = 1;
static const char *validation_layers[1] = {"VK_LAYER_KHRONOS_validation"};

static void InitGLFW(GraphioContext *ctx);
static void InitVulkan(GraphioContext *ctx);

static void CreateInstance(GraphioContext *ctx);
static void SetupDebugMessenger(GraphioContext *ctx);
static void CreateSurface(GraphioContext *ctx);
static void SelectPhysicalDevice(GraphioContext *ctx);
static void CreateLogicalDevice(GraphioContext *ctx);
static void CreateSwapChain(GraphioContext *ctx);
static void CreateImageViews(GraphioContext *ctx);
static void CreateRenderPass(GraphioContext *ctx);
static void CreateDescriptorSetLayout(GraphioContext *ctx);
static void CreateGraphicsPipeline(GraphioContext *ctx);
static void CreateFramebuffers(GraphioContext *ctx);
static void CreateCommandPool(GraphioContext *ctx);
static void CreateTextureImage(GraphioContext *ctx);
static void CreateTextureImageView(GraphioContext *ctx);
static void CreateTextureSampler(GraphioContext *ctx);
static void CreateDescriptorPool(GraphioContext *ctx);
static void CreateDescriptorSets(GraphioContext *ctx);
static void CreateCommandBuffers(GraphioContext *ctx);
static void CreateSyncObjects(GraphioContext *ctx);

// Debug/Extensions

static bool CheckValidationLayerSupport(GraphioContext *ctx);
static char **GetRequiredExtensions(GraphioContext *ctx, uint32_t *count);
static void PopulateDebugMessengerCreateInfo(Logger *logger, VkDebugUtilsMessengerCreateInfoEXT *createInfo);
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);

// Swapchain

static void CleanUpSwapchain(GraphioContext *ctx);
static void RecreateSwapChain(GraphioContext *ctx);
static SwapChainSupportDetails QuerySwapChainSupport(GraphioContext *ctx);
static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(SwapChainSupportDetails *support);
static VkPresentModeKHR ChooseSwapPresentMode(SwapChainSupportDetails *support);
static VkExtent2D ChooseSwapExtent(GraphioContext *ctx, SwapChainSupportDetails *support);
static void DestroySwapChainSupportDetails(SwapChainSupportDetails details);

// Image/Imageview

static VkImageView CreateImageView(GraphioContext *ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
static void CreateImage(GraphioContext *ctx, uint32_t width, uint32_t height, VkFormat format,
                        VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                        VkImage *image, VkDeviceMemory *imageMemory);
static void CopyBufferToImage(GraphioContext *ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
static void TransitionImageLayout(GraphioContext *ctx, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

// Memory/Buffers

static uint32_t FindMemoryType(GraphioContext *ctx, uint32_t type_filter, VkMemoryPropertyFlags properties);
static void CreateBuffer(GraphioContext *ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory);

// Shaders

static void ReadShaderFile(Logger *logger, const char *filename, char **data, size_t *size);
static void CreateShaderModule(GraphioContext *ctx, const char *code, size_t size, VkShaderModule *module);

// Commands

static void RecordCommandBuffer(GraphioContext *ctx, uint32_t image_index);
static VkCommandBuffer BeginSingleTimeCommands(GraphioContext *ctx);
static void EndSingleTimeCommands(GraphioContext *ctx, VkCommandBuffer commandBuffer);

// Keys

static void SetKeyPressed(GraphioContext *ctx, int key);
static void SetKeyReleased(GraphioContext *ctx, int key);

// Callbacks

static void glfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
static void glfwErrorCallback(int error_code, const char *description);
static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

GraphioContext *gio_CreateGraphioContext(Logger *logger, Display *display, uint16_t *keys)
{
    GraphioContext *ctx = calloc(1, sizeof(GraphioContext));
    ctx->logger = logger;

#ifdef NDEBUG
    ctx->enable_validation_layers = false;
#else
    ctx->enable_validation_layers = true;
#endif

    ctx->commandBuffers = realloc(ctx->commandBuffers, sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
    ctx->physicalDevice = VK_NULL_HANDLE;
    ctx->imageAvailableSemaphores = realloc(ctx->imageAvailableSemaphores, sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    ctx->renderFinishedSemaphores = realloc(ctx->renderFinishedSemaphores, sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    ctx->inFlightFences = realloc(ctx->inFlightFences, sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    ctx->currentFrame = 0;
    ctx->graphicsQueueFamilyIdx = UINT32_MAX;
    ctx->presentQueueFamilyIdx = UINT32_MAX;
    ctx->descriptorSets = realloc(ctx->descriptorSets, sizeof(VkDescriptorSet) * MAX_FRAMES_IN_FLIGHT);

    ctx->display = display;
    ctx->keys = keys;

    InitGLFW(ctx);
    InitVulkan(ctx);

    return ctx;
}

void gio_DestroyGraphioContext(GraphioContext *ctx)
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(ctx->device, ctx->imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(ctx->device, ctx->renderFinishedSemaphores[i], NULL);
        vkDestroyFence(ctx->device, ctx->inFlightFences[i], NULL);
    }

    vkDestroyCommandPool(ctx->device, ctx->commandPool, NULL);

    CleanUpSwapchain(ctx);

    vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, NULL);

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

void gio_UpdateTexture(GraphioContext *ctx)
{
    // Steps to dynamically update texture:
    // ------------------------------------
    //
    // Before render loop:
    // 1. Create a VkImage with VK_IMAGE_LAYOUT_UNDEFINED.
    // 2. Set up staging buffer.
    // 3. Map the staging buffer memory.
    //
    // In render loop:
    // 1. Transfer from staging buffer to device local memory(?).
    //
    // Clean up.
    // 1. Unmap memory.
    // 2. Clean up image/memory/imageview.
}

void gio_DrawFrame(GraphioContext *ctx)
{
    vkWaitForFences(ctx->device, 1, &ctx->inFlightFences[ctx->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(ctx->device, ctx->swapChain, UINT64_MAX,
                                            ctx->imageAvailableSemaphores[ctx->currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain(ctx);
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        PANIC(ctx->logger, "Failed to acquire swap chain image.");
    }

    vkResetFences(ctx->device, 1, &ctx->inFlightFences[ctx->currentFrame]);

    vkResetCommandBuffer(ctx->commandBuffers[ctx->currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordCommandBuffer(ctx, imageIndex);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {ctx->imageAvailableSemaphores[ctx->currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &ctx->commandBuffers[ctx->currentFrame];

    VkSemaphore signalSemaphores[] = {ctx->renderFinishedSemaphores[ctx->currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, ctx->inFlightFences[ctx->currentFrame]) != VK_SUCCESS)
    {
        PANIC(ctx->logger, "Failed to submit draw command buffer.");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {ctx->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(ctx->presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || ctx->frameBufferResized)
    {
        ctx->frameBufferResized = false;
        RecreateSwapChain(ctx);
    }
    else if (result != VK_SUCCESS)
    {
        PANIC(ctx->logger, "Failed to present swap chain image.");
    }

    ctx->currentFrame = (ctx->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void gio_UpdateFPS(GraphioContext *ctx, double fps)
{
    char title[256];
    title[255] = '\0';

    snprintf(title, 255, "FPS: %3.2f", fps);
    glfwSetWindowTitle(ctx->window, title);
}

double gio_GetCurrentTime()
{
    return glfwGetTime();
}

void gio_StopGraphioContext(GraphioContext *ctx)
{
    CALL_VK(vkDeviceWaitIdle(ctx->device), ctx->logger, "Failed while waiting for device to go idle.");
}

void InitGLFW(GraphioContext *ctx)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    ctx->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Chip-8", NULL, NULL);
    glfwSetWindowUserPointer(ctx->window, ctx);

    glfwSetFramebufferSizeCallback(ctx->window, framebufferResizeCallback);
    glfwSetErrorCallback(glfwErrorCallback);
    glfwSetKeyCallback(ctx->window, glfwKeyCallback);
}

void InitVulkan(GraphioContext *ctx)
{
    CreateInstance(ctx);
    SetupDebugMessenger(ctx);
    CreateSurface(ctx);
    SelectPhysicalDevice(ctx);
    CreateLogicalDevice(ctx);
    CreateSwapChain(ctx);
    CreateImageViews(ctx);
    CreateRenderPass(ctx);
    CreateDescriptorSetLayout(ctx);
    CreateGraphicsPipeline(ctx);
    CreateFramebuffers(ctx);
    CreateCommandPool(ctx);
    CreateTextureImage(ctx);
    CreateTextureImageView(ctx);
    CreateTextureSampler(ctx);
    CreateDescriptorPool(ctx);
    CreateDescriptorSets(ctx);
    CreateCommandBuffers(ctx);
    CreateSyncObjects(ctx);
}

void CreateInstance(GraphioContext *ctx)
{
    if (ctx->enable_validation_layers && !CheckValidationLayerSupport(ctx))
    {
        PANIC(ctx->logger, "Validation layers requested, but not available.");
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_0,
    };

    uint32_t extensions_count;
    char **extensions = GetRequiredExtensions(ctx, &extensions_count);
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensions_count,
        .ppEnabledExtensionNames = extensions,
    };

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    if (ctx->enable_validation_layers)
    {
        createInfo.enabledLayerCount = validation_layers_count;
        createInfo.ppEnabledLayerNames = validation_layers;

        PopulateDebugMessengerCreateInfo(ctx->logger, &debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = NULL;
    }

    CALL_VK(vkCreateInstance(&createInfo, NULL, &ctx->instance), ctx->logger, "Failed to create instance!");
}

void SetupDebugMessenger(GraphioContext *ctx)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    PopulateDebugMessengerCreateInfo(ctx->logger, &createInfo);

    CALL_VK(CreateDebugUtilsMessengerEXT(ctx->instance, &createInfo, NULL, &ctx->debugMessenger),
            ctx->logger, "Failed to set up debug messenger.");
}

void CreateSurface(GraphioContext *ctx)
{
    // Using CALL_VK on GLFW function because it returns VkResult.
    CALL_VK(glfwCreateWindowSurface(ctx->instance, ctx->window, NULL, &ctx->surface),
            ctx->logger, "Failed to create window surface!");
}

void SelectPhysicalDevice(GraphioContext *ctx)
{
    // First fetch to get count.
    uint32_t device_count = 0;
    CALL_VK(vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL),
            ctx->logger, "Failed to enumerate physical devices.");

    if (device_count == 0)
        PANIC(ctx->logger, "Failed to find GPUs with Vulkan support!");

    // Then allocate a suitable array and fetch the actual devices.
    VkPhysicalDevice devices[device_count];
    CALL_VK(vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices),
            ctx->logger, "Failed to enumerate physical devices.");

    // Select first device as we don't require any special suitability right now.
    ctx->physicalDevice = devices[0];
}

void CreateLogicalDevice(GraphioContext *ctx)
{
    // Get properties of all queue families on physical device.
    uint32_t properties_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physicalDevice, &properties_count, NULL);
    if (properties_count == 0)
    {
        PANIC(ctx->logger, "Found no queue-family properties.");
    }
    VkQueueFamilyProperties properties[properties_count];
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physicalDevice, &properties_count, properties);

    // Find out which queue family supports graphics and presentation.
    for (uint32_t i = 0; i < properties_count; i++)
    {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            ctx->graphicsQueueFamilyIdx = i;
        }

        VkBool32 present_support = false;
        CALL_VK(vkGetPhysicalDeviceSurfaceSupportKHR(ctx->physicalDevice, i, ctx->surface, &present_support),
                ctx->logger, "Failed to get physical device surface support.");
        if (present_support)
        {
            ctx->presentQueueFamilyIdx = i;
        }

        if (ctx->graphicsQueueFamilyIdx != UINT32_MAX && ctx->presentQueueFamilyIdx != UINT32_MAX)
            break;
    }

    if (ctx->graphicsQueueFamilyIdx == UINT32_MAX || ctx->presentQueueFamilyIdx == UINT32_MAX)
        PANIC(ctx->logger, "Failed to find suitable queue family.");

    // Create array of QueueCreateInfo's and populate it.
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo *queue_create_infos = calloc(1, 2 * sizeof(VkDeviceQueueCreateInfo));
    queue_create_infos[0] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->graphicsQueueFamilyIdx,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[1] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->presentQueueFamilyIdx,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    VkPhysicalDeviceFeatures device_features = {};
    device_features.samplerAnisotropy = VK_TRUE;

    // We now create the DeviceCreateInfo and prepare to create the logical device.
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        // Set the queue-family create info's so vulkan can create the queue's.
        .queueCreateInfoCount = 2,
        .pQueueCreateInfos = queue_create_infos,
        // Set device-features because it's required. Won't be used for now.
        .pEnabledFeatures = &device_features,
        // Set enabled extensions for device.
        .enabledExtensionCount = device_extensions_count,
        .ppEnabledExtensionNames = device_extensions,
    };
    if (ctx->enable_validation_layers)
    {
        device_create_info.enabledLayerCount = validation_layers_count;
        device_create_info.ppEnabledLayerNames = validation_layers;
    }
    else
    {
        device_create_info.enabledLayerCount = 0;
    }

    CALL_VK(vkCreateDevice(ctx->physicalDevice, &device_create_info, NULL, &ctx->device),
            ctx->logger, "Failed to create logical device.");

    // After we created the logical device, we can fetch the actual queues.
    vkGetDeviceQueue(ctx->device, ctx->graphicsQueueFamilyIdx, 0, &ctx->graphicsQueue);
    vkGetDeviceQueue(ctx->device, ctx->presentQueueFamilyIdx, 0, &ctx->presentQueue);
}

void CreateSwapChain(GraphioContext *ctx)
{
    // Get info about swap-chain support and set up surface-format, present-mode and extent.
    SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(ctx);

    VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(&swap_chain_support);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(&swap_chain_support);
    VkExtent2D extent = ChooseSwapExtent(ctx, &swap_chain_support);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    // Prepare to create swap-chain.
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ctx->surface;
    createInfo.minImageCount = image_count;
    createInfo.imageFormat = surface_format.format;
    createInfo.imageColorSpace = surface_format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // If the same queue-family is not used for both graphics and presentation,
    // we set the following properties. Else, we use imageSharingMode VK_SHARING_MODE_EXCLUSIVE.
    if (ctx->graphicsQueueFamilyIdx != ctx->presentQueueFamilyIdx)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = (uint32_t[2]){ctx->graphicsQueueFamilyIdx, ctx->presentQueueFamilyIdx};
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swap_chain_support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = present_mode;
    createInfo.clipped = VK_TRUE;

    CALL_VK(vkCreateSwapchainKHR(ctx->device, &createInfo, NULL, &ctx->swapChain),
            ctx->logger, "failed to create swap chain!");

    // Get swapchain-images and store format and extent.
    CALL_VK(vkGetSwapchainImagesKHR(ctx->device, ctx->swapChain, &ctx->swapChainImages_count, NULL),
            ctx->logger, "Failed to get swapchain images.");
    ctx->swapChainImages = realloc(ctx->swapChainImages, sizeof(VkImage) * ctx->swapChainImages_count);
    CALL_VK(vkGetSwapchainImagesKHR(ctx->device, ctx->swapChain, &ctx->swapChainImages_count, ctx->swapChainImages),
            ctx->logger, "Failed to get swapchain images.");

    ctx->swapChainImageFormat = surface_format.format;
    ctx->swapChainExtent = extent;
    // DestroySwapChainSupportDetails(swap_chain_support);
}

void CreateImageViews(GraphioContext *ctx)
{
    // Resize SwapchainImageViews to the same size as SwapChainImages.
    // We want one ImageView per Image.
    ctx->swapChainImageViews_count = ctx->swapChainImages_count;
    ctx->swapChainImageViews = realloc(ctx->swapChainImageViews, sizeof(VkImageView) * ctx->swapChainImageViews_count);

    // Create ImageView for each Image.
    for (uint32_t i = 0; i < ctx->swapChainImageViews_count; i++)
    {
        ctx->swapChainImageViews[i] = CreateImageView(ctx, ctx->swapChainImages[i], ctx->swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void CreateRenderPass(GraphioContext *ctx)
{
    VkAttachmentDescription color_attachment_desc = {
        .format = ctx->swapChainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass_desc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency subpass_depd = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkAttachmentDescription descriptions[1] = {color_attachment_desc};

    VkRenderPassCreateInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = descriptions,
        .subpassCount = 1,
        .pSubpasses = &subpass_desc,
        .dependencyCount = 1,
        .pDependencies = &subpass_depd,
    };

    CALL_VK(vkCreateRenderPass(ctx->device, &renderpass_info, NULL, &ctx->renderPass),
            ctx->logger, "Failed to create render pass.");
}

void CreateDescriptorSetLayout(GraphioContext *ctx)
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = NULL;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[1] = {samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = 1;
    createInfo.pBindings = bindings;

    CALL_VK(vkCreateDescriptorSetLayout(ctx->device, &createInfo, NULL, &ctx->descriptorSetLayout),
            ctx->logger, "Failed to create descriptor set layout.");
}

void CreateGraphicsPipeline(GraphioContext *ctx)
{
    // First we create the programmable stages by reading and compiling
    // the vertex and fragment shaders.
    size_t vert_shader_code_size;
    char *vert_shader_code = NULL;
    ReadShaderFile(ctx->logger, SHADERS_BASE_PATH "vert.spv", &vert_shader_code, &vert_shader_code_size);
    VkShaderModule vert_shader_module;
    CreateShaderModule(ctx, vert_shader_code, vert_shader_code_size, &vert_shader_module);

    size_t frag_shader_code_size;
    char *frag_shader_code = NULL;
    ReadShaderFile(ctx->logger, SHADERS_BASE_PATH "frag.spv", &frag_shader_code, &frag_shader_code_size);
    VkShaderModule frag_shader_module;
    CreateShaderModule(ctx, frag_shader_code, frag_shader_code_size, &frag_shader_module);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shader_stage_infos[2] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    // We now configure the fixed-function stages.

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    // Set up input assembly.
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Set up viewport state. We provide viewport and scissor dynamically, so we
    // don't configure anything here. We just let it know that we will provide
    // one viewport and one scissor.
    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // Set up rasterization.
    VkPipelineRasterizationStateCreateInfo rasterization_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_FRONT_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        //.frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    // Set up multisampling.
    // We don't use multisampling for now, so disable it.
    VkPipelineMultisampleStateCreateInfo multisample_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Set up color-blending.
    // We don't use color-blending for now, so disable it.
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    // We allow the viewport and scissor to be dynamic so they are set at each draw.
    VkDynamicState dynamic_states[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    // Set up and create pipeline layout.
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx->descriptorSetLayout,
    };

    CALL_VK(vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL, &ctx->pipelineLayout),
            ctx->logger, "Failed to create pipeline layout.");

    // Set up and create pipeline.
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        // The pipeline will have two programmable stages: the vertex and fragment shader stage.
        .stageCount = 2,
        .pStages = shader_stage_infos,
        // We assign configuration for the fixed-function stages.
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterization_state_info,
        .pMultisampleState = &multisample_state_info,
        .pColorBlendState = &color_blend_state_info,
        .pDynamicState = &dynamic_state_info,
        .layout = ctx->pipelineLayout,
        .renderPass = ctx->renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    CALL_VK(vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &ctx->graphicsPipeline),
            ctx->logger, "Failed to create graphics pipeline.");

    vkDestroyShaderModule(ctx->device, vert_shader_module, NULL);
    vkDestroyShaderModule(ctx->device, frag_shader_module, NULL);
    free(frag_shader_code);
    free(vert_shader_code);
}

void CreateFramebuffers(GraphioContext *ctx)
{
    // We want one framebuffer per image-view.
    ctx->swapChainFramebuffers_count = ctx->swapChainImageViews_count;
    ctx->swapChainFramebuffers = realloc(ctx->swapChainFramebuffers, sizeof(VkFramebuffer) * ctx->swapChainFramebuffers_count);

    for (uint32_t i = 0; i < ctx->swapChainFramebuffers_count; i++)
    {
        VkImageView attachments[] = {
            ctx->swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = ctx->renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = ctx->swapChainExtent.width,
            .height = ctx->swapChainExtent.height,
            .layers = 1,
        };

        CALL_VK(vkCreateFramebuffer(ctx->device, &framebufferInfo, NULL, &ctx->swapChainFramebuffers[i]),
                ctx->logger, "Failed to create framebuffer %i", i);
    }
}

void CreateCommandPool(GraphioContext *ctx)
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->graphicsQueueFamilyIdx,
    };

    CALL_VK(vkCreateCommandPool(ctx->device, &poolInfo, NULL, &ctx->commandPool),
            ctx->logger, "Failed to create command pool.");
}

void CreateTextureImage(GraphioContext *ctx)
{
    // Fetch texture data from display buffer.
    VkDeviceSize imageSize = ctx->display->display_buffer_size;
    int texWidth = ctx->display->display_buffer_width;
    int texHeight = ctx->display->display_buffer_height;
    int texChannels = ctx->display->display_buffer_channels;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer(ctx, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    void *data;
    CALL_VK(vkMapMemory(ctx->device, stagingBufferMemory, 0, imageSize, 0, &data),
            ctx->logger, "Failed to map memory for texture image staging buffer.");
    memcpy(data, ctx->display->display_buffer, (size_t)imageSize);
    vkUnmapMemory(ctx->device, stagingBufferMemory);

    CreateImage(ctx, (uint32_t)texWidth, (uint32_t)texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ctx->textureImage, &ctx->textureImageMemory);

    // Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
    TransitionImageLayout(ctx, ctx->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    // Copy buffer to image.
    CopyBufferToImage(ctx, stagingBuffer, ctx->textureImage, (uint32_t)texWidth, (uint32_t)texHeight);
    // Transition texture image from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL to prepare it for shader access.
    TransitionImageLayout(ctx, ctx->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx->device, stagingBuffer, NULL);
    vkFreeMemory(ctx->device, stagingBufferMemory, NULL);
}

void CreateTextureImageView(GraphioContext *ctx)
{
    ctx->textureImageView = CreateImageView(ctx, ctx->textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void CreateTextureSampler(GraphioContext *ctx)
{
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // Should we use VK_FILTER_NEAREST to get the 'square-pixel' effect?
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.anisotropyEnable = VK_TRUE;

    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(ctx->physicalDevice, &properties);

    createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias = 0.0f;
    createInfo.minLod = 0.0f;
    createInfo.maxLod = 0.0f;
    CALL_VK(vkCreateSampler(ctx->device, &createInfo, NULL, &ctx->textureSampler),
            ctx->logger, "Failed to create texture sampler.");
}

void CreateDescriptorPool(GraphioContext *ctx)
{
    VkDescriptorPoolSize poolSizes[1];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = poolSizes;
    createInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    CALL_VK(vkCreateDescriptorPool(ctx->device, &createInfo, NULL, &ctx->descriptorPool),
            ctx->logger, "Failed to create descriptor pool.");
}

void CreateDescriptorSets(GraphioContext *ctx)
{
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {ctx->descriptorSetLayout, ctx->descriptorSetLayout};

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = ctx->descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts;

    CALL_VK(vkAllocateDescriptorSets(ctx->device, &allocInfo, ctx->descriptorSets),
            ctx->logger, "Failed to allocate descriptor sets.");

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = ctx->textureImageView;
        imageInfo.sampler = ctx->textureSampler;

        VkWriteDescriptorSet descriptorWrite[1] = {};
        descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[0].dstSet = ctx->descriptorSets[i];
        descriptorWrite[0].dstBinding = 0;
        descriptorWrite[0].dstArrayElement = 0;
        descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite[0].descriptorCount = 1;
        descriptorWrite[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(ctx->device, 1, descriptorWrite, 0, NULL);
    }
}

void CreateCommandBuffers(GraphioContext *ctx)
{
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };

    CALL_VK(vkAllocateCommandBuffers(ctx->device, &alloc_info, ctx->commandBuffers),
            ctx->logger, "Failed to create command buffers.");
}

void CreateSyncObjects(GraphioContext *ctx)
{
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {

        CALL_VK(vkCreateSemaphore(ctx->device, &semaphore_info, NULL, &ctx->imageAvailableSemaphores[i]),
                ctx->logger, "Failed to create ImageAvailable-semaphore.");
        CALL_VK(vkCreateSemaphore(ctx->device, &semaphore_info, NULL, &ctx->renderFinishedSemaphores[i]),
                ctx->logger, "Failed to create RenderFinished-semaphore.");
        CALL_VK(vkCreateFence(ctx->device, &fence_info, NULL, &ctx->inFlightFences[i]),
                ctx->logger, "Failed to create InFlight-fence.");
    }
}

// Debug/Extensions

bool CheckValidationLayerSupport(GraphioContext *ctx)
{
    uint32_t layerCount;
    CALL_VK(vkEnumerateInstanceLayerProperties(&layerCount, NULL),
            ctx->logger, "Failed to enumerate instance layer properties.");

    if (layerCount == 0)
    {
        PANIC(ctx->logger, "No available instance layer properties.");
    }

    VkLayerProperties available_layers[layerCount];
    CALL_VK(vkEnumerateInstanceLayerProperties(&layerCount, available_layers),
            ctx->logger, "Failed to enumerate instance layer properties.");

    for (uint32_t i = 0; i < validation_layers_count; i++)
    {
        bool layer_found = false;

        for (uint32_t j = 0; j < layerCount; j++)
        {
            if (strcmp(validation_layers[i], available_layers[j].layerName) == 0)
            {
                layer_found = true;
                break;
            }
        }

        if (!layer_found)
            return false;
    }

    return true;
}

char **GetRequiredExtensions(GraphioContext *ctx, uint32_t *count)
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    char **extensions;
    if (ctx->enable_validation_layers)
    {
        extensions = calloc(1, sizeof(const char *) * glfwExtensionCount + 1 * sizeof(const char *));
        *count = glfwExtensionCount + 1;
    }
    else
    {
        extensions = calloc(1, sizeof(const char *) * glfwExtensionCount);
        *count = glfwExtensionCount;
    }

    for (uint32_t i = 0; i < glfwExtensionCount; i++)
    {
        extensions[i] = calloc(1, strlen(glfwExtensions[i]));
        strcpy(extensions[i], glfwExtensions[i]);
    }

    if (ctx->enable_validation_layers)
    {
        extensions[glfwExtensionCount] = calloc(1, strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
        strcpy(extensions[glfwExtensionCount], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void PopulateDebugMessengerCreateInfo(Logger *logger, VkDebugUtilsMessengerCreateInfoEXT *createInfo)
{
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pUserData = logger;
    createInfo->pfnUserCallback = debugCallback;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

// Swapchain

void CleanUpSwapchain(GraphioContext *ctx)
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

void RecreateSwapChain(GraphioContext *ctx)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(ctx->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx->device);

    CleanUpSwapchain(ctx);

    CreateSwapChain(ctx);
    CreateImageViews(ctx);
    CreateFramebuffers(ctx);
}

// Get details about Swapchain surface-support.
// Includes surface capabilities, surface formats and surface present-modes.
// Creates a SwapChainSupportDetails object that needs to be destroyed with a call to
// DestroySwapChainSupportDetails.
SwapChainSupportDetails QuerySwapChainSupport(GraphioContext *ctx)
{
    SwapChainSupportDetails details = {};

    // Get surface capabilities.
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physicalDevice, ctx->surface, &details.capabilities),
            ctx->logger, "Failed to get physical device's surface capabilities.");

    // Get surface formats.
    CALL_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physicalDevice, ctx->surface, &details.formats_count, NULL),
            ctx->logger, "Failed to get physical device's surface formats.");
    details.formats = realloc(details.formats, sizeof(VkSurfaceFormatKHR) * details.formats_count);
    CALL_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physicalDevice, ctx->surface, &details.formats_count, details.formats),
            ctx->logger, "Failed to get physical device's surface formats.");

    // Get present modes.
    CALL_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physicalDevice, ctx->surface, &details.present_modes_count, NULL),
            ctx->logger, "Failed to get physical device's surface present modes.");
    details.present_modes = realloc(details.present_modes, sizeof(VkPresentModeKHR) * details.present_modes_count);
    CALL_VK(vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physicalDevice, ctx->surface, &details.present_modes_count, details.present_modes),
            ctx->logger, "Failed to get physical device's surface present modes.");

    return details;
}

VkSurfaceFormatKHR ChooseSwapSurfaceFormat(SwapChainSupportDetails *support)
{
    for (uint32_t i = 0; i < support->formats_count; i++)
    {
        if (support->formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            support->formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return support->formats[i];
    }

    return support->formats[0];
}

VkPresentModeKHR ChooseSwapPresentMode(SwapChainSupportDetails *support)
{
    for (uint32_t i = 0; i < support->present_modes_count; i++)
    {
        if (support->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return support->present_modes[i];
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapExtent(GraphioContext *ctx, SwapChainSupportDetails *support)
{
    if (support->capabilities.currentExtent.width != UINT32_MAX)
    {
        return support->capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(ctx->window, &width, &height);

        VkExtent2D actualExtent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
        };

        actualExtent.width = ClampUINT_32(actualExtent.width, support->capabilities.minImageExtent.width, support->capabilities.maxImageExtent.width);
        actualExtent.height = ClampUINT_32(actualExtent.height, support->capabilities.minImageExtent.height, support->capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void DestroySwapChainSupportDetails(SwapChainSupportDetails details)
{
    free(details.formats);
    free(details.present_modes);
}

// Image/Imageviews

VkImageView CreateImageView(GraphioContext *ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
        .subresourceRange = (VkImageSubresourceRange){
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkImageView imageView;
    CALL_VK(vkCreateImageView(ctx->device, &createInfo, NULL, &imageView),
            ctx->logger, "Failed to create texture image view.");

    return imageView;
}

void CreateImage(GraphioContext *ctx, uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                 VkImage *image, VkDeviceMemory *imageMemory)
{
    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = (uint32_t)width;
    createInfo.extent.height = (uint32_t)height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    CALL_VK(vkCreateImage(ctx->device, &createInfo, NULL, image),
            ctx->logger, "Failed to create texture image.");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ctx->device, *image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(ctx, memRequirements.memoryTypeBits, properties);
    CALL_VK(vkAllocateMemory(ctx->device, &allocInfo, NULL, imageMemory),
            ctx->logger, "Failed to allocate memory for texture image memory.");

    CALL_VK(vkBindImageMemory(ctx->device, *image, *imageMemory, 0),
            ctx->logger, "Failed to bind texture image to texture image memory.");
}

void CopyBufferToImage(GraphioContext *ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(ctx);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = (VkImageSubresourceLayers){
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    region.imageOffset = (VkOffset3D){
        .x = 0,
        .y = 0,
        .z = 0,
    };
    region.imageExtent = (VkExtent3D){
        .width = width,
        .height = height,
        .depth = 1,
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(ctx, commandBuffer);
}

void TransitionImageLayout(GraphioContext *ctx, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(ctx);

    VkImageMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.oldLayout = oldLayout;
    memoryBarrier.newLayout = newLayout;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.image = image;
    memoryBarrier.subresourceRange = (VkImageSubresourceRange){
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    // Set up access masks and pipeline stages for transition from
    // undefined -> transfer destination &
    // transfer destination -> shader reading
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        // We have nothing to wait for in the source stage, so we set access mask to 0.
        memoryBarrier.srcAccessMask = 0;
        // Set transfer write to destination.
        memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        // The source will be the earliest point of the pipeline.
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        // The destination is the transfer pseudo-stage.
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        // Set transfer write in source stage.
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        // Set shader read in destination stage.
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // The image will come from the transfer stage.
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        // Set destination stage to fragment shader.
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        memoryBarrier.srcAccessMask = 0;
        memoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        PANIC(ctx->logger, "Unsupported layout transition.");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, NULL, 0, NULL, 1, &memoryBarrier);

    EndSingleTimeCommands(ctx, commandBuffer);
}

// Memory/Buffers

uint32_t FindMemoryType(GraphioContext *ctx, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties = {};
    vkGetPhysicalDeviceMemoryProperties(ctx->physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if (type_filter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    PANIC(ctx->logger, "Failed to find suitable memory type.");
}

void CreateBuffer(GraphioContext *ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CALL_VK(vkCreateBuffer(ctx->device, &bufferInfo, NULL, buffer),
            ctx->logger, "Failed to create vertex buffer.");

    VkMemoryRequirements memRequirements = {};
    vkGetBufferMemoryRequirements(ctx->device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(ctx, memRequirements.memoryTypeBits, properties);
    CALL_VK(vkAllocateMemory(ctx->device, &allocInfo, NULL, bufferMemory),
            ctx->logger, "Failed to allocate vertex-buffer memory.");

    CALL_VK(vkBindBufferMemory(ctx->device, *buffer, *bufferMemory, 0),
            ctx->logger, "Failed to bind vertex buffer to vertex memory.");
}

// Shaders

void ReadShaderFile(Logger *logger, const char *filename, char **data, size_t *size)
{
    FILE *fp;
    if ((fp = fopen(filename, "rb")) == NULL)
    {
        PANIC(logger, "Can't open file '%s'.", filename);
        return;
    }

    // find size.
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // read file into char-buffer.
    *data = realloc(*data, sizeof(char) * (*size));
    fread(*data, sizeof(char) * (*size), 1, fp);
}

void CreateShaderModule(GraphioContext *ctx, const char *code, size_t size, VkShaderModule *module)
{
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t *)code,
    };

    CALL_VK(vkCreateShaderModule(ctx->device, &create_info, NULL, module),
            ctx->logger, "Failed to create shader module.");

    return;
}

// Commands

void RecordCommandBuffer(GraphioContext *ctx, uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };

    CALL_VK(vkBeginCommandBuffer(ctx->commandBuffers[ctx->currentFrame], &begin_info),
            ctx->logger, "Failed to being recording command buffer for image %i.", image_index);

    VkClearValue clearValues[1] = {};
    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;

    VkRenderPassBeginInfo renderpass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->renderPass,
        .framebuffer = ctx->swapChainFramebuffers[image_index],
        .renderArea = {
            .offset = {0, 0},
            .extent = ctx->swapChainExtent,
        },
        .clearValueCount = 1,
        .pClearValues = clearValues,
    };

    vkCmdBeginRenderPass(ctx->commandBuffers[ctx->currentFrame], &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(ctx->commandBuffers[ctx->currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphicsPipeline);

    // Since we set viewport and scissor as dynamic states we need to pass them in here
    // while recording the command buffer.
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)ctx->swapChainExtent.width,
        .height = (float)ctx->swapChainExtent.height,
        .maxDepth = 1.0f,
        .minDepth = 0.0f,
    };
    vkCmdSetViewport(ctx->commandBuffers[ctx->currentFrame], 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = ctx->swapChainExtent,
    };
    vkCmdSetScissor(ctx->commandBuffers[ctx->currentFrame], 0, 1, &scissor);

    vkCmdBindDescriptorSets(ctx->commandBuffers[ctx->currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            ctx->pipelineLayout, 0, 1, &ctx->descriptorSets[ctx->currentFrame], 0, NULL);
    vkCmdDraw(ctx->commandBuffers[ctx->currentFrame], 3, 1, 0, 0);

    vkCmdEndRenderPass(ctx->commandBuffers[ctx->currentFrame]);

    // When we record commands, we are not able to error-check. When we here call
    // 'vkEndCommandBuffer', we are finally submitting the commands and can then error-check.
    // This means that no errors are caught until we are finished recording.
    CALL_VK(vkEndCommandBuffer(ctx->commandBuffers[ctx->currentFrame]),
            ctx->logger, "Failed to record command buffer for image %i.", image_index);
}

VkCommandBuffer BeginSingleTimeCommands(GraphioContext *ctx)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx->commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void EndSingleTimeCommands(GraphioContext *ctx, VkCommandBuffer commandBuffer)
{
    CALL_VK(vkEndCommandBuffer(commandBuffer), ctx->logger, "Failed to end command buffer.");

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->graphicsQueue);

    vkFreeCommandBuffers(ctx->device, ctx->commandPool, 1, &commandBuffer);
}

// Keys

void SetKeyPressed(GraphioContext *ctx, int key)
{
    switch (key)
    {
    case GLFW_KEY_1:
    {
        logger_LogDebug(ctx->logger, "Key 1 pressed.");
        *(ctx->keys) |= CH8_IO_KEY1_BIT;
        break;
    }
    case GLFW_KEY_2:
    {
        logger_LogDebug(ctx->logger, "Key 2 pressed.");
        *(ctx->keys) |= CH8_IO_KEY2_BIT;
        break;
    }
    case GLFW_KEY_3:
    {
        logger_LogDebug(ctx->logger, "Key 3 pressed.");
        *(ctx->keys) |= CH8_IO_KEY3_BIT;
        break;
    }
    case GLFW_KEY_4:
    {
        logger_LogDebug(ctx->logger, "Key C pressed.");
        *(ctx->keys) |= CH8_IO_KEYC_BIT;
        break;
    }
    case GLFW_KEY_Q:
    {
        logger_LogDebug(ctx->logger, "Key 4 pressed.");
        *(ctx->keys) |= CH8_IO_KEY4_BIT;
        break;
    }
    case GLFW_KEY_W:
    {
        logger_LogDebug(ctx->logger, "Key 5 pressed.");
        *(ctx->keys) |= CH8_IO_KEY5_BIT;
        break;
    }
    case GLFW_KEY_E:
    {
        logger_LogDebug(ctx->logger, "Key 6 pressed.");
        *(ctx->keys) |= CH8_IO_KEY6_BIT;
        break;
    }
    case GLFW_KEY_R:
    {
        logger_LogDebug(ctx->logger, "Key D pressed.");
        *(ctx->keys) |= CH8_IO_KEYD_BIT;
        break;
    }
    case GLFW_KEY_A:
    {
        logger_LogDebug(ctx->logger, "Key 7 pressed.");
        *(ctx->keys) |= CH8_IO_KEY7_BIT;
        break;
    }
    case GLFW_KEY_S:
    {
        logger_LogDebug(ctx->logger, "Key 8 pressed.");
        *(ctx->keys) |= CH8_IO_KEY8_BIT;
        break;
    }
    case GLFW_KEY_D:
    {
        logger_LogDebug(ctx->logger, "Key 9 pressed.");
        *(ctx->keys) |= CH8_IO_KEY9_BIT;
        break;
    }
    case GLFW_KEY_F:
    {
        logger_LogDebug(ctx->logger, "Key E pressed.");
        *(ctx->keys) |= CH8_IO_KEYE_BIT;
        break;
    }
    case GLFW_KEY_Z:
    {
        logger_LogDebug(ctx->logger, "Key A pressed.");
        *(ctx->keys) |= CH8_IO_KEYA_BIT;
        break;
    }
    case GLFW_KEY_X:
    {
        logger_LogDebug(ctx->logger, "Key 0 pressed.");
        *(ctx->keys) |= CH8_IO_KEY0_BIT;
        break;
    }
    case GLFW_KEY_C:
    {
        logger_LogDebug(ctx->logger, "Key B pressed.");
        *(ctx->keys) |= CH8_IO_KEYB_BIT;
        break;
    }
    case GLFW_KEY_V:
    {
        logger_LogDebug(ctx->logger, "Key F pressed.");
        *(ctx->keys) |= CH8_IO_KEYF_BIT;
        break;
    }
    default:
        logger_LogDebug(ctx->logger, "Unknown key pressed.");
        break;
    }
}

void SetKeyReleased(GraphioContext *ctx, int key)
{
    switch (key)
    {
    case GLFW_KEY_1:
    {
        logger_LogDebug(ctx->logger, "Key 1 released.");
        *(ctx->keys) ^= CH8_IO_KEY1_BIT;
        break;
    }
    case GLFW_KEY_2:
    {
        logger_LogDebug(ctx->logger, "Key 2 released.");
        *(ctx->keys) ^= CH8_IO_KEY2_BIT;
        break;
    }
    case GLFW_KEY_3:
    {
        logger_LogDebug(ctx->logger, "Key 3 released.");
        *(ctx->keys) ^= CH8_IO_KEY3_BIT;
        break;
    }
    case GLFW_KEY_4:
    {
        logger_LogDebug(ctx->logger, "Key C released.");
        *(ctx->keys) ^= CH8_IO_KEYC_BIT;
        break;
    }
    case GLFW_KEY_Q:
    {
        logger_LogDebug(ctx->logger, "Key 4 released.");
        *(ctx->keys) ^= CH8_IO_KEY4_BIT;
        break;
    }
    case GLFW_KEY_W:
    {
        logger_LogDebug(ctx->logger, "Key 5 released.");
        *(ctx->keys) ^= CH8_IO_KEY5_BIT;
        break;
    }
    case GLFW_KEY_E:
    {
        logger_LogDebug(ctx->logger, "Key 6 released.");
        *(ctx->keys) ^= CH8_IO_KEY6_BIT;
        break;
    }
    case GLFW_KEY_R:
    {
        logger_LogDebug(ctx->logger, "Key D released.");
        *(ctx->keys) ^= CH8_IO_KEYD_BIT;
        break;
    }
    case GLFW_KEY_A:
    {
        logger_LogDebug(ctx->logger, "Key 7 released.");
        *(ctx->keys) ^= CH8_IO_KEY7_BIT;
        break;
    }
    case GLFW_KEY_S:
    {
        logger_LogDebug(ctx->logger, "Key 8 released.");
        *(ctx->keys) ^= CH8_IO_KEY8_BIT;
        break;
    }
    case GLFW_KEY_D:
    {
        logger_LogDebug(ctx->logger, "Key 9 released.");
        *(ctx->keys) ^= CH8_IO_KEY9_BIT;
        break;
    }
    case GLFW_KEY_F:
    {
        logger_LogDebug(ctx->logger, "Key E released.");
        *(ctx->keys) ^= CH8_IO_KEYE_BIT;
        break;
    }
    case GLFW_KEY_Z:
    {
        logger_LogDebug(ctx->logger, "Key A released.");
        *(ctx->keys) ^= CH8_IO_KEYA_BIT;
        break;
    }
    case GLFW_KEY_X:
    {
        logger_LogDebug(ctx->logger, "Key 0 released.");
        *(ctx->keys) ^= CH8_IO_KEY0_BIT;
        break;
    }
    case GLFW_KEY_C:
    {
        logger_LogDebug(ctx->logger, "Key B released.");
        *(ctx->keys) ^= CH8_IO_KEYB_BIT;
        break;
    }
    case GLFW_KEY_V:
    {
        logger_LogDebug(ctx->logger, "Key F released.");
        *(ctx->keys) ^= CH8_IO_KEYF_BIT;
        break;
    }
    default:
        logger_LogDebug(ctx->logger, "Unknown key released.");
        break;
    }
}

// Callbacks

void glfwKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    GraphioContext *ctx = glfwGetWindowUserPointer(window);

    if (action == GLFW_PRESS)
    {
        SetKeyPressed(ctx, key);
    }
    else if (action == GLFW_RELEASE)
    {
        SetKeyReleased(ctx, key);
    }
}

void glfwErrorCallback(int error_code, const char *description)
{
    GLFWwindow *window = glfwGetCurrentContext();
    GraphioContext *ctx = glfwGetWindowUserPointer(window);
    PANIC(ctx->logger, "GLFW(RC=%d) - %s.", error_code, description);
}

void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    GraphioContext *ctx = glfwGetWindowUserPointer(window);
    ctx->frameBufferResized = true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    Logger *logger = pUserData;
    logger_LogDebug(logger, "Validation layer: %s", pCallbackData->pMessage);
    return VK_FALSE;
}
