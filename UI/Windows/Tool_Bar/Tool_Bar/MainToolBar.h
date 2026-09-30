#pragma once
#include "NewCanvasDialog.h"

class CanvasManager;

class MainToolBar {
public:
    MainToolBar();

    void render(CanvasManager& canvas_manager);
    void open_new_canvas() { new_canvas_dialog_.open(); }

private:
    NewCanvasDialog new_canvas_dialog_;
};
