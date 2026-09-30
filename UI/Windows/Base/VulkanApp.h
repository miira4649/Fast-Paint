#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "imgui_impl_vulkan.h"
#include "CanvasManager.h"
#include "MainToolBar.h"
#include "ToolSidebar.h"

class VulkanApp {
public:
    VulkanApp(int width, int height, const char* title);
    ~VulkanApp();

    void run();

private:
    void init_window();
    void init_vulkan();
    void init_imgui();
    void cleanup();
    void main_loop();
    void render_canvas_area();

    void frame_render(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
    void frame_present(ImGui_ImplVulkanH_Window* wd);

    int width_;
    int height_;
    const char* title_;

    GLFWwindow* window_ = nullptr;
    VkAllocationCallbacks* allocator_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queue_family_ = (uint32_t)-1;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;

    ImGui_ImplVulkanH_Window main_window_data_;
    int min_image_count_ = 2;
    bool swapchain_rebuild_ = false;
    bool glfw_initialized_ = false;
    bool imgui_glfw_initialized_ = false;
    bool imgui_vulkan_initialized_ = false;
    bool stroke_in_progress_ = false;

    CanvasManager canvas_manager_;
    MainToolBar main_tool_bar_;
    ToolSidebar tool_sidebar_;
};
