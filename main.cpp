#include "raylib.h"

#include <string.h>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>

#include "stl_reader.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS

#include "raygui.h"
#include "rlgl.h"

struct State {
    bool running = true;

    Shader shader = {};

    int model_loc = -1;
    int light_pos_loc = -1;

    Camera camera = {};
};

Model load_model(const std::string& path) {
    return LoadModel(path.c_str());
}

void unload_model(Model& model) {
    UnloadModel(model);
}

void draw_model(State& state, Model& model) {
    auto pos = (Vector3){
        model.transform.m3,
        model.transform.m6,
        model.transform.m9 };

    model.materials[0].shader = state.shader;

    SetShaderValueMatrix(state.shader, state.model_loc, model.transform);

    DrawModel(model, pos, 0.1, RAYWHITE);
}

void draw_gui(const State& state) {
    GuiPanel(Rectangle{0, 0, 200, 400});

    if (GuiButton((Rectangle){0, 0, 100, 20}, "Hello World")) {

    }
}

int main () {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Hello World");
    SetTargetFPS(60);

    State state;

    state.camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    state.camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    state.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state.camera.fovy = 45.0f;
    state.camera.type = CAMERA_PERSPECTIVE;
    SetCameraMode(state.camera, CAMERA_FREE); // Set a free camera mode

    state.shader = LoadShader("base_vs.glsl", "base_fs.glsl");
    state.model_loc = GetShaderLocation(state.shader, "model");
    state.light_pos_loc = GetShaderLocation(state.shader, "light_pos");

    auto model = load_model("models/z-assm.obj");

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
            draw_model(state, model);
        EndMode3D();

        draw_gui(state);
        DrawFPS(100, 100);

        EndDrawing();
    }

    return 0;
}
