#pragma once
#include "renderer.h"
#include "gamestate.h"
#include "entitymanager.h"

void initPlanets(EntityManager& em);
void drawPlanets(Renderer& r, EntityManager& em, GameState& gs, const glm::mat4& view);