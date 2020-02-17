#include "raylib.h"

#include <string.h>
#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <tuple>
#include <sstream>

#include "stl_reader.h"

#define RAYGUI_IMPLEMENTATION
#define RAYGUI_SUPPORT_ICONS

#include "raygui.h"
#include "rlgl.h"

#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "gui_file_dialog.h"

constexpr auto MENU_MARGIN {10};

struct MenuButtonState {
    std::string label {"Menu"};
    std::vector<std::string> sub_options;
    bool show_dropdown {false};
};

struct ModelGuiState {
    std::string name {"Model"};

    int active {0};
    int edit {1};

    bool selected {false};
};

struct State {
    bool running = true;

    Shader shader {{}};

    int model_loc {-1};
    int light_pos_loc {-1};
    int diffuse_loc {-1};

    Camera camera {{}};
    Font font {{}};

    GuiFileDialogState file_dialog_state;

    std::vector<std::tuple<Model, ModelGuiState>> models;

    std::vector<MenuButtonState> menu_buttons {
        {"File",
         {"Load scene",
          "Save scene",
          "Import model",
          "Exit"}},
        {"Edit",
         {"Why are you trying to edit something?"}},
        {"Render", {}},
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
    SetShaderValue(
        state.shader,
        state.diffuse_loc,
        &model.materials[0].maps[0].color,
        UNIFORM_VEC4);

    DrawModel(model, pos, 0.1, RAYWHITE);
}

void do_menu_bar(State& state) {
    const auto font_size = state.font.baseSize;

    GuiPanel(Rectangle{0, 0, GetScreenWidth(), 32});

    float cursor_x = MENU_MARGIN;

    // reset the dropdown state
    bool last_show_dropdown = false;
    bool first = true;
    for (auto& m_state : state.menu_buttons) {
        if (first) {
            last_show_dropdown = m_state.show_dropdown;
            first = false;
            continue;
        }

        if (last_show_dropdown) {
            last_show_dropdown = m_state.show_dropdown;
            m_state.show_dropdown = false;
        }
    }

    for (auto& m_state : state.menu_buttons) {
        const auto [txt, options, show_dropdown] = m_state;

        const auto [w, h] = MeasureTextEx(state.font, txt.c_str(), font_size, 1);
        const auto reg = Rectangle{cursor_x, 0, w, 32};
        const auto mpos = GetMousePosition();

        if (!show_dropdown) m_state.show_dropdown = CheckCollisionPointRec(mpos, reg);

        if (GuiLabelButton(reg, txt.c_str())) {
            m_state.show_dropdown = true;
        }

        if (show_dropdown) {
            float cursor_y = 32;

            float panel_w = 0, panel_h = 0;
            for (const auto& option : options) {
                const auto [w, h] = MeasureTextEx(state.font, option.c_str(), font_size, 1);
                if (w > panel_w) panel_w = w;
                panel_h += h;
            }

            const auto drop_panel_reg = Rectangle{
                cursor_x-MENU_MARGIN,
                32,
                panel_w+MENU_MARGIN*2,
                panel_h+MENU_MARGIN*2};

            auto drop_panel_collider = drop_panel_reg;
            drop_panel_collider.y -= 32;
            drop_panel_collider.height += 32;

            GuiPanel(drop_panel_reg);

            for (const auto& option : options) {

                const auto reg = Rectangle{cursor_x, cursor_y+MENU_MARGIN, w, MENU_MARGIN*2};
                if (GuiLabelButton(reg, option.c_str())) {
                    //TODO
                }

                cursor_y += MENU_MARGIN*2;
            }

            if (!CheckCollisionPointRec(mpos, drop_panel_collider)) {
                m_state.show_dropdown = false;
            }
        }

        cursor_x += w + MENU_MARGIN;
    }
}

void do_file_dialog_update(State& state) {
    if (state.file_dialog_state.SelectFilePressed) {
        // Load file

        if (!IsFileExtension(state.file_dialog_state.fileNameText, ".obj")) {
            // DO WARN
        } else {
            // Load the model

            const auto path = "models/" + std::string{state.file_dialog_state.fileNameText};

            state.models.push_back(
                {load_model(path), {std::string{state.file_dialog_state.fileNameText}}});
        }

        state.file_dialog_state.SelectFilePressed = false;
    }

    GuiFileDialog(&state.file_dialog_state);
}

void do_objects_window(State& state) {
    auto win_reg = Rectangle{GetScreenWidth()-256, 32, 256, GetScreenHeight() - 128};
    GuiWindowBox(win_reg, "Inspector");

    if (!CheckCollisionPointRec(GetMousePosition(), win_reg)) GuiLock();
    else GuiUnlock();

    float cursor_x = win_reg.x+MENU_MARGIN, cursor_y = win_reg.y+32;
    const float sub_w = win_reg.width - MENU_MARGIN*2;

    const auto [bw, bh] = MeasureTextEx(state.font, "#198#Load model", state.font.baseSize, 1);
    if (GuiButton(Rectangle{cursor_x, cursor_y, sub_w, bh+10}, "#198#Load model")) {
        // Load a model
        state.file_dialog_state.fileDialogActive = true;
    }

    cursor_y += bh+10+MENU_MARGIN;
    GuiLabel(Rectangle{
            cursor_x,
            cursor_y,
            sub_w,
            bh+10}, "-- Models --");

    cursor_y += bh+10+MENU_MARGIN;
    
    static auto panel_scroll = Vector2{0.0f, 0.0f};
    auto panel_rec = Rectangle{cursor_x, cursor_y, sub_w, win_reg.height-cursor_y};
    auto panel_content_rec = Rectangle{0, 0, panel_rec.width, panel_rec.height * 4};
    auto view = GuiScrollPanel(panel_rec, panel_content_rec, &panel_scroll);

    BeginScissorMode(view.x, view.y, view.width, view.height);

    float p_cursor_x = cursor_x;
    float p_cursor_y = cursor_y;

    cursor_x = panel_rec.x+panel_scroll.x;
    cursor_y = panel_rec.y+panel_scroll.y;

    for (auto& [model, model_state] : state.models) {
        std::stringstream ss;
        ss << "[Location] x: " << model.transform.m3
           << " y: " << model.transform.m6
           << " z: " << model.transform.m9;

        GuiLabel(Rectangle{
                cursor_x,
                cursor_y,
                100, 32}, ss.str().c_str());
        cursor_y += 32 + MENU_MARGIN;

        ss.str("");

        ss << "[Color]";
        GuiLabel(Rectangle{
                cursor_x,
                cursor_y,
                100, 32}, ss.str().c_str());
        cursor_y += 32 + MENU_MARGIN;

        model.materials[0].maps[0].color =
            GuiColorPicker(Rectangle{
                    cursor_x,
                    cursor_y,
                    100, 100}, model.materials[0].maps[0].color);
    }
    EndScissorMode();

    cursor_x = p_cursor_x;
    cursor_y = p_cursor_y;
}

void do_gui(State& state) {
    do_file_dialog_update(state);

    if (state.file_dialog_state.fileDialogActive) GuiLock();
    do_menu_bar(state);
    do_objects_window(state);
    GuiUnlock();

    std::stringstream title;
    title << "FPS: "
          << 1.0f/GetFrameTime();

    GuiStatusBar(
        (Rectangle){ 0, GetScreenHeight() - 20, GetScreenWidth(), 20 },
        title.str().c_str());
}

int main () {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Hello World");
    SetTargetFPS(60);

    State state;

    state.file_dialog_state = InitGuiFileDialog();

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
    state.diffuse_loc = GetShaderLocation(state.shader, "diffuse");

    // Initialize the gui
    state.font = LoadFontEx("resources/Cantarell-Regular.otf", 16, NULL, -1);

    GuiSetFont(state.font);

    while (!WindowShouldClose() && state.running) {

        // Update
        if (1) UpdateCamera(&state.camera);          // Update camera

        SetShaderValue(
            state.shader,
            state.light_pos_loc,
            &state.camera.position,
            UNIFORM_VEC3);

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(state.camera);
        for (const auto& [model, _] : state.models) {
            draw_model(state, model);
        }
        EndMode3D();

        do_gui(state);

        EndDrawing();
    }

    return 0;
}
