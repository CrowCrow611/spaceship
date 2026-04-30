#pragma once
#include <string>
#include <glm/glm.hpp>

enum PlayerMode {
    WALKING, //free roam in cockpit
    PILOTING, //seated, wasd toc control ship
    TERMINAL,  //USING PC, FULLSCREEN MODE
};

// shared game state

struct GameState {
    //ship stst
    float fuel = 100.0f;
    float speed = 0.0f;
    float posX = 0.0f;
    float posY = 0.0;
    float shipYaw = -90.0f;
    std::string status = "OK";

    //player mode
    PlayerMode mode = PlayerMode::WALKING;

    //camera
    glm::vec3 camPos = glm::vec3(0.0f, 0.8f, 1.8f);
    glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 camUp  = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool firstMouse = true;

    //star scrool speed (0=parked, >0 = traveling)
    float starSpeed = 0.0f;

    //interaction flags set each frame by cockpit
    bool nearChair = false;
    bool nearPC = false;
    float chairAngle      = 0.0f;   // current angle in degrees
    float chairTargetAngle = 0.0f;  // target angle 180 or 0
};
