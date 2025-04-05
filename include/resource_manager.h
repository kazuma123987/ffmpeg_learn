#ifndef __RESOURCE__MANAGER__H
#define __RESOURCE__MANAGER__H
#include <map>
#include "texture.h"
#include "shader.h"
#include "stb_image.h"
class ResourceManager
{
public:
    static std::map<std::string, Shader> shaders;
    static std::map<std::string, Texture> textures;
    static Shader loadShader(const char *vpath, const char *fpath, const char *gpath, std::string shaderName);
    static Shader &getShader(std::string shaderName);
    static Texture loadTexture(const char *path, std::string textureName);
    static Texture &getTexture(std::string textureName);
    static char *loadText(const char *path);
    static void Clear();

private:
    ResourceManager(){};
    static Shader loadShaderFromFile(const char *vpath, const char *fpath, const char *gpath = NULL);
    static Texture loadTextureFromFile(const char *path);
};
#endif