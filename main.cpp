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

    Camera camera = {};
};

Mesh load_mesh(const std::string& path) {
}

void unload_mesh(Mesh& mesh) {
    UnloadMesh(mesh);
}

void draw_mesh(const std::unique_ptr<mesh_t>& mesh) {
}

void draw_gui(const State& state) {
    GuiPanel(Rectangle{0, 0, 200, 400});

    if (GuiButton((Rectangle){0, 0, 100, 20}, "Hello World")) {

    }
}

int main () {
    InitWindow(1280, 720, "Hello World");

    SetTargetFPS(60);

    State state;

    state.camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    state.camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    state.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state.camera.fovy = 45.0f;
    state.camera.type = CAMERA_PERSPECTIVE;
    SetCameraMode(state.camera, CAMERA_FREE); // Set a free camera mode

    auto mesh = load_mesh("models/z-assm.stl");

    while (!WindowShouldClose() && state.running) {

        // Update
        UpdateCamera(&state.camera);          // Update camera

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(state.camera);

        EndMode3D();

        draw_gui(state);

        DrawFPS(100, 100);

        EndDrawing();
    }

    return 0;
}
