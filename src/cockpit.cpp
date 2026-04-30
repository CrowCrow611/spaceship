#include "cockpit.h"
#include "scenenode.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// ── colour palette ──────────────────────────────────────────────────────────
static const float M1[] = {0.05f, 0.06f, 0.08f}; // darkest metal
static const float M2[] = {0.08f, 0.09f, 0.12f}; // mid metal
static const float M3[] = {0.12f, 0.13f, 0.17f}; // light metal / console surface
static const float AB[] = {0.95f, 0.65f, 0.10f}; // bright amber
static const float AM[] = {0.65f, 0.40f, 0.05f}; // mid amber
static const float AD[] = {0.30f, 0.18f, 0.02f}; // dim amber
static const float GP[] = {0.10f, 0.90f, 0.20f}; // bright green phosphor
static const float GD[] = {0.04f, 0.35f, 0.08f}; // dim green

// shorthand: add a quad to a node using a const float[3] colour
#define NQ(nd, ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, col, alp) \
    nd.addQuad({ax,ay,az},{bx,by,bz},{cx,cy,cz},{dx,dy,dz}, \
               (col)[0],(col)[1],(col)[2], alp)

static SceneNode cockpitRoot;
static bool      initialized = false;

// ── solid box helper ────────────────────────────────────────────────────────
static void addBox(SceneNode& nd,
                   float x0,float y0,float z0,
                   float x1,float y1,float z1,
                   const float* col, float alpha = 1.0f) {
    NQ(nd, x0,y0,z0, x1,y0,z0, x1,y1,z0, x0,y1,z0, col,alpha); // front
    NQ(nd, x0,y0,z1, x1,y0,z1, x1,y1,z1, x0,y1,z1, col,alpha); // back
    NQ(nd, x0,y0,z0, x0,y0,z1, x0,y1,z1, x0,y1,z0, col,alpha); // left
    NQ(nd, x1,y0,z0, x1,y0,z1, x1,y1,z1, x1,y1,z0, col,alpha); // right
    NQ(nd, x0,y1,z0, x1,y1,z0, x1,y1,z1, x0,y1,z1, col,alpha); // top
    NQ(nd, x0,y0,z0, x1,y0,z0, x1,y0,z1, x0,y0,z1, col,alpha); // bottom
}

// ── build scene tree (called once) ─────────────────────────────────────────
void initCockpit() {
    cockpitRoot      = SceneNode();
    cockpitRoot.name = "root";

    // ── ROOM ─────────────────────────────────────────────────────────────
    auto& room = cockpitRoot.addChild("room");
    NQ(room, -3,-1,-5,  3,-1,-5,  3,-1, 3, -3,-1, 3,  M2, 1); // floor
    NQ(room, -3, 2,-5,  3, 2,-5,  3, 2, 3, -3, 2, 3,  M1, 1); // ceiling
    NQ(room, -3,-1,-5, -3,-1, 3, -3, 2, 3, -3, 2,-5,  M2, 1); // left wall
    NQ(room,  3,-1,-5,  3,-1, 3,  3, 2, 3,  3, 2,-5,  M2, 1); // right wall
    NQ(room, -3,-1, 3,  3,-1, 3,  3, 2, 3, -3, 2, 3,  M1, 1); // back wall
    // front wall with window cutout
    NQ(room, -3,-1,-4.9f, -1.5f,-1,-4.9f, -1.5f, 2,-4.9f, -3, 2,-4.9f,   M2,1);
    NQ(room,  1.5f,-1,-4.9f,  3,-1,-4.9f,  3, 2,-4.9f,  1.5f, 2,-4.9f,   M2,1);
    NQ(room, -1.5f,1.8f,-4.9f, 1.5f,1.8f,-4.9f, 1.5f,2,-4.9f,-1.5f,2,-4.9f, M2,1);
    NQ(room, -1.5f,-1,-4.9f, 1.5f,-1,-4.9f, 1.5f,-0.3f,-4.9f,-1.5f,-0.3f,-4.9f, M2,1);

    // window frame strips
    auto& wframe = cockpitRoot.addChild("window_frame");
    NQ(wframe, -1.54f,-0.34f,-4.88f,-1.50f,-0.34f,-4.88f,-1.50f,1.84f,-4.88f,-1.54f,1.84f,-4.88f, AB,1);
    NQ(wframe,  1.50f,-0.34f,-4.88f, 1.54f,-0.34f,-4.88f, 1.54f,1.84f,-4.88f, 1.50f,1.84f,-4.88f, AB,1);
    NQ(wframe, -1.54f,1.80f,-4.88f, 1.54f,1.80f,-4.88f, 1.54f,1.84f,-4.88f,-1.54f,1.84f,-4.88f,   AB,1);
    NQ(wframe, -1.54f,-0.34f,-4.88f,1.54f,-0.34f,-4.88f,1.54f,-0.30f,-4.88f,-1.54f,-0.30f,-4.88f, AB,1);

    // floor / wall trim
    auto& trim = cockpitRoot.addChild("trim");
    NQ(trim, -3,-0.98f,-5, 3,-0.98f,-5, 3,-0.95f,3,-3,-0.95f,3,   AD,1);
    NQ(trim, -2.98f,-1,-5,-2.94f,-1,-5,-2.94f,2,3,-2.98f,2,3,     AD,1);
    NQ(trim,  2.94f,-1,-5, 2.98f,-1,-5, 2.98f,2,3, 2.94f,2,3,     AD,1);

    // ── PILOT CHAIR ──────────────────────────────────────────────────────
    // pivot matches original drawRotatedQuad pivot point
    auto& chair = cockpitRoot.addChild("chair");
    chair.pivot = {0.0f, -0.72f, 0.6f};
    chair.onUpdate = [](SceneNode& self, const GameState& gs, float) {
        self.rotation.y = gs.chairAngle; // scene graph handles the rotation math
    };
    NQ(chair, -0.45f,-0.72f,0.2f, 0.45f,-0.72f,0.2f, 0.45f,-0.72f,1.0f,-0.45f,-0.72f,1.0f, M3,1); // seat top
    NQ(chair, -0.45f,-1.0f, 1.0f, 0.45f,-1.0f, 1.0f, 0.45f,-0.72f,1.0f,-0.45f,-0.72f,1.0f, M3,1); // seat front
    NQ(chair, -0.4f,-0.72f,1.0f, 0.4f,-0.72f,1.0f, 0.4f,0.55f,0.55f,-0.4f,0.55f,0.55f,     M3,1); // backrest
    NQ(chair, -0.3f,0.5f,0.50f, 0.3f,0.5f,0.50f, 0.3f,0.72f,0.40f,-0.3f,0.72f,0.40f,       M2,1); // headrest
    NQ(chair, -0.50f,-0.72f,0.2f,-0.45f,-0.72f,0.2f,-0.45f,-0.50f,0.85f,-0.50f,-0.50f,0.85f,M3,1); // left arm
    NQ(chair,  0.45f,-0.72f,0.2f, 0.50f,-0.72f,0.2f, 0.50f,-0.50f,0.85f, 0.45f,-0.50f,0.85f,M3,1); // right arm
    NQ(chair, -0.46f,-0.73f,0.19f,0.46f,-0.73f,0.19f,0.46f,-0.73f,1.01f,-0.46f,-0.73f,1.01f,AM,1); // trim

    // tablet arm + screen (child of chair — rotates with it for free)
    auto& tablet = chair.addChild("tablet");
    NQ(tablet, 0.50f,-0.52f,0.55f, 0.65f,-0.52f,0.55f, 0.65f,-0.50f,0.30f, 0.50f,-0.50f,0.30f, M2,  1   );
    NQ(tablet, 0.55f,-0.48f,0.28f, 0.93f,-0.48f,0.28f, 0.93f,-0.48f,0.53f, 0.55f,-0.48f,0.53f, GD,  1   );
    NQ(tablet, 0.57f,-0.47f,0.30f, 0.93f,-0.47f,0.30f, 0.93f,-0.47f,0.53f, 0.57f,-0.47f,0.53f, GP,  0.3f);
    NQ(tablet, 0.54f,-0.49f,0.27f, 0.96f,-0.49f,0.27f, 0.96f,-0.49f,0.56f, 0.54f,-0.49f,0.56f, AM,  0.8f);

    // ── CONSOLE ──────────────────────────────────────────────────────────
    auto& console = cockpitRoot.addChild("console");
    NQ(console, -2,-0.42f,-1.8f, 2,-0.42f,-1.8f, 2,-0.42f,-2.8f,-2,-0.42f,-2.8f, M3,1); // top
    NQ(console, -2,-1,-1.8f, 2,-1,-1.8f, 2,-0.42f,-1.8f,-2,-0.42f,-1.8f,         M2,1); // front
    NQ(console, -2,-1,-1.8f,-2,-1,-2.8f,-2,-0.42f,-2.8f,-2,-0.42f,-1.8f,         M2,1); // left
    NQ(console,  2,-1,-1.8f, 2,-1,-2.8f, 2,-0.42f,-2.8f, 2,-0.42f,-1.8f,         M2,1); // right
    NQ(console, -2,-0.40f,-1.79f,2,-0.40f,-1.79f,2,-0.38f,-1.79f,-2,-0.38f,-1.79f,AB,1); // amber edge

    // ── INSTRUMENT PANEL ─────────────────────────────────────────────────
    auto& ipanel = cockpitRoot.addChild("instrument_panel");
    NQ(ipanel, -2,-0.42f,-2.79f,-0.65f,-0.42f,-2.79f,-0.65f,0.6f,-2.79f,-2,0.6f,-2.79f,     M2,1);
    NQ(ipanel,  0.65f,-0.42f,-2.79f, 0.85f,-0.42f,-2.79f, 0.85f,0.6f,-2.79f, 0.65f,0.6f,-2.79f, M2,1);
    NQ(ipanel, -0.65f,-0.42f,-2.79f, 0.65f,-0.42f,-2.79f, 0.65f,0.6f,-2.79f,-0.65f,0.6f,-2.79f, M1,1);
    NQ(ipanel, -2.01f,-0.43f,-2.79f,-1.99f,-0.43f,-2.79f,-1.99f,0.61f,-2.79f,-2.01f,0.61f,-2.79f,AM,1);
    NQ(ipanel,  1.99f,-0.43f,-2.79f, 2.01f,-0.43f,-2.79f, 2.01f,0.61f,-2.79f, 1.99f,0.61f,-2.79f,AM,1);

    // fuel gauge — bg + ticks are static, fill is a separate animated child
    auto& fuelGauge = ipanel.addChild("fuel_gauge");
    NQ(fuelGauge, -1.9f,0.2f,-2.78f,-1.4f,0.2f,-2.78f,-1.4f,0.8f,-2.78f,-1.9f,0.8f,-2.78f, AD,1);
    for (int t = 1; t <= 4; t++) {
        float ty = 0.22f + t * 0.11f;
        fuelGauge.addQuad({-1.9f,ty,-2.765f},{-1.4f,ty,-2.765f},
                          {-1.4f,ty+0.01f,-2.765f},{-1.9f,ty+0.01f,-2.765f},
                          AM[0],AM[1],AM[2],0.5f);
    }
    auto& fuelFill = fuelGauge.addChild("fuel_fill");
    fuelFill.onUpdate = [](SceneNode& self, const GameState& gs, float) {
        self.quads.clear();
        float h  = (gs.fuel / 100.0f) * 0.56f;
        float fr = gs.fuel > 30.0f ? 0.3f : 1.0f;
        float fg = gs.fuel > 30.0f ? 0.9f : 0.2f;
        self.addQuad({-1.85f,0.22f,-2.77f},{-1.45f,0.22f,-2.77f},
                     {-1.45f,0.22f+h,-2.77f},{-1.85f,0.22f+h,-2.77f},
                     fr, fg, 0.05f, 1.0f);
    };

    // speed gauge
    auto& spdGauge = ipanel.addChild("speed_gauge");
    NQ(spdGauge, -1.3f,0.2f,-2.78f,-0.8f,0.2f,-2.78f,-0.8f,0.8f,-2.78f,-1.3f,0.8f,-2.78f, AD,1);
    for (int t = 1; t <= 4; t++) {
        float ty = 0.22f + t * 0.11f;
        spdGauge.addQuad({-1.3f,ty,-2.765f},{-0.8f,ty,-2.765f},
                         {-0.8f,ty+0.01f,-2.765f},{-1.3f,ty+0.01f,-2.765f},
                         AM[0],AM[1],AM[2],0.5f);
    }
    auto& spdFill = spdGauge.addChild("speed_fill");
    spdFill.onUpdate = [](SceneNode& self, const GameState& gs, float) {
        self.quads.clear();
        float h = std::min(gs.speed / 5.0f, 1.0f) * 0.56f;
        self.addQuad({-1.25f,0.22f,-2.77f},{-0.85f,0.22f,-2.77f},
                     {-0.85f,0.22f+h,-2.77f},{-1.25f,0.22f+h,-2.77f},
                     0.3f,0.7f,1.0f,1.0f);
    };

    // buttons
    static const float BTN_POS[][2] = {
        {-1.9f,0.2f},{-1.65f,0.2f},{-1.4f,0.2f},
        {-1.9f,0.38f},{-1.65f,0.38f},{-1.4f,0.38f}
    };
    static const float BTN_COL[][3] = {
        {0.9f,0.15f,0.1f},{0.1f,0.8f,0.2f},{0.1f,0.4f,0.9f},
        {0.9f,0.75f,0.1f},{0.8f,0.1f,0.6f},{0.2f,0.7f,0.7f}
    };
    auto& buttons = ipanel.addChild("buttons");
    for (int i = 0; i < 6; i++) {
        float bx = BTN_POS[i][0], by = BTN_POS[i][1];
        buttons.addQuad({bx,by,-2.78f},{bx+0.18f,by,-2.78f},
                        {bx+0.18f,by+0.12f,-2.78f},{bx,by+0.12f,-2.78f},
                        BTN_COL[i][0],BTN_COL[i][1],BTN_COL[i][2],1.0f);
        buttons.addQuad({bx-0.01f,by-0.01f,-2.775f},{bx+0.19f,by-0.01f,-2.775f},
                        {bx+0.19f,by+0.13f,-2.775f},{bx-0.01f,by+0.13f,-2.775f},
                        AM[0],AM[1],AM[2],0.4f);
    }

    // ── PC SETUP ─────────────────────────────────────────────────────────
    auto& pc = cockpitRoot.addChild("pc_setup");

    auto& tower = pc.addChild("tower");
    addBox(tower, 1.75f,-1.0f,-1.7f, 2.15f,0.3f,-1.0f, M2);
    tower.addQuad({1.78f,-0.2f,-0.99f},{1.82f,-0.2f,-0.99f},   // power light
                  {1.82f,-0.1f,-0.99f},{1.78f,-0.1f,-0.99f}, AB[0],AB[1],AB[2],1);
    tower.addQuad({1.80f,0.1f,-0.99f},{2.10f,0.1f,-0.99f},     // disk slot 1
                  {2.10f,0.14f,-0.99f},{1.80f,0.14f,-0.99f}, M1[0],M1[1],M1[2],1);
    tower.addQuad({1.80f,0.0f,-0.99f},{2.10f,0.0f,-0.99f},     // disk slot 2
                  {2.10f,0.04f,-0.99f},{1.80f,0.04f,-0.99f}, M1[0],M1[1],M1[2],1);

    auto& monitor = pc.addChild("monitor");
    addBox(monitor, 1.22f,-0.42f,-2.2f, 1.33f,0.0f,-2.1f, M2); // stand
    monitor.addQuad({0.85f,0.0f,-2.49f},{1.75f,0.0f,-2.49f},    // bezel
                    {1.75f,0.85f,-2.49f},{0.85f,0.85f,-2.49f}, M1[0],M1[1],M1[2],1);
    monitor.addQuad({0.91f,0.05f,-2.48f},{1.69f,0.05f,-2.48f},  // screen
                    {1.69f,0.79f,-2.48f},{0.91f,0.79f,-2.48f}, GD[0],GD[1],GD[2],1);

    auto& screenLines = monitor.addChild("screen_lines");
    for (int i = 0; i < 5; i++) {
        float ly = 0.10f + i * 0.13f;
        float lw = (i % 2 == 0) ? 0.6f : 0.4f;
        screenLines.addQuad({0.93f,ly,-2.47f},{0.93f+lw,ly,-2.47f},
                            {0.93f+lw,ly+0.04f,-2.47f},{0.93f,ly+0.04f,-2.47f},
                            GP[0],GP[1],GP[2],0.4f);
    }

    auto& mtrim = monitor.addChild("monitor_trim");
    mtrim.addQuad({0.84f,-0.01f,-2.475f},{1.76f,-0.01f,-2.475f},{1.76f,0.02f,-2.475f},{0.84f,0.02f,-2.475f}, AM[0],AM[1],AM[2],1);
    mtrim.addQuad({0.84f,0.83f,-2.475f},{1.76f,0.83f,-2.475f},{1.76f,0.86f,-2.475f},{0.84f,0.86f,-2.475f}, AM[0],AM[1],AM[2],1);
    mtrim.addQuad({0.84f,-0.01f,-2.475f},{0.87f,-0.01f,-2.475f},{0.87f,0.86f,-2.475f},{0.84f,0.86f,-2.475f}, AM[0],AM[1],AM[2],1);
    mtrim.addQuad({1.73f,-0.01f,-2.475f},{1.76f,-0.01f,-2.475f},{1.76f,0.86f,-2.475f},{1.73f,0.86f,-2.475f}, AM[0],AM[1],AM[2],1);

    auto& keyboard = pc.addChild("keyboard");
    keyboard.addQuad({0.85f,-0.41f,-2.1f},{1.75f,-0.41f,-2.1f},
                     {1.75f,-0.41f,-1.82f},{0.85f,-0.41f,-1.82f}, M3[0],M3[1],M3[2],1);
    for (int row = 0; row < 3; row++) {
        float kz = -2.07f + row * 0.08f;
        keyboard.addQuad({0.88f,-0.40f,kz},{1.72f,-0.40f,kz},
                         {1.72f,-0.40f,kz+0.06f},{0.88f,-0.40f,kz+0.06f},
                         AD[0],AD[1],AD[2],0.8f);
    }

    initialized = true;
}

// ── per-frame draw ──────────────────────────────────────────────────────────
void drawCockpit(Renderer& r, GameState& gs, const glm::mat4& view, float dt) {
    if (!initialized) initCockpit();

    cockpitRoot.update(gs, dt);   // animations, gauge fills, chair rotation
    cockpitRoot.draw(r, view);    // pushes all geometry into the batch

    // proximity checks
    gs.nearChair = glm::length(gs.camPos - glm::vec3(0.0f, 0.0f, 0.6f))  < 1.5f;
    gs.nearPC    = glm::length(gs.camPos - glm::vec3(1.3f, 0.0f, -2.1f)) < 1.5f;
}