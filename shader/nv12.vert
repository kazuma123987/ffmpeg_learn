// 顶点着色器
#version 330 core
layout(location=0) in vec4 pos;
out vec2 texcoord;
void main() {
    gl_Position = vec4(pos.xy, 0.0, 1.0);
    texcoord = pos.zw;
}