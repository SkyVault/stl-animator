#include "raylib.h"

#include <string>
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

constexpr auto MARGIN {10};
constexpr auto TIMELINE_HEIGHT {48};
constexpr auto STATUS_BAR_HEIGHT {20};
constexpr auto TOTAL_BOTTOM_PANEL_HEIGHT {TIMELINE_HEIGHT+STATUS_BAR_HEIGHT};

constexpr auto FRAMES_A_SECOND {60};

struct ToggleDropdownState {
    bool open{false};
};

enum class Interp {
    LINEAR,
};

enum class PlaybackState {
    PLAYING,
    STOPPED,
    PAUSED,
};

struct KeyFrame {
    Transform transform;
    Interp interpolation{Interp::LINEAR};

    int start_frame {0}; // Where the keyframe is located in time
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

    bool selected{false};

    Color blend{RAYWHITE};
    float blend_timer{0};

    Transform transform{Vector3{0,0,0}, Quaternion{0, 0, 0, 1}, Vector3{1, 1, 1}};

    std::vector<KeyFrame> keyframes{};
};

struct State {
    Shader shader {{}};

    Camera camera {{}};
    Font font {{}};

    GuiFileDialogState file_dialog_state;

    ToggleDropdownState toggle_drop_down_states[100];

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

    PlaybackState playback_state {PlaybackState::STOPPED};

    int frame_selected {-1};
    int model_selected {-1};

    bool running = true;
    bool window_locked {false};
};

void Lock(State& state) { state.window_locked = true; }
void UnLock(State& state) { state.window_locked = false; }
bool Locked(State& state) { return state.window_locked; }

bool GuiDropDown(State& state, int id, Rectangle rect, const char* text, int flags) {
    auto* dstate = &state.toggle_drop_down_states[id];

    std::string txt{text};

    if (dstate->open) {
        constexpr auto grow {4};
        rect.x -= grow;
        rect.width += grow*2;
        rect.y -= grow;
        rect.height += grow*2;
    }

    if (GuiButton(rect, txt.c_str())){
        dstate->open = !dstate->open;
    }

    return dstate->open;
}

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

    auto blend = model_state.blend;
    if (model_state.selected) {
        float t = 0.5 + (cos(model_state.blend_timer)/2);
        blend = Color{255, (t/2+0.5)*255, (t/2+0.5)*255, 255};
        model_state.blend_timer += GetFrameTime()*10.0f;
    }

    DrawModel(model, trans_, 1.0, blend);
}

void do_menu_bar(State& state) {
    const auto font_size = state.font.baseSize;

    GuiPanel(Rectangle{0, 0, GetScreenWidth(), 32});

    float cursor_x = MARGIN;

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
                panel_h += h+MARGIN;
            }

            const auto drop_panel_reg = Rectangle{
                cursor_x-MARGIN,
                32,
                panel_w+MARGIN*2,
                panel_h+MARGIN*2};

            auto drop_panel_collider = drop_panel_reg;
            drop_panel_collider.y -= 32;
            drop_panel_collider.height += 32;

            GuiPanel(drop_panel_reg);

            for (const auto& option : options) {

                const auto reg = Rectangle{cursor_x, cursor_y+MARGIN, w, MARGIN*2};
                if (GuiLabelButton(reg, option.c_str())) {
                    //TODO
                }

                cursor_y += MARGIN*2;
            }

            if (!CheckCollisionPointRec(mpos, drop_panel_collider)) {
                m_state.show_dropdown = false;
            }
        }

        cursor_x += w + MARGIN;
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

    if (CheckCollisionPointRec(GetMousePosition(), win_reg)) Lock(state);

    float cursor_x = win_reg.x+MARGIN, cursor_y = win_reg.y+32;
    const float sub_w = win_reg.width - MARGIN*2;

    const auto [bw, bh] = MeasureTextEx(state.font, "#198#Load model", state.font.baseSize, 1);
    if (GuiButton(Rectangle{cursor_x, cursor_y, sub_w, bh+10}, "#198#Load model")) {
        // Load a model
        state.file_dialog_state.fileDialogActive = true;
    }

    cursor_y += bh+10+MARGIN;
    GuiLabel(Rectangle{cursor_x, cursor_y, sub_w, bh+10}, "-- Models --");

    cursor_y += bh+10+MARGIN;
    
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

    int i {0};
    for (auto& [model, model_state] : state.models) {
        std::stringstream ss;

        cursor_y += MARGIN;

        std::string s = "Model [" + std::to_string(i) + "]";

        model_state.selected = false;
        if (GuiDropDown(state, i, Rectangle{cursor_x, cursor_y, sub_w, bh+10}, s.c_str(), 0)) {
            model_state.selected = true;
            state.model_selected = i;

            cursor_y += bh+10+MARGIN;

            if (GuiButton(Rectangle{cursor_x+MARGIN, cursor_y, sub_w-MARGIN*3, bh+10}, "#48#Insert Keyframe")) {
                model_state.keyframes.push_back(KeyFrame{
                        model_state.transform,
                        Interp::LINEAR,
                        (state.frame_selected<0)?0:state.frame_selected
                    });
            }
            cursor_y += bh+10+MARGIN;

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

            cursor_y += r.height * 3 + MARGIN * 3;
            height += r.height * 3 + MARGIN * 3;

            DrawRectangle(cursor_x + MARGIN / 2, cursor_y, sub_w - MARGIN / 2, 1, Color{200, 200, 200, 255});

            cursor_y += 1 + MARGIN;

            GuiLabel(Rectangle{cursor_x, cursor_y, 100, 32}, "::[ SCALE ]::");
            cursor_y += 32;
            r.y = cursor_y;

            // SCALE
            scale->x = GuiSlider(r, "X", TextFormat("%2.2f", (float)scale->x), scale->x, 0.001f, 10.f); r.y += 32;
            scale->y = GuiSlider(r, "Y", TextFormat("%2.2f", (float)scale->y), scale->y, 0.001f, 10.f); r.y += 32;
            scale->z = GuiSlider(r, "Z", TextFormat("%2.2f", (float)scale->z), scale->z, 0.001f, 10.f);

            cursor_y = r.y + MARGIN*4;
            DrawRectangle(cursor_x + MARGIN / 2, cursor_y, sub_w - MARGIN / 2, 1, Color{200, 200, 200, 255});
            cursor_y += MARGIN;

            GuiLabel(Rectangle{cursor_x, cursor_y, 100, 32}, "::[ COLOR ]::");

            cursor_y += 32;
            height += 32;

            model.materials[0].maps[0].color =
                GuiColorPicker(Rectangle{
                        cursor_x,
                        cursor_y,
                        100, 100}, model.materials[0].maps[0].color);

            cursor_y += 100 + MARGIN;
            height += 100 + MARGIN;
        }

        cursor_y += bh+10+MARGIN;

        DrawRectangle(cursor_x + MARGIN/2, cursor_y, sub_w-MARGIN/2, 3, Color{200, 200, 200, 255});

        i++;
    }

    EndScissorMode();

    cursor_x = p_cursor_x;
    cursor_y = p_cursor_y;
}

void do_timeline(State& state) {
    auto panel = Rectangle{0, GetScreenHeight()-TOTAL_BOTTOM_PANEL_HEIGHT, GetScreenWidth(), TIMELINE_HEIGHT};
    GuiPanel(panel);

    constexpr auto frame_width = 16;
    const auto btn_size = panel.height - 8;
    const auto mouse_pos = GetMousePosition();
    // Lock the ui
    if (CheckCollisionPointRec(mouse_pos, panel)) {
        Lock(state);
    }

    DrawRectangle(panel.x + 4, panel.y + 4, panel.width-8, panel.height-8, Color{50, 50, 50, 255});

    auto cursor_x = 0;

    char buff[100] = {0};
    //STOP
    if (GuiButton(Rectangle{cursor_x+panel.x + 4, panel.y + 4, btn_size, btn_size}, "#133#")) {
        state.playback_state = PlaybackState::STOPPED;
    }
    cursor_x += panel.height-8;

    //PLAY/PAUSE
    sprintf(buff, "%s", (state.playback_state == PlaybackState::PLAYING?"#132#":"#131#"));
    if (GuiButton(Rectangle{cursor_x+panel.x + 4, panel.y + 4, btn_size, btn_size}, buff)) {
        if (state.playback_state == PlaybackState::PLAYING)
            state.playback_state = PlaybackState::PAUSED;
        else
            state.playback_state = PlaybackState::PLAYING;
    }
    cursor_x += panel.height-8;

    //SCROLL LEFT
    if (GuiButton(Rectangle{cursor_x+panel.x + 4, panel.y + 4, btn_size, btn_size/2}, "#114#")) {
        
    }

    // SCROLL RIGHT
    if (GuiButton(Rectangle{cursor_x+panel.x + 4, panel.y + 4+btn_size/2, btn_size, btn_size/2}, "#115#")) {

    }
    cursor_x += panel.height-8;

    // We have a max animation length of a minute at 60 frames a second
    for (int i = 0; i < (FRAMES_A_SECOND*60); i++) {
        auto c = i == state.frame_selected ? Color{255, 0, 255, 255} : Color{100, 100, 100, 255};
        auto r = Rectangle{cursor_x + panel.x + 4 + frame_width * i, panel.y + 4, frame_width, panel.height-8};
        DrawRectangle(r.x, r.y, r.width, r.height, c);
        DrawRectangle(cursor_x + (panel.x + 4 + frame_width * i) + frame_width - 1, panel.y + 4, 1, panel.height-8, Color{0, 0, 0, 255});

        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) continue;
        if (CheckCollisionPointRec(mouse_pos, r)){
            state.frame_selected = i;
        }
    }

    if (state.model_selected >= 0){
        const auto* selected_model = &state.models[state.model_selected];
        for (const auto& frame : std::get<1>(*selected_model).keyframes) {
            auto color = BLUE;
            if (frame.start_frame == state.frame_selected)
                color = (Color){100, 0, 255, 255};
            DrawRectangle(
                cursor_x + panel.x + 4 + frame_width * frame.start_frame,
                panel.y + 4, frame_width, panel.height-8, color);
        }
    }
}

void do_gui(State& state) {
    UnLock(state);

    std::stringstream title;
    title << "FPS: "
          << 1.0f/GetFrameTime();

    const auto status_region = (Rectangle){0, GetScreenHeight() - STATUS_BAR_HEIGHT, GetScreenWidth(), STATUS_BAR_HEIGHT};
    const auto mouse_pos = GetMousePosition();
    GuiStatusBar(
        status_region,
        title.str().c_str());
    if (CheckCollisionPointRec(mouse_pos, status_region)) Lock(state);

    do_file_dialog_update(state);

    if (state.file_dialog_state.fileDialogActive) {
        GuiLock();
        Lock(state);
    }

    do_menu_bar(state);
    do_objects_window(state);
    do_timeline(state);

    if (!state.file_dialog_state.fileDialogActive) {
        GuiUnlock();
    }
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

    state.models.push_back({load_model(state, "models/monkey.obj"), {std::string{"monkey"}}});

    while (!WindowShouldClose() && state.running) {

        // Update
        if (!Locked(state)) {
            printf("locked%d\n", rand());
            UpdateCamera(&state.camera);          // Update camera
        }

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
        DrawSphere(pos, 1, YELLOW);
        DrawGizmo(Vector3{-5, 0, -5});

        EndMode3D();

        do_gui(state);

        EndDrawing();
    }

    return 0;
}
