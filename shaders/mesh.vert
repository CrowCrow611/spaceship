#version 460 core
layout(location=0) in vec3 pos;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;

out vec4 vColor;

uniform mat4 view, projection;

void main() {
    gl_Position = projection * view * vec4(pos, 1.0);
    vColor = color;
}