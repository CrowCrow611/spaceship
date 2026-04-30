#version 460 core
in vec2 tc;
out vec4 fc;
uniform sampler2D tex;
uniform vec3 color;
void main() {
    fc = vec4(color, texture(tex, tc).r);
}