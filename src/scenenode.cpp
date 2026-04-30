#include "scenenode.h"
#include "renderer.h"
#include <glm/gtc/matrix_transform.hpp>

SceneNode& SceneNode::addChild(const std::string& n) {
    children.push_back(std::make_unique<SceneNode>());
    children.back()->name = n;
    return *children.back();
}

SceneNode* SceneNode::find(const std::string& n) {
    if (name == n) return this;
    for (auto& c : children)
        if (auto* f = c->find(n)) return f;
    return nullptr;
}

void SceneNode::addQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                        float r, float g, float bl, float alpha) {
    quads.push_back({a, b, c, d, {r, g, bl, alpha}});
}

glm::mat4 SceneNode::localTransform() const {
    glm::mat4 t(1.0f);
    t = glm::translate(t, position + pivot);
    t = glm::rotate(t, glm::radians(rotation.y), {0,1,0});
    t = glm::rotate(t, glm::radians(rotation.x), {1,0,0});
    t = glm::rotate(t, glm::radians(rotation.z), {0,0,1});
    t = glm::translate(t, -pivot);
    return t;
}

void SceneNode::update(const GameState& gs, float dt) {
    if (onUpdate) onUpdate(*this, gs, dt);
    for (auto& c : children) c->update(gs, dt);
}

void SceneNode::draw(Renderer& renderer, const glm::mat4& view,
                     const glm::mat4& parentWorld) const {
    if (!visible) return;
    glm::mat4 world = parentWorld * localTransform();

    for (auto& q : quads) {
        auto xf = [&](glm::vec3 p) { return glm::vec3(world * glm::vec4(p, 1.0f)); };
        glm::vec3 wa = xf(q.a), wb = xf(q.b), wc = xf(q.c), wd = xf(q.d);
        glm::vec3 normal = glm::normalize(glm::cross(wb - wa, wd - wa));
        renderer.quad3D(wa, wb, wc, wd,
                        q.col.r, q.col.g, q.col.b, q.col.a,
                        view, normal);
    }
    for (auto& c : children) c->draw(renderer, view, world);
}