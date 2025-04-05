#version 330 core
in vec2 texcoord;
uniform sampler2D y_tex;
uniform sampler2D u_tex;
uniform sampler2D v_tex;
out vec4 FragColor;
void main()
{
    vec3 yuv;
    yuv.r=texture(y_tex,texcoord).r;
    yuv.g=texture(u_tex,texcoord).r-0.5f;
    yuv.b=texture(v_tex,texcoord).r-0.5f;
    vec3 rgb=mat3(vec3(1,1,1),vec3(0,-0.395,2.032),vec3(1.140,-0.581,0))*yuv;
    FragColor=vec4(rgb,1.0f);
}