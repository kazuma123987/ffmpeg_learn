#ifndef __TEXTURE__H
#define __TEXTURE__H
#include <glad/glad.h>
#include <GLFW/glfw3.h>
class Texture
{
public:
    GLuint ID;
    int width,height;
    int internalFormat,imageFormat;
    int wrapS,wrapT;
    int minFilter,magFilter;
    Texture();
    Texture(int interFmt,int imageFmt);
    void Generate(int width,int height,void *data,int dataType=GL_UNSIGNED_BYTE,bool changeable=true);
    void Bind()const;
    void setFormat(int internalFormat,int imageFormat);
};
#endif