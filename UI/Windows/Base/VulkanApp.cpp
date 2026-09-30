#include "VulkanApp.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

static void check_vk_result(VkResult err) {
    if (err == 0) return;
    std::cerr << "[Vulkan Error] VkResult = " << err << std::endl;
    if (err < 0) abort();
}

VulkanApp::VulkanApp(int width, int height, const char* title)
    : width_(width), height_(height), title_(title) {}

VulkanApp::~VulkanApp() {
    cleanup();
}

void VulkanApp::run() {
    init_window();
    init_vulkan();
    init_imgui();
    main_loop();
}

void VulkanApp::init_window() {
    if (!glfwInit()) {
        throw std::runtime_error("GLFWの初期化に失敗しました。");
    }
    glfw_initialized_ = true;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(width_, height_, title_, nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("GLFWウィンドウの作成に失敗しました。");
    }
}

void VulkanApp::init_vulkan() {
    VkResult err;

    // 1. Instance 作成
    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;

    err = vkCreateInstance(&create_info, allocator_, &instance_);
    check_vk_result(err);

    // 2. Physical Device 選択
    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(instance_, &gpu_count, nullptr);
    check_vk_result(err);
    if (gpu_count == 0) {
        throw std::runtime_error("Vulkanに対応したGPUが見つかりません。");
    }

    std::vector<VkPhysicalDevice> gpus(gpu_count);
    err = vkEnumeratePhysicalDevices(instance_, &gpu_count, gpus.data());
    check_vk_result(err);

    for (auto& gpu : gpus) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(gpu, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physical_device_ = gpu;
            break;
        }
    }
    if (physical_device_ == VK_NULL_HANDLE) {
        physical_device_ = gpus[0];
    }

    // 3. Queue Family 選択
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queues(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &count, queues.data());
    for (uint32_t i = 0; i < count; i++) {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_ = i;
            break;
        }
    }
    if (queue_family_ == static_cast<uint32_t>(-1)) {
        throw std::runtime_error("グラフィックスキューを持つ Vulkan キューが見つかりません。");
    }

    // 4. Logical Device 作成
    int device_extension_count = 1;
    const char* device_extensions[] = { "VK_KHR_swapchain" };
    float queue_priority[] = { 1.0f };

    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = queue_family_;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;

    VkDeviceCreateInfo create_info_dev = {};
    create_info_dev.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info_dev.queueCreateInfoCount = 1;
    create_info_dev.pQueueCreateInfos = queue_info;
    create_info_dev.enabledExtensionCount = device_extension_count;
    create_info_dev.ppEnabledExtensionNames = device_extensions;

    err = vkCreateDevice(physical_device_, &create_info_dev, allocator_, &device_);
    check_vk_result(err);
    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);

    // 5. Descriptor Pool 作成
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    err = vkCreateDescriptorPool(device_, &pool_info, allocator_, &descriptor_pool_);
    check_vk_result(err);

    // 6. Surface & Window 構築
    VkSurfaceKHR surface;
    err = glfwCreateWindowSurface(instance_, window_, allocator_, &surface);
    check_vk_result(err);

    ImGui_ImplVulkanH_Window* wd = &main_window_data_;
    wd->Surface = surface;

    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, queue_family_, wd->Surface, &res);
    if (res != VK_TRUE) {
        throw std::runtime_error("WSI Support に失敗しました。");
    }

    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physical_device_, wd->Surface, requestSurfaceImageFormat, IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);
    // FIFO is the Vulkan-required vsync mode. The helper window defaults to an
    // invalid sentinel, so choose it explicitly instead of allowing an
    // unthrottled/driver-dependent presentation mode.
    wd->PresentMode = VK_PRESENT_MODE_FIFO_KHR;

    ImGui_ImplVulkanH_CreateOrResizeWindow(instance_, physical_device_, device_, wd, queue_family_, allocator_, width_, height_, min_image_count_, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

void VulkanApp::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    char* windows_dir = nullptr;
    size_t windows_dir_length = 0;
    _dupenv_s(&windows_dir, &windows_dir_length, "WINDIR");
    const std::string fonts_dir = std::string(windows_dir && windows_dir_length > 1 ? windows_dir : "C:\\Windows") + "\\Fonts\\";
    std::free(windows_dir);
    const char* japanese_font_files[] = { "YuGothR.ttc", "meiryo.ttc", "msgothic.ttc" };
    for (const char* font_file : japanese_font_files) {
        const std::string font_path = fonts_dir + font_file;
        std::ifstream font_probe(font_path, std::ios::binary);
        if (!font_probe) continue;

        ImFontConfig font_config{};
        font_config.FontNo = 0;
        ImFont* japanese_font = io.Fonts->AddFontFromFileTTF(
            font_path.c_str(), 18.0f, &font_config, io.Fonts->GetGlyphRangesJapanese());
        if (japanese_font != nullptr) {
            io.FontDefault = japanese_font;
            break;
        }
    }

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForVulkan(window_, true)) {
        throw std::runtime_error("ImGui GLFW バックエンドの初期化に失敗しました。");
    }
    imgui_glfw_initialized_ = true;

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_0;
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physical_device_;
    init_info.Device = device_;
    init_info.QueueFamily = queue_family_;
    init_info.Queue = queue_;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = descriptor_pool_;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = main_window_data_.ImageCount;
    init_info.Allocator = allocator_;
    init_info.PipelineInfoMain.RenderPass = main_window_data_.RenderPass;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        throw std::runtime_error("ImGui Vulkan バックエンドの初期化に失敗しました。");
    }
    imgui_vulkan_initialized_ = true;
}

void VulkanApp::frame_render(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(device_, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        swapchain_rebuild_ = true;
        return;
    }
    check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(device_, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        check_vk_result(err);

        err = vkResetFences(device_, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(device_, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(queue_, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

void VulkanApp::frame_present(ImGui_ImplVulkanH_Window* wd) {
    if (swapchain_rebuild_) return;

    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(queue_, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        swapchain_rebuild_ = true;
        return;
    }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

void VulkanApp::main_loop() {
    ImVec4 clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    ImGui_ImplVulkanH_Window* wd = &main_window_data_;

    while (!glfwWindowShouldClose(window_)) {
        // Wait for input/window events, with a 60 Hz timeout so a static UI
        // does not continuously submit frames and keep the GPU busy.
        glfwWaitEventsTimeout(1.0 / 60.0);
        if (glfwWindowShouldClose(window_)) break;

        if (swapchain_rebuild_) {
            int w, h;
            glfwGetFramebufferSize(window_, &w, &h);
            if (w > 0 && h > 0) {
                ImGui_ImplVulkan_SetMinImageCount(min_image_count_);
                check_vk_result(vkDeviceWaitIdle(device_));
                ImGui_ImplVulkanH_CreateOrResizeWindow(instance_, physical_device_, device_, &main_window_data_, queue_family_, allocator_, w, h, min_image_count_, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
                main_window_data_.FrameIndex = 0;
                swapchain_rebuild_ = false;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
            main_tool_bar_.open_new_canvas();
        }
        if (CanvasDocument* active_canvas = canvas_manager_.get_active_canvas()) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) {
                active_canvas->stroke_manager->undo();
            } else if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) ||
                       ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) {
                active_canvas->stroke_manager->redo();
            }
        }

        main_tool_bar_.render(canvas_manager_);
        render_canvas_area();
        ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        tool_sidebar_.render(main_viewport->WorkPos, main_viewport->WorkSize);

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            frame_render(wd, draw_data);
            frame_present(wd);
        }
    }
}

void VulkanApp::render_canvas_area() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
    ImGui::Begin("CanvasViewport", nullptr, host_window_flags);
    ImGui::PopStyleColor();

    CanvasDocument* active_canvas = canvas_manager_.get_active_canvas();
    if (active_canvas != nullptr) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 win_size = ImGui::GetWindowSize();
        ImVec2 win_pos = ImGui::GetWindowPos();

        float canvas_w = static_cast<float>(active_canvas->width);
        float canvas_h = static_cast<float>(active_canvas->height);
        const float canvas_scale = std::min(1.0f, std::min(win_size.x / canvas_w, win_size.y / canvas_h));
        const float view_canvas_w = canvas_w * canvas_scale;
        const float view_canvas_h = canvas_h * canvas_scale;

        ImVec2 canvas_pos = ImVec2(
            win_pos.x + (win_size.x - view_canvas_w) * 0.5f,
            win_pos.y + (win_size.y - view_canvas_h) * 0.5f
        );

        draw_list->AddRectFilled(
            canvas_pos,
            ImVec2(canvas_pos.x + view_canvas_w, canvas_pos.y + view_canvas_h),
            IM_COL32(255, 255, 255, 255)
        );

        ImGui::SetCursorScreenPos(canvas_pos);
        ImGui::InvisibleButton("##CanvasInput", ImVec2(view_canvas_w, view_canvas_h));
        const bool canvas_hovered = ImGui::IsItemHovered();
        const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        const float canvas_x = std::clamp((mouse_pos.x - canvas_pos.x) / canvas_scale, 0.0f, canvas_w);
        const float canvas_y = std::clamp((mouse_pos.y - canvas_pos.y) / canvas_scale, 0.0f, canvas_h);
        StrokeManager& stroke_manager = *active_canvas->stroke_manager;

        if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec4 color = tool_sidebar_.selected_color();
            const StrokeColor stroke_color{
                static_cast<uint8_t>(color.x * 255.0f + 0.5f),
                static_cast<uint8_t>(color.y * 255.0f + 0.5f),
                static_cast<uint8_t>(color.z * 255.0f + 0.5f),
                static_cast<uint8_t>(color.w * 255.0f + 0.5f)
            };
            stroke_manager.on_stroke_begin(canvas_x, canvas_y, tool_sidebar_.brush_size(), tool_sidebar_.selected_pen_type(), stroke_color);
            stroke_in_progress_ = stroke_manager.get_current_stroke() != nullptr;
        }
        if (stroke_in_progress_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            stroke_manager.on_stroke_move(canvas_x, canvas_y, tool_sidebar_.brush_size());
        }
        if (stroke_in_progress_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            stroke_manager.on_stroke_end(canvas_x, canvas_y, tool_sidebar_.brush_size());
            stroke_in_progress_ = false;
        }

        draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + view_canvas_w, canvas_pos.y + view_canvas_h), true);
        const auto draw_stroke = [draw_list, canvas_pos, canvas_scale](const std::vector<Point>& points, ImU32 color) {
            if (points.empty()) return;
            const auto screen_point = [canvas_pos, canvas_scale](const Point& point) {
                return ImVec2(canvas_pos.x + point.pos.x * canvas_scale, canvas_pos.y + point.pos.y * canvas_scale);
            };
            if (points.size() == 1) {
                draw_list->AddCircleFilled(screen_point(points.front()), points.front().thickness * canvas_scale * 0.5f, color);
                return;
            }
            for (size_t i = 0; i + 1 < points.size(); ++i) {
                const float radius = (points[i].thickness + points[i + 1].thickness) * canvas_scale * 0.25f;
                draw_list->AddLine(screen_point(points[i]), screen_point(points[i + 1]), color, radius * 2.0f);
                draw_list->AddCircleFilled(screen_point(points[i]), radius, color);
            }
            draw_list->AddCircleFilled(screen_point(points.back()), points.back().thickness * canvas_scale * 0.5f, color);
        };
        for (const Stroke& stroke : stroke_manager.get_strokes()) {
            const ImU32 color = stroke.pen_type == static_cast<uint32_t>(StrokeTool::Eraser)
                ? IM_COL32(255, 255, 255, 255)
                : IM_COL32(stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a);
            draw_stroke(stroke.points, color);
        }
        if (stroke_in_progress_) {
            if (const Stroke* current_stroke = stroke_manager.get_current_stroke()) {
                const ImU32 color = current_stroke->pen_type == static_cast<uint32_t>(StrokeTool::Eraser)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(current_stroke->color.r, current_stroke->color.g, current_stroke->color.b, current_stroke->color.a);
                draw_stroke(current_stroke->points, color);
            }
        }
        draw_list->PopClipRect();

        draw_list->AddRect(
            canvas_pos,
            ImVec2(canvas_pos.x + view_canvas_w, canvas_pos.y + view_canvas_h),
            IM_COL32(100, 100, 100, 255)
        );
    }

    ImGui::End();
}

void VulkanApp::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    if (imgui_vulkan_initialized_) {
        ImGui_ImplVulkan_Shutdown();
        imgui_vulkan_initialized_ = false;
    }
    if (imgui_glfw_initialized_) {
        ImGui_ImplGlfw_Shutdown();
        imgui_glfw_initialized_ = false;
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::DestroyContext();
    }

    if (device_ != VK_NULL_HANDLE && main_window_data_.Swapchain != VK_NULL_HANDLE) {
        ImGui_ImplVulkanH_DestroyWindow(instance_, device_, &main_window_data_, allocator_);
    }
    if (device_ != VK_NULL_HANDLE && descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptor_pool_, allocator_);
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, allocator_);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, allocator_);
        instance_ = VK_NULL_HANDLE;
    }

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }
}
