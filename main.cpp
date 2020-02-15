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

struct State {
    bool running = true;

    Camera camera = {};
};

struct mesh_t {
    std::vector<float> vertices, normals;
    std::vector<unsigned int> tris, solids;
};

void each_vert(std::function<void(float*)> fn, std::unique_ptr<mesh_t>& mesh) {
    const size_t numTris = mesh->tris.size() / 3;
    for(size_t itri = 0; itri < numTris; ++itri) {
        std::cout << "coordinates of triangle " << itri << ": ";
        for(size_t icorner = 0; icorner < 3; ++icorner) {
            float* c = &mesh->vertices[3 * mesh->tris[3 * itri + icorner]];

            fn(c);
        }
    }
}

std::unique_ptr<mesh_t> load_mesh(const std::string& path) {
    mesh_t mesh;

    auto result = std::make_unique<mesh_t>();

    try {
        stl_reader::ReadStlFile(
            path.c_str(),
            mesh.vertices,
            mesh.normals,
            mesh.tris,
            mesh.solids);

        const size_t numTris = mesh.tris.size() / 3;
        for(size_t itri = 0; itri < numTris; ++itri) {
            std::cout << "coordinates of triangle " << itri << ": ";
            for(size_t icorner = 0; icorner < 3; ++icorner) {
                float* c = &mesh.vertices[3 * mesh.tris[3 * itri + icorner]];
                result->vertices.push_back(c[0]);
                result->vertices.push_back(c[1]);
                result->vertices.push_back(c[2]);
                std::cout << "(" << c[0] << ", " << c[1] << ", " << c[2] << ") ";
            }
            std::cout << std::endl;

            float* n = &mesh.normals[3 * itri];
            result->normals.push_back(n[0]);
            result->normals.push_back(n[1]);
            result->normals.push_back(n[2]);
            std::cout   << "normal of triangle " << itri << ": "
                        << "(" << n[0] << ", " << n[1] << ", " << n[2] << ")\n";
        }
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    return result;
}

void draw_mesh(const std::unique_ptr<Mesh>& mesh) {
    each_vert([](float* v){

              }, mesh);
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

    load_mesh("models/ico.stl");

    while (!WindowShouldClose() && state.running) {

        // Update
        UpdateCamera(&state.camera);          // Update camera

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(state.camera);
            DrawCube((Vector3){-4.0f, 0.0f, 2.0f}, 2.0f, 5.0f, 2.0f, RED);
            DrawCubeWires((Vector3){-4.0f, 0.0f, 2.0f}, 2.0f, 5.0f, 2.0f, GOLD);
            DrawCubeWires((Vector3){-4.0f, 0.0f, -2.0f}, 3.0f, 6.0f, 2.0f, MAROON);

            DrawSphere((Vector3){-1.0f, 0.0f, -2.0f}, 1.0f, GREEN);
            DrawSphereWires((Vector3){1.0f, 0.0f, 2.0f}, 2.0f, 16, 16, LIME);

            DrawCylinder((Vector3){4.0f, 0.0f, -2.0f}, 1.0f, 2.0f, 3.0f, 4, SKYBLUE);
            DrawCylinderWires((Vector3){4.0f, 0.0f, -2.0f}, 1.0f, 2.0f, 3.0f, 4, DARKBLUE);
            DrawCylinderWires((Vector3){4.5f, -1.0f, 2.0f}, 1.0f, 1.0f, 2.0f, 6, BROWN);

            DrawCylinder((Vector3){1.0f, 0.0f, -4.0f}, 0.0f, 1.5f, 3.0f, 8, GOLD);
            DrawCylinderWires((Vector3){1.0f, 0.0f, -4.0f}, 0.0f, 1.5f, 3.0f, 8, PINK);

            DrawGrid(10, 1.0f);        // Draw a grid

        EndMode3D();

        draw_gui(state);

        EndDrawing();
    }

    return 0;
}
