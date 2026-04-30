#include "entitymanager.h"
#include <glm/glm.hpp>
#include <algorithm>

void EntityManager::clear() {
    entities.clear();
}

void EntityManager::add(const Entity& e) {
    entities.push_back(e);
}

Entity* EntityManager::nearest(glm::vec3 pos, float range) {
    Entity* closest = nullptr;
    float minDist = range;
    for (auto& e : entities) {
        float d = glm::distance(pos, e.position);
        if (d < minDist) {
            minDist = d;
            closest = &e;
        }
    }
    return closest;
}

std::vector<Entity*> EntityManager::inRange(glm::vec3 pos, float range) {
    std::vector<Entity*> result;
    for (auto& e : entities) {
        if (glm::distance(pos, e.position) < range)
            result.push_back(&e);
    }
    return result;
}
