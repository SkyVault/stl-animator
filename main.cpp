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

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#define GUI_FILE_DIALOG_IMPLEMENTATION
#include "gui_file_dialog.h"

constexpr auto MENU_MARGIN {10};
constexpr auto TIMELINE_HEIGHT {32};
constexpr auto STATUS_BAR_HEIGHT {20};
constexpr auto TOTAL_BOTTOM_PANEL_HEIGHT {TIMELINE_HEIGHT+STATUS_BAR_HEIGHT};

enum class Interp {
    LINEAR,
};

struct KeyFrame {
    std::tuple<Transform, Transform> transform;
    Interp interpolation{Interp::LINEAR};
    int frames {60};
};

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

    Transform transform{Vector3{0,0,0}, Quaternion{0, 0, 0, 1}, Vector3{1, 1, 1}};
};

struct State {
    bool running = true;

    Shader shader {{}};

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

Model load_model(const State& state, const std::string& path) {
    auto model = LoadModel(path.c_str());
    model.materials[0].shader = state.shader;
    return model;
}

void unload_model(Model& model) {
    UnloadModel(model);
}

void draw_model(const State& state, std::tuple<Model, ModelGuiState>& model_tuple) {
    auto& [model, model_state] = model_tuple;

    const auto scale_ = model_state.transform.scale;
    const auto trans_ = model_state.transform.translation;

    auto translation = MatrixTranslate(trans_.x, trans_.y, trans_.z);
    auto rotation = QuaternionToMatrix(model_state.transform.rotation);
    auto scale = MatrixScale(scale_.x, scale_.y, scale_.z);
    auto color = model.materials[0].maps[0].color;
    auto color_v3 = Vector3{color.r, color.g, color.b};

    model.transform = MatrixMultiply(scale, MatrixMultiply(rotation, translation));

    DrawModel(model, trans_, 1.0, RAYWHITE);
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
                {load_model(state, path), {std::string{state.file_dialog_state.fileNameText}}});
        }

        state.file_dialog_state.SelectFilePressed = false;
    }

    GuiFileDialog(&state.file_dialog_state);
}

void do_objects_window(State& state) {
    auto win_reg = Rectangle{GetScreenWidth()-256, 32, 256, GetScreenHeight() - TOTAL_BOTTOM_PANEL_HEIGHT};
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
    GuiLabel(Rectangle{cursor_x, cursor_y, sub_w, bh+10}, "-- Models --");

    cursor_y += bh+10+MENU_MARGIN;
    
    static auto panel_scroll = Vector2{0.0f, 0.0f};
    auto panel_rec = Rectangle{cursor_x, cursor_y, sub_w, win_reg.height-cursor_y};
    auto panel_content_rec = Rectangle{0, 0, panel_rec.width, panel_rec.height * 4};
    auto view = GuiScrollPanel(panel_rec, panel_content_rec, &panel_scroll);

    BeginScissorMode(view.x, view.y, view.width, view.height);

    float p_cursor_x = cursor_x;
    float p_cursor_y = cursor_y;

    float height = 0;

    cursor_x = panel_rec.x+panel_scroll.x;
    cursor_y = panel_rec.y+panel_scroll.y;

    for (auto& [model, model_state] : state.models) {
        std::stringstream ss;

        // TRANSFORM
        GuiLabel(Rectangle{cursor_x, cursor_y, 100, 32}, "::[ TRANSLATION ]::");
        cursor_y += 32;

        auto* trans = &model_state.transform.translation;
        auto* scale = &model_state.transform.scale;
        auto* rotation = &model_state.transform.rotation;

        auto r = Rectangle{cursor_x + 16, cursor_y + (32-24)/2, sub_w-(64), 24};

        trans->x = GuiSlider(r, "X", TextFormat("%2.2f", (float)trans->x), trans->x, -10, 10); r.y += 32;
        trans->y = GuiSlider(r, "Y", TextFormat("%2.2f", (float)trans->y), trans->y, -10, 10); r.y += 32;
        trans->z = GuiSlider(r, "Z", TextFormat("%2.2f", (float)trans->z), trans->z, -10, 10); r.y += 32;

        cursor_y += r.height * 3 + MENU_MARGIN * 3;
        height += r.height * 3 + MENU_MARGIN * 3;

        DrawRectangle(cursor_x + MENU_MARGIN / 2, cursor_y, sub_w - MENU_MARGIN / 2, 1, Color{200, 200, 200, 255});

        cursor_y += 1 + MENU_MARGIN;

        GuiLabel(Rectangle{cursor_x, cursor_y, 100, 32}, "::[ SCALE ]::");
        cursor_y += 32;
        r.y = cursor_y;

        // SCALE
        scale->x = GuiSlider(r, "X", TextFormat("%2.2f", (float)scale->x), scale->x, 0.001f, 10.f); r.y += 32;
        scale->y = GuiSlider(r, "Y", TextFormat("%2.2f", (float)scale->y), scale->y, 0.001f, 10.f); r.y += 32;
        scale->z = GuiSlider(r, "Z", TextFormat("%2.2f", (float)scale->z), scale->z, 0.001f, 10.f);

        cursor_y = r.y + MENU_MARGIN*4;
        DrawRectangle(cursor_x + MENU_MARGIN / 2, cursor_y, sub_w - MENU_MARGIN / 2, 1, Color{200, 200, 200, 255});
        cursor_y += MENU_MARGIN;

        GuiLabel(Rectangle{cursor_x, cursor_y, 100, 32}, "::[ COLOR ]::");

        cursor_y += 32;
        height += 32;

        model.materials[0].maps[0].color =
            GuiColorPicker(Rectangle{
                    cursor_x,
                    cursor_y,
                    100, 100}, model.materials[0].maps[0].color);

        cursor_y += 100 + MENU_MARGIN;
        height += 100 + MENU_MARGIN;

        DrawRectangle(cursor_x + MENU_MARGIN/2, cursor_y, sub_w-MENU_MARGIN/2, 3, Color{200, 200, 200, 255});
    }
    EndScissorMode();

    cursor_x = p_cursor_x;
    cursor_y = p_cursor_y;
}

void do_timeline(State& state) {
    auto panel = Rectangle{0, GetScreenHeight()-TOTAL_BOTTOM_PANEL_HEIGHT, GetScreenWidth(), TIMELINE_HEIGHT};
    GuiPanel(panel);

    DrawRectangle(panel.x + 4, panel.y + 4, panel.width-8, panel.height-8, Color{50, 50, 50, 255});
}

void do_gui(State& state) {
    do_file_dialog_update(state);

    if (state.file_dialog_state.fileDialogActive) GuiLock();
    do_menu_bar(state);
    do_objects_window(state);
    do_timeline(state);
    GuiUnlock();

    std::stringstream title;
    title << "FPS: "
          << 1.0f/GetFrameTime();

    GuiStatusBar(
        (Rectangle){0, GetScreenHeight() - STATUS_BAR_HEIGHT, GetScreenWidth(), STATUS_BAR_HEIGHT},
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
    state.shader = LoadShader("resources/phong_vs.glsl", "resources/phong_fs.glsl");

    state.shader.locs[LOC_MATRIX_MODEL] = GetShaderLocation(state.shader, "matModel");
    state.shader.locs[LOC_VECTOR_VIEW] = GetShaderLocation(state.shader, "viewPos");

    int ambientLoc = GetShaderLocation(state.shader, "ambient");
    float val[] = {0.2f, 0.2f, 0.2f, 1.0f};
    SetShaderValue(state.shader, ambientLoc, val, UNIFORM_VEC4);

    Light lights[MAX_LIGHTS] = { 0 };
    lights[0] = CreateLight(LIGHT_POINT, (Vector3){ -10, 0, -10 }, (Vector3){0}, WHITE, state.shader);

    for (int i = 0; i < MAX_LIGHTS; i++) {
        UpdateLightValues(state.shader, lights[i]);
    }

    // Initialize the gui
    state.font = LoadFontEx("resources/Cantarell-Regular.otf", 16, NULL, -1);

    GuiSetFont(state.font);

    state.models.push_back(
        {load_model(state, "models/monkey.obj"), {std::string{"monkey"}}});

    while (!WindowShouldClose() && state.running) {

        // Update
        if (1) UpdateCamera(&state.camera);          // Update camera

        static float timer = 0;

        timer += GetFrameTime();

        auto pos = (Vector3){-10, 5, 10};
        for (int i = 0; i < MAX_LIGHTS; i++) {
            lights[i].position = pos;
            UpdateLightValues(state.shader, lights[i]);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(state.camera);
        for (auto& model_tuple : state.models) {
            draw_model(state, model_tuple);
        }
        DrawSphere(pos, 1, RED);
        EndMode3D();

        do_gui(state);

        EndDrawing();
    }

    return 0;
}
