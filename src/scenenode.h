#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

struct Renderer;
struct GameState;

struct SceneNode {
    std::string name;

    //transform relative to parent
    glm::vec3 position = {0, 0, 0};
    glm::vec3 rotation = {0, 0, 0};
    glm::vec3 pivot = {0, 0, 0};

    bool visible = true;

    //geometry each quad stores its own color
    struct Quad {
        glm::vec3 a, b, c, d;
        glm::vec4 col;
    };
    std::vector<Quad> quads;

    //children
    std::vector <std::unique_ptr<SceneNode>> children;

    // assign a lambda here to animate this node each frame
    std::function<void(SceneNode&, const GameState&, float)> onUpdate;

    // add a child, returns reference to it
    SceneNode& addChild(const std::string& childName);

    // search subtree by name
    SceneNode* find(const std::string& n);

    // add a quad with RGBA color
    void addQuad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                 float r, float g, float bl, float alpha = 1.0f);

    void update(const GameState& gs, float dt);
    void draw(Renderer& r, const glm::mat4& view,
              const glm::mat4& parentWorld = glm::mat4(1.0f)) const;

private:
    glm::mat4 localTransform() const;
};