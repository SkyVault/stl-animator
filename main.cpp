#include "raylib.h"

#include <string.h>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <tuple>

#include "stl_reader.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS

#include "raygui.h"
#include "rlgl.h"

constexpr auto MENU_MARGIN {10};

struct State {
    bool running = true;

    Shader shader = {};

    int model_loc = -1;
    int light_pos_loc = -1;

    Camera camera = {};

    std::vector<Model> models;

    std::vector<std::tuple<std::string, std::vector<std::string>>>
    top_menu_buttons {
        {"File",
         {"Load scene",
          "Save scene",
          "Import model",
          "Exit"}},
        {"Edit", {}},
        {"Render", {}}
    };
};

Model load_model(const std::string& path) {
    return LoadModel(path.c_str());
}

void unload_model(Model& model) {
    UnloadModel(model);
}

void draw_model(const State& state, const Model& model) {
    auto pos = (Vector3){
        model.transform.m3,
        model.transform.m6,
        model.transform.m9 };

    model.materials[0].shader = state.shader;

    SetShaderValueMatrix(state.shader, state.model_loc, model.transform);

    DrawModel(model, pos, 0.1, RAYWHITE);
}

void do_menu_bar(State& state) {
    GuiPanel(Rectangle{0, 0, GetScreenWidth(), 32});

    float cursor_x = MENU_MARGIN;

    for (const auto& [txt, options] : state.top_menu_buttons) {

        const auto w = MeasureText(txt.c_str(), GetFontDefault().baseSize);
        const auto reg = Rectangle{cursor_x, 0, w, 32};
        const auto mpos = GetMousePosition();

        cursor_x += w + MENU_MARGIN;

        bool show_drop_down =
            CheckCollisionPointRec(mpos, reg);

        if (GuiLabelButton(reg, txt.c_str())) {
            show_drop_down = true;
        }


    }
}

void draw_gui(State& state) {
    do_menu_bar(state);

    GuiPanel(Rectangle{0, 32, 200, 400});

    GuiStatusBar(
        (Rectangle){ 0, GetScreenHeight() - 20, GetScreenWidth(), 20 },
        "");
}

int main () {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Hello World");
    SetTargetFPS(60);

    State state;

    // Camera initialization
    state.camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    state.camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    state.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state.camera.fovy = 45.0f;
    state.camera.type = CAMERA_PERSPECTIVE;
    SetCameraMode(state.camera, CAMERA_FREE); // Set a free camera mode

    // Shader initialization
    state.shader = LoadShader("base_vs.glsl", "base_fs.glsl");
    state.model_loc = GetShaderLocation(state.shader, "model");
    state.light_pos_loc = GetShaderLocation(state.shader, "light_pos");

    // TEMP
    state.models.push_back(load_model("models/z-assm.obj"));

    while (!WindowShouldClose() && state.running) {

        // Update
        UpdateCamera(&state.camera);          // Update camera

        SetShaderValue(
            state.shader,
            state.light_pos_loc,
            &state.camera.position,
            UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(state.camera);
            for (const auto& model : state.models) {
                draw_model(state, model);
            }
        EndMode3D();

        draw_gui(state);
        DrawFPS(100, 100);

        EndDrawing();
    }

    return 0;
}
