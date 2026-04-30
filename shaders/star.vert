#version 460 core
layout(location=0) in vec3 pos;
uniform mat4 view;
uniform mat4 projection;
void main() {
    // strip translation from view so stars act like a skybox
    mat4 rotView = mat4(mat3(view));
    gl_Position = (projection * rotView * vec4(pos, 1.0)).xyww;
    gl_PointSize = 2.0;
}