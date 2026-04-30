#include "config.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>

#include "gamestate.h"
#include "renderer.h"
#include "cockpit.h"
#include "hud.h"
#include "terminal.h"
#include "savegame.h"
#include "planets.h"
EntityManager em;

Renderer* gRenderer = nullptr;


int W, H;

GameState gs;
Terminal  term;

//mouse
void onMouse(GLFWwindow*, double xpos, double ypos) {
    if (gs.mode == PlayerMode::TERMINAL) return;
    if (gs.firstMouse) { gs.lastX = xpos; gs.lastY = ypos; gs.firstMouse = false; }
    float sens = cfg.getFloat("mouse_sensitivity", 0.1f);
    float xoff = (xpos - gs.lastX) * sens;
    float yoff = (gs.lastY - ypos) * sens;
    gs.lastX = xpos; gs.lastY = ypos;
    gs.yaw  += xoff;
    gs.pitch = glm::clamp(gs.pitch + yoff, -89.0f, 89.0f);
    glm::vec3 front;
    front.x = cos(glm::radians(gs.yaw)) * cos(glm::radians(gs.pitch));
    front.y = sin(glm::radians(gs.pitch));
    front.z = sin(glm::radians(gs.yaw)) * cos(glm::radians(gs.pitch));
    gs.camFront = glm::normalize(front);
}

//key 
void onKey(GLFWwindow* win, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    //f5: hot reload shaders
    if (key == GLFW_KEY_F5) {
        if (gRenderer) gRenderer->reloadShaders();
        return;
    }

    if (key == GLFW_KEY_F6) { saveGame(gs); return; }
    if (key == GLFW_KEY_F7) { loadGame(gs); return; }

    //esc: exit terminal or close window
    if (key == GLFW_KEY_ESCAPE) {
        if (gs.mode == PlayerMode::TERMINAL) {
            gs.mode = PlayerMode::WALKING;
            term.active = false;
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetWindowShouldClose(win, true);
        }
        return;
    }

    //terminal gets key input whiel active
    if (gs.mode == PlayerMode::TERMINAL) {
        term.handleKey(key);
        if (!term.pendingCmd.empty()) {
            term.execute(term.pendingCmd, gs);
            term.pendingCmd = "";
        }
        return;
    }


    // E interact wiht nearby object
    if (key == GLFW_KEY_E) {
        if (gs.mode == PlayerMode::WALKING) {
            if (gs.nearPC) {
                // Open terminal fullscreen
                gs.mode = PlayerMode::TERMINAL;
                term.active = true;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else if (gs.nearChair) {
                // Sit in pilot seat
                gs.mode     = PlayerMode::PILOTING;
                gs.camPos = glm::vec3(0.0f, 0.4f, 0.3f);  // sitting IN the seat
                gs.camFront = glm::vec3(0.0f, -0.05f,-1.0f);
                gs.yaw      = -90.0f;
                gs.pitch  = -5.0f;  // look slightly down over the console
                gs.firstMouse = true;
            }
        } else if (gs.mode == PlayerMode::PILOTING) {
            // Stand up
            gs.mode      = PlayerMode::WALKING;
        }
    }
}

//char: printable characters go to terminal
void onChar(GLFWwindow*, unsigned int c) {
    term.handleChar(c);
}

// Ship physics makes the ship run even though the player is walking
void updateShip(float dt) {

    // Smooth chair rotation
    float diff = gs.chairTargetAngle - gs.chairAngle;
    if (diff > 180.0f)  diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;

    float step = diff * 8.0f * dt;
    gs.chairAngle += step;

    // Camera rotates by the SAME amount the chair moved this frame
    if (gs.mode == PlayerMode::PILOTING && glm::abs(step) > 0.001f) {
        gs.yaw += step;
        glm::vec3 front;
        front.x = cos(glm::radians(gs.yaw)) * cos(glm::radians(gs.pitch));
        front.y = sin(glm::radians(gs.pitch));
        front.z = sin(glm::radians(gs.yaw)) * cos(glm::radians(gs.pitch));
        gs.camFront = glm::normalize(front);
    }


    if (gs.speed <= 0.0f) return;
    gs.posX += gs.speed * dt * 0.1f * cos(glm::radians(gs.shipYaw + 90.0f));
    gs.posY += gs.speed * dt * 0.1f * sin(glm::radians(gs.shipYaw + 90.0f));
    gs.starSpeed = gs.speed * 0.0003f;
    gs.fuel -= gs.speed * dt * cfg.getFloat("fuel_drain_rate", 0.3f);
    if (gs.fuel <= 0.0f) {
        gs.fuel      = 0.0f;
        gs.speed     = 0.0f;
        gs.starSpeed = 0.0f;
        gs.status    = "NO FUEL";
    }
}

//WASD movement
void processMovement(GLFWwindow* win, float dt) {
    if (gs.mode == PlayerMode::TERMINAL) return;

    float mv = cfg.getFloat("walk_speed", 2.0f) * dt;

    if (gs.mode == PlayerMode::WALKING) {
        glm::vec3 right     = glm::normalize(glm::cross(gs.camFront, gs.camUp));
        glm::vec3 flatFront = glm::normalize(glm::vec3(gs.camFront.x, 0, gs.camFront.z));
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) gs.camPos += flatFront * mv;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) gs.camPos -= flatFront * mv;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) gs.camPos -= right * mv;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) gs.camPos += right * mv;
        gs.camPos.x = glm::clamp(gs.camPos.x, -2.5f, 2.5f);
        gs.camPos.z = glm::clamp(gs.camPos.z, -2.6f, 2.5f);
        gs.camPos.y = 0.8f;
    }
    else if (gs.mode == PlayerMode::PILOTING) {
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
            gs.speed = glm::min(gs.speed + dt, cfg.getFloat("max_speed", 5.0f));
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
            gs.speed = glm::max(gs.speed - dt, 0.0f);
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) gs.shipYaw -= 45.0f * dt;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) gs.shipYaw += 45.0f * dt;
        // R/F: rotate chair left/right continuously
    if (glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS) gs.chairTargetAngle += 90.0f * dt;
    if (glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS) gs.chairTargetAngle -= 90.0f * dt;
    }
}

//main
int main() {
    cfg.load("spaceship.cfg");
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    W = mode->width;
    H = mode->height;
    
    GLFWwindow* win;
    if (cfg.getBool("fullscreen", false)) {
        W = mode->width; H = mode->height;
        win = glfwCreateWindow(W, H, "Spaceship", monitor, NULL);
    } else {
        W = 1280; H = 720;
        win = glfwCreateWindow(W, H, "Spaceship", NULL, NULL);
    }

    if (!win) {
    std::cerr << "Failed to create window\n";
    glfwTerminate();
    return -1;
    }
    glfwMakeContextCurrent(win);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }
    

    int fbW, fbH;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    W = fbW;
    H = fbH;
    glViewport(0, 0, W, H);
    glfwSetFramebufferSizeCallback(win, [](GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
    if (gRenderer) gRenderer->resize(w, h);
    });


    glfwSetCursorPosCallback(win, onMouse);
    glfwSetKeyCallback(win, onKey);
    glfwSetCharCallback(win, onChar);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Renderer r;
    gRenderer = &r;
    r.init(W, H);
    r.loadFont(cfg.getString("font_path", "include/fonts/VT323.ttf").c_str(), cfg.getInt("font_size", 24));
    term.init(&gs);
    initPlanets(em);
    initCockpit();

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(win)) {
        float now = glfwGetTime();
        float dt  = now - lastFrame;
        lastFrame = now;
        if (dt > 0.1f) dt = 0.1f; 

        processMovement(win, dt);
        updateShip(dt); 
        // run CPU cycles each frame (only if a program is loaded)
        if (term.cpuLoaded && 
            term.cpu.status == LS32::CPUStatus::RUNNING &&
            gs.mode != PlayerMode::TERMINAL) {
            term.cpu.run(10000);
        }
        r.updateStars(gs.starSpeed);

        float baseFov = cfg.getFloat("fov", 70.0f);
        if (cfg.getBool("fov_zoom", true))
            r.setFOV(glm::clamp(baseFov + gs.speed * 3.0f, baseFov, 100.0f));
        else
            r.setFOV(baseFov);

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //1. stars
        glm::vec3 shakePos = gs.camPos;
        if (cfg.getBool("camera_shake", true) && gs.speed > 0.5f) {
            float intensity = gs.speed * 0.0015f;
            shakePos.x += ((rand() % 100) / 100.0f - 0.5f) * intensity;
            shakePos.y += ((rand() % 100) / 100.0f - 0.5f) * intensity;
        }
        glm::mat4 view = glm::lookAt(shakePos, shakePos + gs.camFront, gs.camUp);
        r.updateFrustum(r.persProj * view);
        r.drawStars(view);

        //2. 3D cockpit
        drawCockpit(r, gs, view, dt);
        r.flushMesh();

        //3. 2D hud overlay
        glDisable(GL_DEPTH_TEST);
        drawHUD(r, gs, dt);

        //4. Terminal (fullscreen, only when active)
        if (gs.mode == PlayerMode::TERMINAL)
            term.draw(r, dt);

        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}