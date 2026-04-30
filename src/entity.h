#pragma once
#include <string>
#include <glm/glm.hpp>
enum class EntityType {
    PLANET,
    DERELICT,
    ANOMALY,
    SIGNAL
};
struct Entity {
    EntityType type;
    std::string name;
    glm::vec3 position; //poition in soace
    float size; //radius
    glm::vec3 color; //rgb
    std::string description;
    bool visited = false;
};