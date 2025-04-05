// 片段着色器（YUV420P转RGB）
#version 330 core
uniform sampler2D y_tex;
uniform sampler2D uv_tex;
in vec2 texcoord;
out vec4 color;

void main() {
    vec3 yuv;
    yuv.r=texture(y_tex,texcoord).r;
    yuv.gb=texture(uv_tex,texcoord).rg-0.5f;
    vec3 rgb=mat3(vec3(1,1,1),vec3(0,-0.395,2.032),vec3(1.140,-0.581,0))*yuv;
    color=vec4(rgb,1.0f);
}