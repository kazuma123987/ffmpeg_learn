#include "texture.h"
Texture::Texture()
    : width(0), height(0), internalFormat(GL_RGB), imageFormat(GL_RGB), wrapS(GL_REPEAT), wrapT(GL_REPEAT), minFilter(GL_LINEAR), magFilter(GL_LINEAR)
{
}
Texture::Texture(int interFmt, int imageFmt)
    : width(0), height(0), internalFormat(interFmt), imageFormat(imageFmt), wrapS(GL_REPEAT), wrapT(GL_REPEAT), minFilter(GL_LINEAR), magFilter(GL_LINEAR) {}
void Texture::Generate(int width, int height, void *data, int dataType, bool changeable)
{
    glGenTextures(1, &ID);
    this->width = width;
    this->height = height;
    glBindTexture(GL_TEXTURE_2D, ID);
    if (width % 4 == 0)
    {
        if (changeable)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, imageFormat, dataType, data);
        }
        else
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
        }
    }
    else
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (changeable)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, imageFormat, dataType, data);
        }
        else
        {
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void Texture::Bind() const
{
    glBindTexture(GL_TEXTURE_2D, ID);
}
void Texture::setFormat(int internalFormat, int imageFormat)
{
    this->internalFormat = internalFormat;
    this->imageFormat = imageFormat;
}