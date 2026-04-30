#pragma once
#include "entity.h"
#include <vector>

struct EntityManager {
    std::vector<Entity> entities;

    void clear();
    void add(const Entity& e);

    //find nearest entity to a position witihin rnage
    Entity* nearest(glm::vec3 pos, float range = 999.0f);

    //get all entities within range
    std::vector<Entity*> inRange(glm::vec3 pos, float range);
};