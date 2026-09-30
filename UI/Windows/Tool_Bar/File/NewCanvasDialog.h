#pragma once

class CanvasManager;

class NewCanvasDialog {
public:
    NewCanvasDialog();

    void open();
    void render(CanvasManager& canvas_manager);

private:
    bool is_open_ = false;
    int input_width_ = 1920;
    int input_height_ = 1080;
};