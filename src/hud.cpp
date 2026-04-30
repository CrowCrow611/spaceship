#include "config.h"
#include "hud.h"
#include <string>
#include <cmath>

void drawHUD(Renderer& r, GameState& gs, float dt) {
    float W = (float)r.W;
    float H = (float)r.H;
    float bh = 72.0f;

    // bottom bar
    r.rect(0, 0, W, bh, 0.04f, 0.05f, 0.07f, 0.88f);
    r.rect(0, bh-2, W, 2, 0.8f, 0.55f, 0.08f, 1.0f);

    // fuel
    float fr = gs.fuel > 30.0f ? 0.3f : 1.0f;
    float fg = gs.fuel > 30.0f ? 0.9f : 0.2f;
    r.text("FUEL", 20, bh-25, 1.0f, fr, fg, 0.1f);
    r.rect(64, bh-22, 130, 12, 0.15f, 0.15f, 0.15f, 1.0f);
    r.rect(64, bh-22, gs.fuel * 1.3f, 12, fr, fg, 0.1f, 1.0f);
    r.text(std::to_string((int)gs.fuel)+"%", 200, bh-25, 1.0f, fr, fg, 0.1f);

    // speed
    r.text("SPD:" + std::to_string(gs.speed).substr(0,4), 260, bh-25, 1.0f, 0.3f, 0.8f, 1.0f);

    // status
    float sc  = (gs.status == "OK") ? 0.3f : 1.0f;
    float sc2 = (gs.status == "OK") ? 0.9f : 0.2f;
    r.text("STS:" + gs.status, 420, bh-25, 1.0f, sc, sc2, 0.3f);

    // coordinates
    std::string coords = "X:" + std::to_string(gs.posX).substr(0,5)
                       + " Y:" + std::to_string(gs.posY).substr(0,5);
    r.text(coords, 580, bh-25, 1.0f, 0.6f, 0.7f, 0.5f);

    // mode indicator
    std::string modeStr;
    if      (gs.mode == PlayerMode::PILOTING) modeStr = "[ PILOTING ]";
    else if (gs.mode == PlayerMode::TERMINAL) modeStr = "[ TERMINAL ]";
    else                                       modeStr = "[ WALKING  ]";
    r.text(modeStr, W-200, bh-25, 1.0f, 0.7f, 0.55f, 0.2f);

    // interaction hints
    float hintX = W/2 - 100;
    float hintY = H/2 + 50;
    if (gs.nearChair && gs.mode == PlayerMode::WALKING)
        r.text("[ E ]  SIT DOWN", hintX, hintY, 1.1f, 0.9f, 0.7f, 0.2f);
    if (gs.nearPC && gs.mode == PlayerMode::WALKING)
        r.text("[ E ]  USE TERMINAL", hintX-30, hintY, 1.1f, 0.2f, 0.95f, 0.3f);
    if (gs.mode == PlayerMode::PILOTING)
        r.text("[ E ]  STAND  |  WASD: STEER", hintX-60, hintY, 1.0f, 0.7f, 0.6f, 0.2f);

    // crosshair
    if (gs.mode != PlayerMode::TERMINAL) {
        float cx = W/2, cy = H/2;
        r.rect(cx-10, cy-1, 20, 2, 0.6f, 0.9f, 0.3f, 0.65f);
        r.rect(cx-1, cy-10, 2, 20, 0.6f, 0.9f, 0.3f, 0.65f);
    }

    // FPS counter
    if (cfg.getBool("show_fps", true) && dt > 0.0f) {
        static float fpsTimer   = 0.0f;
        static int   fpsCount   = 0;
        static int   fpsDisplay = 0;
        fpsTimer += dt;
        fpsCount++;
        if (fpsTimer >= 1.0f) {
            fpsDisplay = fpsCount;
            fpsCount   = 0;
            fpsTimer   = 0.0f;
        }
        float fc  = fpsDisplay >= 55 ? 0.3f : (fpsDisplay >= 30 ? 0.9f : 1.0f);
        float fc2 = fpsDisplay >= 55 ? 0.9f : (fpsDisplay >= 30 ? 0.7f : 0.2f);
        r.text("FPS:" + std::to_string(fpsDisplay), W-100, bh+20, 1.0f, fc, fc2, 0.2f);
    }
}



