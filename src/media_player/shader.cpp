#include "shader.h"
Shader &Shader::Use()
{
    glUseProgram(shaderProgram);
    return *this;
}
void Shader::compile(const char *vCode, const char *fCode, const char *gCode)
{
    // 创建着色器
    GLuint vsh, fsh, gsh;
    vsh = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsh, 1, &vCode, NULL);
    glCompileShader(vsh);
    checkShaderErrors(vsh, "VERTEX");
    if (gCode)
    {
        gsh = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(gsh, 1, &gCode, NULL);
        glCompileShader(gsh);
        checkShaderErrors(gsh, "GEOMETRY");
    }
    fsh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsh, 1, &fCode, NULL);
    glCompileShader(fsh);
    checkShaderErrors(fsh, "FRAGMENT");
    // 创建着色器程序
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vsh);
    if (gCode)
        glAttachShader(shaderProgram, gsh);
    glAttachShader(shaderProgram, fsh);
    glLinkProgram(shaderProgram);
    checkShaderErrors(shaderProgram, "PROGRAM");
    // 删除着色器
    glDeleteShader(vsh);
    if (gCode)
        glDeleteShader(gsh);
    glDeleteShader(fsh);
}
void Shader::unfm1f(const char *str, float value)
{
    glUniform1f(getUniformLocation(str), value);
}
void Shader::unfm1fN(const char *str, float *ptr, int count)
{
    glUniform1fv(getUniformLocation(str), count, ptr);
}
void Shader::unfm1i(const char *str, int value)
{
    glUniform1i(getUniformLocation(str), value);
}
void Shader::unfm1iN(const char *str, int *ptr, int count)
{
    glUniform1iv(getUniformLocation(str), count, ptr);
}
void Shader::unfm2f(const char *str, float a, float b)
{
    glUniform2f(getUniformLocation(str), a, b);
}
void Shader::unfm3f(const char *str, float a, float b, float c)
{
    glUniform3f(getUniformLocation(str), a, b, c);
}
void Shader::unfm4f(const char *str, float a, float b, float c, float d)
{
    glUniform4f(getUniformLocation(str), a, b, c, d);
}
void Shader::unfvec2(const char *str, glm::vec2 vec2)
{
    glUniform2fv(getUniformLocation(str), 1, glm::value_ptr(vec2));
}
void Shader::unfvec2N(const char *str, float *ptr, int count)
{
    glUniform2fv(getUniformLocation(str), count, ptr);
}
void Shader::unfvec3(const char *str, glm::vec3 vec3)
{
    glUniform3fv(getUniformLocation(str), 1, glm::value_ptr(vec3));
}
void Shader::unfvec3N(const char *str, float *ptr, int count)
{
    glUniform3fv(getUniformLocation(str), count, ptr);
}
void Shader::unfvec4(const char *str, glm::vec4 vec4)
{
    glUniform4fv(getUniformLocation(str), 1, glm::value_ptr(vec4));
}
void Shader::unfvec4N(const char *str, float *ptr, int count)
{
    glUniform4fv(getUniformLocation(str), count, ptr);
}
void Shader::unfmat3(const char *str, glm::mat3 mat3)
{
    glUniformMatrix3fv(getUniformLocation(str), 1, GL_FALSE, glm::value_ptr(mat3));
}
void Shader::unfmat4(const char *str, glm::mat4 mat4)
{
    glUniformMatrix4fv(getUniformLocation(str), 1, GL_FALSE, glm::value_ptr(mat4));
}

int Shader::getUniformLocation(const char *str)
{
    if (uniformCache.find(str) != uniformCache.end())
        return uniformCache[str];
    int location = glGetUniformLocation(shaderProgram, str);
    uniformCache[str] = location;
    return location;
}
void Shader::checkShaderErrors(GLuint obj, const char *type)
{
    GLint success;
    char infoLog[1024];
    if (strcmp(type, "PROGRAM") != 0)
    {
        glGetShaderiv(obj, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(obj, 1024, NULL, infoLog);
            printf("ERROR::SHADER %s:\n%s\n", type, infoLog);
        }
    }
    else
    {
        glGetProgramiv(obj, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(obj, 1024, NULL, infoLog);
            printf("ERROR::PROGRAM %s:\n%s\n", type, infoLog);
        }
    }
}

static const char *getGLErrorString(GLenum err)
{
    switch (err)
    {
    case GL_NO_ERROR:
        return "无错误";
    case GL_INVALID_ENUM:
        return "GL无效枚举";
    case GL_INVALID_VALUE:
        return "GL无效值";
    case GL_INVALID_OPERATION:
        return "GL无效操作";
    case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:
        return "GL内存越界";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL无效帧缓冲操作";
    default:
        return "未知 OpenGL 错误";
    }
}

int checkGLError(void)
{
    int ret = 0;
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        std::cerr << "OpenGL错误: " << getGLErrorString(err) << std::endl;
        ret = -1;
    }
    else
    {
        ret = (int)err;
    }
    return ret;
}