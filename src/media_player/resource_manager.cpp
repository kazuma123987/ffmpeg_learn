#include "resource_manager.h"
std::map<std::string, Shader> ResourceManager::shaders;
std::map<std::string, Texture> ResourceManager::textures;
Shader ResourceManager::loadShader(const char *vpath, const char *fpath, const char *gpath, std::string shaderName)
{
    shaders[shaderName] = loadShaderFromFile(vpath, fpath, gpath);
    return shaders[shaderName];
}
Shader &ResourceManager::getShader(std::string shaderName)
{
    return shaders[shaderName];
}
Texture ResourceManager::loadTexture(const char *path, std::string textureName)
{
    textures[textureName] = loadTextureFromFile(path);
    return textures[textureName];
}
Texture &ResourceManager::getTexture(std::string textureName)
{
    return textures[textureName];
}
void ResourceManager::Clear()
{
    for (auto iter : shaders)
        glDeleteProgram(iter.second.shaderProgram);
    for (auto iter : textures)
        glDeleteTextures(1, &iter.second.ID);
}
Shader ResourceManager::loadShaderFromFile(const char *vpath, const char *fpath, const char *gpath)
{
    char *vcode, *fcode, *gcode;
    vcode = loadText(vpath);
    fcode = loadText(fpath);
    if (gpath)
    {
        gcode = loadText(gpath);
    }
    Shader shader;
    shader.compile(vcode, fcode, gpath ? gcode : nullptr);
    free(vcode);
    free(fcode);
    if (gpath)
        free(gcode);
    return shader;
}
Texture ResourceManager::loadTextureFromFile(const char *path)
{
    int width, height, nColorChannels;
    unsigned char *imageData = stbi_load(path, &width, &height, &nColorChannels, 0);
    Texture texture;
    if (imageData)
    {
        switch (nColorChannels)
        {
        case 1:
            texture.setFormat(GL_RED, GL_RED);
            break;
        case 2:
            texture.setFormat(GL_RG, GL_RG);
            break;
        case 3:
            texture.setFormat(GL_RGB, GL_RGB);
            break;
        case 4:
            texture.setFormat(GL_RGBA, GL_RGBA);
            break;
        }
        texture.Generate(width, height, imageData);
    }
    else
        printf("failed to load the image file:%s\n", path);
    stbi_image_free(imageData);
    return texture;
}
char *ResourceManager::loadText(const char *path)
{
    FILE *fp = NULL;
    fopen_s(&fp, path, "rb");
    if (fp == NULL)
    {
        printf("failed to open the file:%s", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    size_t vSize = ftell(fp);
    char *code = (char *)malloc(vSize + 1);
    rewind(fp);
    size_t readSize = fread(code, sizeof(char), vSize, fp);
    if (readSize < vSize)
        printf("failed to read text from %s\n", path);
    fclose(fp);
    code[vSize] = '\0';
    return code;
}