#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <map>
#include <vector>

//one loaded glyph from the font
struct Character {
    unsigned int textureID;
    glm::ivec2 size, bearing;
    unsigned int advance;

};


//central renderer
//owns all shaders vao, font, 
struct Renderer {
    int W, H; //screen in pixels

    //shader program
    unsigned int starShader, meshShader, panelShader, textShader;

    //VAO and VBO
    unsigned int starVAO, starVBO;
    unsigned int meshVAO, meshVBO;
    std::vector<float> meshBatch; // CPU-side batch buffer
    glm::mat4 lastView; // stored from quad3D calls
    unsigned int panelVAO, panelVBO;
    unsigned int textVAO, textVBO;

    // projection matrices
    glm::mat4 orthoProj; //2d hud, pixel coordi
    glm::mat4 persProj; //3d cockpit

    //loaded font glyphs
    std::map<char, Character> chars;

    //star position - updates every frame
    std::vector<float> stars;
    static const int NUM_STARS = 3000;

    // setup
    void init(int w, int h);
    void loadFont(const char* path, int pixelSize);

    // 2d helpers (pixel coordinates, y=o is bottom)
    void rect(float x, float y, float w, float h,
              float cr, float cg, float cb, float ca);
    void text(const std::string& s, float x, float y, float scale,
              float cr, float cg, float cb);

    // 3d helper
    //draws 2 triangle forming a quad frpm 4 corners
    void quad3D(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
            float cr, float cg, float cb, float ca,
            const glm::mat4& view,
            glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f));

    //starfield
    void initStars();
    void updateStars(float speed);
    void drawStars(const glm::mat4& view);
    void resize(int w, int h);
    void flushMesh();
    void reloadShaders(); //use f5 to reload
    void updateFrustum(const glm::mat4& viewProj);
    bool inFrustum(glm::vec3 point, float radius = 0.5f) const;
    void setFOV(float fov);


private:
    glm::vec4 frustumPlanes[6];
    unsigned int compile(const char* vertSrc, const char* fragSrc);
    unsigned int compileFromFiles(const char* vertPath, const char* fragPath);
    void uploadProjections();
};