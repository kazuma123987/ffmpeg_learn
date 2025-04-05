#ifndef __SHADER__H
#define __SHADER__H
#include <stdio.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
class Shader
{
public:
    GLuint shaderProgram;
    std::unordered_map<const char *, int> uniformCache;
    Shader() {};
    Shader &Use();
    void compile(const char *vCode, const char *fCode, const char *gCode = nullptr);
    void unfm1f(const char *str, float value);
    void unfm1fN(const char *str, float *ptr, int count = 1);
    void unfm1i(const char *str, int value);
    void unfm1iN(const char *str, int *ptr, int count = 1);
    void unfm2f(const char *str, float a, float b);
    void unfm3f(const char *str, float a, float b, float c);
    void unfm4f(const char *str, float a, float b, float c, float d);
    void unfvec2(const char *str, glm::vec2 vec2);
    void unfvec2N(const char *str, float *ptr, int count = 1);
    void unfvec3(const char *str, glm::vec3 vec3);
    void unfvec3N(const char *str, float *ptr, int count = 1);
    void unfvec4(const char *str, glm::vec4 vec4);
    void unfvec4N(const char *str, float *ptr, int count = 1);
    void unfmat3(const char *str, glm::mat3 mat3);
    void unfmat4(const char *str, glm::mat4 mat4);

private:
    int getUniformLocation(const char *str);
    void checkShaderErrors(GLuint obj, const char *type);
};

int checkGLError(void);
#endif