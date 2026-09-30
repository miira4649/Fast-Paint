#include "VulkanApp.h"
#include <iostream>

int main() {
    try {
        // フルHD (1920x1080) でアプリケーション起動
        VulkanApp app(1920, 1080, "Fast Paint - Core Engine");
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "エラーが発生しました: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}