#version 400 core
#define AV_PIX_FMT_YUV420P 0
#define AV_PIX_FMT_NV12 23
in vec2 texCoord;
uniform sampler2D image_Y;
uniform sampler2D image_U;
uniform sampler2D image_V;
uniform sampler2D image_UV;
uniform int format;
out vec4 FragColor;
void main()
{
    vec3 yuv;
    if(format==AV_PIX_FMT_YUV420P)
    {
        yuv.r=texture(image_Y,texCoord).r;
        yuv.g=texture(image_U,texCoord).r-0.5f;
        yuv.b=texture(image_V,texCoord).r-0.5f;
    }
    else if(format==AV_PIX_FMT_NV12)
    {
        yuv.r=texture(image_Y,texCoord).r;
        yuv.gb=texture(image_UV,texCoord).rg-0.5f;
    }
    vec3 rgb=mat3(vec3(1,1,1),vec3(0,-0.395,2.032),vec3(1.140,-0.581,0))*yuv;
    FragColor=vec4(rgb,1.0f);
}