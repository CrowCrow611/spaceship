#include "planets.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

void initPlanets(EntityManager& em) {
    em.clear();
    // spawn a few planets at fixed world positions
    Entity p1;
    p1.type = EntityType::PLANET;
    p1.name = "Kerath";
    p1.position = glm::vec3(500.0f, 0.0f, 800.0f);
    p1.size = 80.0f;
    p1.color = glm::vec3(0.2f, 0.5f, 0.9f); // blue
    em.add(p1);

    Entity p2;
    p2.type = EntityType::PLANET;
    p2.name = "Durn";
    p2.position = glm::vec3(-1200.0f, 100.0f, 2000.0f);
    p2.size = 120.0f;
    p2.color = glm::vec3(0.8f, 0.4f, 0.1f); // orange
    em.add(p2);
}

static void drawSphere(Renderer& r, glm::vec3 center, float radius,
                       glm::vec3 col, const glm::mat4& view) {
    const int LAT = 8, LON = 12;
    for (int i = 0; i < LAT; i++) {
        float lat0 = glm::pi<float>() * (-0.5f + (float)i / LAT);
        float lat1 = glm::pi<float>() * (-0.5f + (float)(i+1) / LAT);
        for (int j = 0; j < LON; j++) {
            float lon0 = 2 * glm::pi<float>() * (float)j / LON;
            float lon1 = 2 * glm::pi<float>() * (float)(j+1) / LON;
            glm::vec3 a = center + radius * glm::vec3(cos(lat0)*cos(lon0), sin(lat0), cos(lat0)*sin(lon0));
            glm::vec3 b = center + radius * glm::vec3(cos(lat0)*cos(lon1), sin(lat0), cos(lat0)*sin(lon1));
            glm::vec3 c = center + radius * glm::vec3(cos(lat1)*cos(lon1), sin(lat1), cos(lat1)*sin(lon1));
            glm::vec3 d = center + radius * glm::vec3(cos(lat1)*cos(lon0), sin(lat1), cos(lat1)*sin(lon0));
            // slight shading by latitude
            float shade = 0.6f + 0.4f * ((float)i / LAT);
            r.quad3D(a, b, c, d, col.r*shade, col.g*shade, col.b*shade, 1.0f, view);
        }
    }
}

void drawPlanets(Renderer& r, EntityManager& em, GameState& gs, const glm::mat4& view) {
    for (auto* e : em.inRange(glm::vec3(0), 99999.0f)) {
        if (e->type != EntityType::PLANET) continue;
        // offset planet position by ship world position
        glm::vec3 relPos = glm::vec3(
            e->position.x - gs.posX,
            e->position.y - gs.posY,
            e->position.z
        );
        drawSphere(r, relPos, e->size, e->color, view);
    }
}