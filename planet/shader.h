#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class Shader {
public:
    unsigned int ID = 0;

    Shader() = default;

    // constructor loads/compiles shaders
    Shader(const char* vertexPath, const char* fragmentPath,
        const char* geometryPath = nullptr) {
        load(vertexPath, fragmentPath, geometryPath);
    }

    // pas de copie (évite double-delete)
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // move OK
    Shader(Shader&& other) noexcept {
        ID = other.ID;
        other.ID = 0;
    }
    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            destroy();
            ID = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    ~Shader() { destroy(); }

    void use() const { glUseProgram(ID); }

    // 
    void setBool(const std::string& name, bool value) const {
        setInt(name, (int)value);
    }

    void setInt(const std::string& name, int value) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform1i(loc, value);
    }

    void setFloat(const std::string& name, float value) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform1f(loc, value);
    }

    void setVec2(const std::string& name, const glm::vec2& value) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform2fv(loc, 1, &value[0]);
    }
    void setVec2(const std::string& name, float x, float y) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform2f(loc, x, y);
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform3fv(loc, 1, &value[0]);
    }
    void setVec3(const std::string& name, float x, float y, float z) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform3f(loc, x, y, z);
    }

    void setVec4(const std::string& name, const glm::vec4& value) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform4fv(loc, 1, &value[0]);
    }
    void setVec4(const std::string& name, float x, float y, float z, float w) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniform4f(loc, x, y, z, w);
    }

    void setMat2(const std::string& name, const glm::mat2& mat) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniformMatrix2fv(loc, 1, GL_FALSE, &mat[0][0]);
    }

    void setMat3(const std::string& name, const glm::mat3& mat) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniformMatrix3fv(loc, 1, GL_FALSE, &mat[0][0]);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) const {
        GLint loc = glGetUniformLocation(ID, name.c_str());
        if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
    }

    
    bool load(const char* vertexPath, const char* fragmentPath,
        const char* geometryPath = nullptr) {
        destroy();

        std::string vertexCode, fragmentCode, geometryCode;
        if (!readFile(vertexPath, vertexCode)) {
            std::cout << "ERROR::SHADER::VERTEX_FILE_NOT_READ: " << vertexPath << "\n";
            return false;
        }
        if (!readFile(fragmentPath, fragmentCode)) {
            std::cout << "ERROR::SHADER::FRAGMENT_FILE_NOT_READ: " << fragmentPath << "\n";
            return false;
        }
        if (geometryPath && !readFile(geometryPath, geometryCode)) {
            std::cout << "ERROR::SHADER::GEOMETRY_FILE_NOT_READ: " << geometryPath << "\n";
            return false;
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        GLuint vertex = compileOne(GL_VERTEX_SHADER, vShaderCode, "VERTEX", vertexPath);
        GLuint fragment = compileOne(GL_FRAGMENT_SHADER, fShaderCode, "FRAGMENT", fragmentPath);
        if (!vertex || !fragment) {
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            return false;
        }

        GLuint geometry = 0;
        if (geometryPath) {
            const char* gShaderCode = geometryCode.c_str();
            geometry = compileOne(GL_GEOMETRY_SHADER, gShaderCode, "GEOMETRY", geometryPath);
            if (!geometry) {
                glDeleteShader(vertex);
                glDeleteShader(fragment);
                return false;
            }
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        if (geometryPath) glAttachShader(ID, geometry);
        glLinkProgram(ID);
        if (!checkLink(ID)) {
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            if (geometryPath) glDeleteShader(geometry);
            destroy();
            return false;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (geometryPath) glDeleteShader(geometry);

        return true;
    }

private:
    void destroy() {
        if (ID != 0) {
            glDeleteProgram(ID);
            ID = 0;
        }
    }

    static bool readFile(const char* path, std::string& out) {
        try {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            std::stringstream ss;
            ss << file.rdbuf();
            out = ss.str();
            return true;
        }
        catch (...) {
            return false;
        }
    }

    static GLuint compileOne(GLenum type, const char* code,
        const char* label, const char* pathForLog) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &code, nullptr);
        glCompileShader(s);

        GLint success = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar infoLog[1024];
            glGetShaderInfoLog(s, 1024, nullptr, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR (" << label << ")\n"
                << "File: " << pathForLog << "\n"
                << infoLog << "\n";
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    static bool checkLink(GLuint program) {
        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar infoLog[1024];
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR\n" << infoLog << "\n";
            return false;
        }
        return true;
    }
};

#endif
