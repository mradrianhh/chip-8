#include <signal.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "graphics/internal.h"
#include "maths/maths.h"

static const uint32_t device_extensions_count = 1;
static const char *device_extensions[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static const uint32_t validation_layers_count = 1;
static const char *validation_layers[1] = {"VK_LAYER_KHRONOS_validation"};

static void InitGLFW(GraphicsContext *ctx);
static void InitVulkan(GraphicsContext *ctx);

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

// Debug/Extensions

static bool CheckValidationLayerSupport(GraphicsContext *ctx);
static char **GetRequiredExtensions(GraphicsContext *ctx, uint32_t *count);
static void PopulateDebugMessengerCreateInfo(Logger *logger, VkDebugUtilsMessengerCreateInfoEXT *createInfo);
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);

// Swapchain

static void CleanUpSwapchain(GraphicsContext *ctx);
static void RecreateSwapChain(GraphicsContext *ctx);
static SwapChainSupportDetails QuerySwapChainSupport(GraphicsContext *ctx);
static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(SwapChainSupportDetails *support);
static VkPresentModeKHR ChooseSwapPresentMode(SwapChainSupportDetails *support);
static VkExtent2D ChooseSwapExtent(GraphicsContext *ctx, SwapChainSupportDetails *support);
static void DestroySwapChainSupportDetails(SwapChainSupportDetails details);

// Image/Imageview

static VkImageView CreateImageView(GraphicsContext *ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

// Shaders

static void ReadShaderFile(Logger *logger, const char *filename, char **data, size_t *size);
static void CreateShaderModule(GraphicsContext *ctx, const char *code, size_t size, VkShaderModule *module);

// Drawing
void RecordCommandBuffer(GraphicsContext *ctx, uint32_t image_index)
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

    vkCmdDraw(ctx->commandBuffers[ctx->currentFrame], 3, 1, 0, 0);

    vkCmdEndRenderPass(ctx->commandBuffers[ctx->currentFrame]);

    // When we record commands, we are not able to error-check. When we here call
    // 'vkEndCommandBuffer', we are finally submitting the commands and can then error-check.
    // This means that no errors are caught until we are finished recording.
    CALL_VK(vkEndCommandBuffer(ctx->commandBuffers[ctx->currentFrame]),
            ctx->logger, "Failed to record command buffer for image %i.", image_index);
}

// Callbacks

static void glfwErrorCallback(int error_code, const char *description);
static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

GraphicsContext *gfx_CreateGraphicsContext(Logger *logger)
{
    GraphicsContext *ctx = calloc(1, sizeof(GraphicsContext));
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

void gfx_DrawFrame(GraphicsContext *ctx)
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

void gfx_StopGraphicsContext(GraphicsContext *ctx)
{
        CALL_VK(vkDeviceWaitIdle(ctx->device), ctx->logger, "Failed while waiting for device to go idle.");
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

void CreateInstance(GraphicsContext *ctx)
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

void SetupDebugMessenger(GraphicsContext *ctx)
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    PopulateDebugMessengerCreateInfo(ctx->logger, &createInfo);

    CALL_VK(CreateDebugUtilsMessengerEXT(ctx->instance, &createInfo, NULL, &ctx->debugMessenger),
            ctx->logger, "Failed to set up debug messenger.");
}

void CreateSurface(GraphicsContext *ctx)
{
    // Using CALL_VK on GLFW function because it returns VkResult.
    CALL_VK(glfwCreateWindowSurface(ctx->instance, ctx->window, NULL, &ctx->surface),
            ctx->logger, "Failed to create window surface!");
}

void SelectPhysicalDevice(GraphicsContext *ctx)
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

void CreateLogicalDevice(GraphicsContext *ctx)
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

void CreateSwapChain(GraphicsContext *ctx)
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
    //DestroySwapChainSupportDetails(swap_chain_support);
}

void CreateImageViews(GraphicsContext *ctx)
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

void CreateRenderPass(GraphicsContext *ctx)
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

void CreateGraphicsPipeline(GraphicsContext *ctx)
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
        .cullMode = VK_CULL_MODE_BACK_BIT,
        //.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
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

void CreateFramebuffers(GraphicsContext *ctx)
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

void CreateCommandPool(GraphicsContext *ctx)
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx->graphicsQueueFamilyIdx,
    };

    CALL_VK(vkCreateCommandPool(ctx->device, &poolInfo, NULL, &ctx->commandPool),
            ctx->logger, "Failed to create command pool.");
}

void CreateCommandBuffers(GraphicsContext *ctx)
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

void CreateSyncObjects(GraphicsContext *ctx)
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

bool CheckValidationLayerSupport(GraphicsContext *ctx)
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

char **GetRequiredExtensions(GraphicsContext *ctx, uint32_t *count)
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

void RecreateSwapChain(GraphicsContext *ctx)
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
SwapChainSupportDetails QuerySwapChainSupport(GraphicsContext *ctx)
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

VkExtent2D ChooseSwapExtent(GraphicsContext *ctx, SwapChainSupportDetails *support)
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

VkImageView CreateImageView(GraphicsContext *ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
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

void CreateShaderModule(GraphicsContext *ctx, const char *code, size_t size, VkShaderModule *module)
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

// Callbacks

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

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    Logger *logger = pUserData;
    logger_LogDebug(logger, "Validation layer: %s", pCallbackData->pMessage);
    return VK_FALSE;
}
