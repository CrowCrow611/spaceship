#include "config.h"
#include "renderer.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <fstream>
#include <sstream>

static std::mt19937 rng(std::random_device{}());
static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

//read a file into string
static std::string readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Shader file not found: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

//shader source setting

//stasr: 2d points, always in backgorund
static const char* STAR_VERT = R"(
#version 460 core
layout(location=0) in vec2 pos;
void main() {
    gl_Position = vec4(pos, 0.999, 1.0); //0.999 depth = alwasy behind 3d
    gl_PointSize = 2.0; //star dot size
}
)";
static const char* STAR_FRAG = R"(
#version 460 core
out vec4 fc;
void main() { fc = vec4(1.0, 1.0, 1.0, 1.0); }
)";

//3d cockpit mesh
static const char* MESH_VERT = R"(
#version 460 core
layout(location=0) in vec3 pos;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;
out vec4 vColor;
uniform mat4 view, projection;
void main() {
    gl_Position = projection * view * vec4(pos, 1.0);
    vColor = color;}
)";

static const char* MESH_FRAG = R"(
#version 460 core
in vec4 vColor;
out vec4 fc;
void main() { fc = vColor; }
)";

// 2D HUD rectangles
static const char* PANEL_VERT = R"(
#version 460 core
layout(location=0) in vec2 pos;
uniform mat4 projection;
void main() { gl_Position = projection * vec4(pos, 0.0, 1.0); }
)";
static const char* PANEL_FRAG = R"(
#version 460 core
out vec4 fc;
uniform vec4 color;
void main() { fc = color; }
)";

//text rendering (grayscale texture alphs)
static const char* TEXT_VERT = R"(
#version 460 core
layout(location = 0) in vec4 vertex; //xy=screen pos, zw=texcoord
out vec2 tc;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    tc = vertex.zw;
}
)";
static const char* TEXT_FRAG = R"(
#version 460 core
in vec2 tc;
out vec4 fc;
uniform sampler2D tex;
uniform vec3 color;
void main() {
    fc = vec4(color, texture(tex, tc).r);
}
)";

//shader compilation helper
unsigned int Renderer::compile(const char* vertSrc, const char* fragSrc) {
    int ok; char log[512];
    auto check = [&](unsigned int id) {
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(id, 512, NULL, log); std::cerr << log << "\n"; }
    };
    unsigned int v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vertSrc, NULL); glCompileShader(v); check(v);
    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fragSrc, NULL); glCompileShader(f); check(f);
    unsigned int p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(p, 512, NULL, log); std::cerr << "Link error: " << log << "\n"; }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

//Init
unsigned int Renderer::compileFromFiles(const char* vertPath, const char* fragPath) {
    std::string vs = readFile(vertPath);
    std::string fs = readFile(fragPath);
    if (vs.empty() || fs.empty()) return 0;
    const char* vsc = vs.c_str();
    const char* fsc = fs.c_str();
    return compile(vsc, fsc);
}
void Renderer::uploadProjections() {
    glUseProgram(panelShader);
    glUniformMatrix4fv(glGetUniformLocation(panelShader,"projection"),1,GL_FALSE,glm::value_ptr(orthoProj));
    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader,"projection"),1,GL_FALSE,glm::value_ptr(orthoProj));
    glUseProgram(meshShader);
    glUniformMatrix4fv(glGetUniformLocation(meshShader,"projection"),1,GL_FALSE,glm::value_ptr(persProj));
    glm::mat4 identity(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(meshShader,"model"),1,GL_FALSE,glm::value_ptr(identity));
    // Light uniforms — warm amber ceiling light
    glUniform3f(glGetUniformLocation(meshShader,"lightPos"),   0.0f, 1.8f, -1.0f);
    glUniform3f(glGetUniformLocation(meshShader,"lightColor"), 1.0f, 0.85f, 0.6f);
    glUniform1f(glGetUniformLocation(meshShader,"ambient"),    0.35f);
}

void Renderer::reloadShaders() {
    std::cout << "Reloading shaders...\n";
    unsigned int s, m, p, t;
    s = compileFromFiles("shaders/star.vert",  "shaders/star.frag");
    m = compileFromFiles("shaders/mesh.vert",  "shaders/mesh.frag");
    p = compileFromFiles("shaders/panel.vert", "shaders/panel.frag");
    t = compileFromFiles("shaders/text.vert",  "shaders/text.frag");
    // Only swap if all compiled successfully
    if (s && m && p && t) {
        glDeleteProgram(starShader);
        glDeleteProgram(meshShader);
        glDeleteProgram(panelShader);
        glDeleteProgram(textShader);
        starShader  = s;
        meshShader  = m;
        panelShader = p;
        textShader  = t;
        uploadProjections();
        std::cout << "Shaders reloaded OK\n";
    } else {
        std::cout << "Shader reload failed — keeping old shaders\n";
    }
}

void Renderer::init(int w, int h) {
    W = w; H = h;

    //compile all shader
    starShader  = compileFromFiles("shaders/star.vert",  "shaders/star.frag");
    meshShader  = compileFromFiles("shaders/mesh.vert",  "shaders/mesh.frag");
    panelShader = compileFromFiles("shaders/panel.vert", "shaders/panel.frag");
    textShader  = compileFromFiles("shaders/text.vert",  "shaders/text.frag");

    //2D ortho projection: maps pixel coord to Opengl clip space
    orthoProj = glm::ortho(0.0f, (float)W, 0.0f, (float)H);

    //3d perspective projection
    float fov = cfg.getFloat("fov", 70.0f);
    persProj = glm::perspective(glm::radians(fov), (float)W/H, 0.01f, 100.0f);

    //upload projection
    uploadProjections();

    // vao: stars (2d points)
    glGenVertexArrays(1,&starVAO); glGenBuffers(1,&starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, NUM_STARS*6*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);

    glEnableVertexAttribArray(0);

    // vao: mesh(3d quads, 6 verts of vec3)
    // vao: mesh (3d quads, 6 verts of vec3 pos + vec3 normal)
    // vao: mesh — interleaved: pos(3) + normal(3) + color(4) = 10 floats/vert
    glGenVertexArrays(1, &meshVAO);
    glGenBuffers(1, &meshVBO);
    glBindVertexArray(meshVAO);
    glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
    glBufferData(GL_ARRAY_BUFFER, 100000 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    // pos at location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal at location 1
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(3*sizeof(float)));
    // color at location 2
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 10*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    // vao: panel (2d rects, 6 verts of vec2)
    glGenVertexArrays(1,&panelVAO); glGenBuffers(1,&panelVBO);
    glBindVertexArray(panelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, panelVBO);
    glBufferData(GL_ARRAY_BUFFER, 6*2*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    //vao: text(6 verts of vec4: xy pos + zw texcoord)
    glGenVertexArrays(1,&textVAO); glGenBuffers(1,&textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, 6*4*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    initStars();
}

//font loading via freestyle

void Renderer::loadFont(const char* path, int px) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) { std::cerr << "FreeType init failed\n"; return; }
    FT_Face face;
    if (FT_New_Face(ft, path, 0, &face)) { std::cerr << "Font load failed: " << path << "\n"; return; }
    FT_Set_Pixel_Sizes(face, 0, px);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (unsigned char c = 0; c < 128; c++) {
        FT_Load_Char(face, c, FT_LOAD_RENDER);
        unsigned int tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     face->glyph->bitmap.width, face->glyph->bitmap.rows,
                     0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        chars[c] = { tex,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left,  face->glyph->bitmap_top),
            (unsigned int)face->glyph->advance.x };
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

//2d filled rectangle
void Renderer::rect(float x, float y, float w, float h,
                    float cr, float cg, float cb, float ca) {
    float v[] = { x,y, x+w,y, x,y+h, x+w,y, x+w,y+h, x,y+h };
    glUseProgram(panelShader);
    glUniform4f(glGetUniformLocation(panelShader,"color"), cr,cg,cb,ca);
    glBindVertexArray(panelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, panelVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

//2d text
void Renderer::text(const std::string& s, float x, float y, float scale,
                    float cr, float cg, float cb) {
    glUseProgram(textShader);
    glUniform3f(glGetUniformLocation(textShader,"color"), cr,cg,cb);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);
    for (char c : s) {
        if (chars.find(c) == chars.end()) continue;
        auto& ch = chars[c];
        float xp = x + ch.bearing.x * scale;
        float yp = y - (ch.size.y - ch.bearing.y) * scale;
        float cw = ch.size.x * scale;
        float ch_h = ch.size.y * scale;
        float v[6][4] = {
            {xp,    yp+ch_h, 0,0}, {xp,    yp,     0,1}, {xp+cw, yp,     1,1},
            {xp,    yp+ch_h, 0,0}, {xp+cw, yp,     1,1}, {xp+cw, yp+ch_h,1,0}
        };
        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.advance >> 6) * scale;
    }
}

//extract fustum planes from current view-projection matrix
void Renderer::updateFrustum(const glm::mat4& vp) {
    // Left
    frustumPlanes[0] = glm::vec4(vp[0][3]+vp[0][0], vp[1][3]+vp[1][0], vp[2][3]+vp[2][0], vp[3][3]+vp[3][0]);
    // Right
    frustumPlanes[1] = glm::vec4(vp[0][3]-vp[0][0], vp[1][3]-vp[1][0], vp[2][3]-vp[2][0], vp[3][3]-vp[3][0]);
    // Bottom
    frustumPlanes[2] = glm::vec4(vp[0][3]+vp[0][1], vp[1][3]+vp[1][1], vp[2][3]+vp[2][1], vp[3][3]+vp[3][1]);
    // Top
    frustumPlanes[3] = glm::vec4(vp[0][3]-vp[0][1], vp[1][3]-vp[1][1], vp[2][3]-vp[2][1], vp[3][3]-vp[3][1]);
    // Near
    frustumPlanes[4] = glm::vec4(vp[0][3]+vp[0][2], vp[1][3]+vp[1][2], vp[2][3]+vp[2][2], vp[3][3]+vp[3][2]);
    // Far
    frustumPlanes[5] = glm::vec4(vp[0][3]-vp[0][2], vp[1][3]-vp[1][2], vp[2][3]-vp[2][2], vp[3][3]-vp[3][2]);
    // Normalize
    for (auto& p : frustumPlanes) {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }
}

bool Renderer::inFrustum(glm::vec3 point, float radius) const {
    for (const auto& plane : frustumPlanes) {
        if (glm::dot(glm::vec3(plane), point) + plane.w + radius < 0)
            return false;
    }
    return true;
}

//3d quad (two trinagle from 4 corners)
void Renderer::quad3D(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                      float cr, float cg, float cb, float ca,
                      const glm::mat4& view, glm::vec3 normal) {
    glm::vec3 center = (a + b + c + d) * 0.25f;
    float radius = glm::max(glm::max(glm::distance(center,a), glm::distance(center,b)),
                            glm::max(glm::distance(center,c), glm::distance(center,d)));
    if (!inFrustum(center, radius)) return;
    lastView = view;
    auto push = [&](glm::vec3 p) {
        meshBatch.insert(meshBatch.end(), {
            p.x, p.y, p.z,
            normal.x, normal.y, normal.z,
            cr, cg, cb, ca
        });
    };
    // 2 triangles = 6 verts
    push(a); push(b); push(c);
    push(a); push(c); push(d);
}

void Renderer::flushMesh() {
    if (meshBatch.empty()) return;
    glUseProgram(meshShader);
    glUniformMatrix4fv(glGetUniformLocation(meshShader,"view"), 1, GL_FALSE,
                       glm::value_ptr(lastView));
    glBindVertexArray(meshVAO);
    glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 meshBatch.size() * sizeof(float),
                 meshBatch.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(meshBatch.size() / 10));
    meshBatch.clear();
}

//starfield
void Renderer::initStars() {
    stars.clear();
    int count = cfg.getInt("star_count", NUM_STARS);
    for (int i = 0; i < count; i++) {
        float theta = dist(rng) * 3.14159f;
        float phi   = dist(rng) * 3.14159f;
        float r     = 90.0f;
        float x = r * sin(theta) * cos(phi);
        float y = r * sin(theta) * sin(phi);
        float z = r * cos(theta);
        stars.push_back(x); stars.push_back(y); stars.push_back(z); // point
        stars.push_back(x); stars.push_back(y); stars.push_back(z); // tail (same at rest)
    }
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, stars.size()*sizeof(float), stars.data());
}

void Renderer::updateStars(float spd) {
    float tailLen = spd * 800.0f; // stretch with speed
    for (int i = 0; i < (int)stars.size(); i += 6) {
        stars[i+2] += spd * 2.0f;
        if (stars[i+2] > 90.0f) {
            stars[i]   = dist(rng) * 90.0f;
            stars[i+1] = dist(rng) * 90.0f;
            stars[i+2] = -90.0f;
        }
        // tail is behind the star along Z
        stars[i+3] = stars[i];
        stars[i+4] = stars[i+1];
        stars[i+5] = stars[i+2] + tailLen;
    }
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, stars.size()*sizeof(float), stars.data());
}

void Renderer::drawStars(const glm::mat4& view) {
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL); 
    glUseProgram(starShader);
    glUniformMatrix4fv(glGetUniformLocation(starShader,"view"),       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(starShader,"projection"), 1, GL_FALSE, glm::value_ptr(persProj));
    glBindVertexArray(starVAO);
    glDrawArrays(GL_POINTS, 0, (GLsizei)(stars.size()/3));
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

void Renderer::resize(int w, int h) {
    W = w; H = h;
    orthoProj = glm::ortho(0.0f, (float)W, 0.0f, (float)H);
    glUseProgram(panelShader);
    glUniformMatrix4fv(glGetUniformLocation(panelShader,"projection"),1,GL_FALSE,glm::value_ptr(orthoProj));
    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader,"projection"),1,GL_FALSE,glm::value_ptr(orthoProj));
}

void Renderer::setFOV(float fov) {
    persProj = glm::perspective(glm::radians(fov), (float)W/H, 0.01f, 100.0f);
    glUseProgram(meshShader);
    glUniformMatrix4fv(glGetUniformLocation(meshShader,"projection"),1,GL_FALSE,glm::value_ptr(persProj));
}