#pragma once
#include "renderer.h"
#include "gamestate.h"
#include "scenenode.h"
#include <glm/glm.hpp>


void initCockpit();
void drawCockpit(Renderer& r, GameState& gs, const glm::mat4& view, float dt);