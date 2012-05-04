#ifndef GL4_H
#define GL4_H

// This is a small library to make OpenGL 4 easier. All objects are lazily
// initialized so it is safe to declare them at global scope (if this wasn't
// the case, they would fail to be created as the OpenGL context wouldn't exist
// yet). The provided vector libraries attempt to imitate some of GLSL syntax.
// The wrappers are intentionally "leaky" in that they don't use private
// variables so you can implement functionality that they don't have if needed.

#include <GL/gl.h>
#include <GL/glu.h>
#include <stdlib.h>
#include <stdio.h>
#include <ostream>
#include <vector>
#include <math.h>

// Definitions for new macros in case they aren't defined.
#define GL_R32F 0x822E
#define GL_RG32F 0x8230
#define GL_RGB32F 0x8815
#define GL_RGBA32F 0x8814
#define GL_PATCHES 0x000E
#define GL_PATCH_VERTICES 0x8E72
#define GL_TESS_CONTROL_SHADER 0x8E88
#define GL_TESS_EVALUATION_SHADER 0x8E87

// Forward declarations for new functions in case they aren't defined.
extern "C" {
    void glUseProgram(GLuint program);
    void glGenFramebuffers(GLsizei n, GLuint *ids);
    void glGenRenderbuffers(GLsizei n, GLuint *ids);
    void glBindFramebuffer(GLenum target, GLuint framebuffer);
    void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
    void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer);
    void glDeleteProgram(GLuint program);
    void glDeleteShader(GLuint shader);
    void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
    void glAttachShader(GLuint program, GLuint shader);
    void glLinkProgram(GLuint program);
    void glCompileShader(GLuint shader);
    void glGetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    void glGetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
    void glGenBuffers(GLsizei n, GLuint *buffers);
    void glDeleteBuffers(GLsizei n, const GLuint *buffers);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
    void glGenVertexArrays(GLsizei n, GLuint *arrays);
    void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
    void glBindVertexArray(GLuint array);
    void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
    void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount);
    void glEnableVertexAttribArray(GLuint index);
    void glDisableVertexAttribArray(GLuint index);
    void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
    void glDeleteFramebuffers(GLsizei n, GLuint *framebuffers);
    void glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers);
    void glDrawBuffers(GLsizei n, const GLenum *bufs);
    void glUniform1i(GLint location, GLint v0);
    void glUniform2i(GLint location, GLint v0, GLint v1);
    void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
    void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    void glUniform1f(GLint location, GLfloat v0);
    void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
    void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform1iv(GLint location, GLsizei count, const GLint *value);
    void glUniform2iv(GLint location, GLsizei count, const GLint *value);
    void glUniform3iv(GLint location, GLsizei count, const GLint *value);
    void glUniform4iv(GLint location, GLsizei count, const GLint *value);
    void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glPatchParameteri(GLenum pname, GLint value);
    void glPatchParameterfv(GLenum pname, const GLfloat *values);
    GLuint glCreateShader(GLenum shaderType);
    GLuint glCreateProgram();
    GLuint glCheckFramebufferStatus(GLenum target);
    GLuint glGetAttribLocation(GLuint program, const GLchar *name);
    GLuint glGetUniformLocation(GLuint program, const GLchar *name);
}

struct vec2 {
    union {
        struct { float x, y; };
        struct { float s, t; };
        struct { float r, g; };
        float xy[2];
        float st[2];
        float rg[2];
    };

    vec2() : x(), y() {}
    vec2(float x, float y) : x(x), y(y) {}
    vec2(const vec2 &xy) : x(xy.x), y(xy.y) {}
    explicit vec2(float f) : x(f), y(f) {}

    vec2 operator + () const { return vec2(+x, +y); }
    vec2 operator - () const { return vec2(-x, -y); }

    vec2 operator + (const vec2 &vec) const { return vec2(x + vec.x, y + vec.y); }
    vec2 operator - (const vec2 &vec) const { return vec2(x - vec.x, y - vec.y); }
    vec2 operator * (const vec2 &vec) const { return vec2(x * vec.x, y * vec.y); }
    vec2 operator / (const vec2 &vec) const { return vec2(x / vec.x, y / vec.y); }
    vec2 operator + (float s) const { return vec2(x + s, y + s); }
    vec2 operator - (float s) const { return vec2(x - s, y - s); }
    vec2 operator * (float s) const { return vec2(x * s, y * s); }
    vec2 operator / (float s) const { return vec2(x / s, y / s); }

    friend vec2 operator + (float s, const vec2 &vec) { return vec2(s + vec.x, s + vec.y); }
    friend vec2 operator - (float s, const vec2 &vec) { return vec2(s - vec.x, s - vec.y); }
    friend vec2 operator * (float s, const vec2 &vec) { return vec2(s * vec.x, s * vec.y); }
    friend vec2 operator / (float s, const vec2 &vec) { return vec2(s / vec.x, s / vec.y); }

    vec2 &operator += (const vec2 &vec) { return *this = *this + vec; }
    vec2 &operator -= (const vec2 &vec) { return *this = *this - vec; }
    vec2 &operator *= (const vec2 &vec) { return *this = *this * vec; }
    vec2 &operator /= (const vec2 &vec) { return *this = *this / vec; }
    vec2 &operator += (float s) { return *this = *this + s; }
    vec2 &operator -= (float s) { return *this = *this - s; }
    vec2 &operator *= (float s) { return *this = *this * s; }
    vec2 &operator /= (float s) { return *this = *this / s; }

    bool operator == (const vec2 &vec) const { return x == vec.x && y == vec.y; }
    bool operator != (const vec2 &vec) const { return x != vec.x || y != vec.y; }

    friend float length(const vec2 &v) { return sqrtf(v.x * v.x + v.y * v.y); }
    friend float dot(const vec2 &a, const vec2 &b) { return a.x * b.x + a.y * b.y; }
    friend float max(const vec2 &v) { return fmaxf(v.x, v.y); }
    friend float min(const vec2 &v) { return fminf(v.x, v.y); }
    friend vec2 max(const vec2 &a, const vec2 &b) { return vec2(fmaxf(a.x, b.x), fmaxf(a.y, b.y)); }
    friend vec2 min(const vec2 &a, const vec2 &b) { return vec2(fminf(a.x, b.x), fminf(a.y, b.y)); }
    friend vec2 floor(const vec2 &v) { return vec2(floorf(v.x), floorf(v.y)); }
    friend vec2 ceil(const vec2 &v) { return vec2(ceilf(v.x), ceilf(v.y)); }
    friend vec2 abs(const vec2 &v) { return vec2(fabsf(v.x), fabsf(v.y)); }
    friend vec2 fract(const vec2 &v) { return v - floor(v); }
    friend vec2 normalized(const vec2 &v) { return v / length(v); }

    friend std::ostream &operator << (std::ostream &out, const vec2 &v) {
        return out << "vec2(" << v.x << ", " << v.y << ")";
    }
};

struct vec3 {
    union {
        struct { float x, y, z; };
        struct { float s, t, p; };
        struct { float r, g, b; };
        float xyz[3];
        float stp[3];
        float rgb[3];
    };

    vec3() : x(), y(), z() {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3(const vec2 &xy, float z) : x(xy.x), y(xy.y), z(z) {}
    vec3(float x, const vec2 &yz) : x(x), y(yz.x), z(yz.y) {}
    vec3(const vec3 &xyz) : x(xyz.x), y(xyz.y), z(xyz.z) {}
    explicit vec3(float f) : x(f), y(f), z(f) {}

    vec3 operator + () const { return vec3(+x, +y, +z); }
    vec3 operator - () const { return vec3(-x, -y, -z); }

    vec3 operator + (const vec3 &vec) const { return vec3(x + vec.x, y + vec.y, z + vec.z); }
    vec3 operator - (const vec3 &vec) const { return vec3(x - vec.x, y - vec.y, z - vec.z); }
    vec3 operator * (const vec3 &vec) const { return vec3(x * vec.x, y * vec.y, z * vec.z); }
    vec3 operator / (const vec3 &vec) const { return vec3(x / vec.x, y / vec.y, z / vec.z); }
    vec3 operator + (float s) const { return vec3(x + s, y + s, z + s); }
    vec3 operator - (float s) const { return vec3(x - s, y - s, z - s); }
    vec3 operator * (float s) const { return vec3(x * s, y * s, z * s); }
    vec3 operator / (float s) const { return vec3(x / s, y / s, z / s); }

    friend vec3 operator + (float s, const vec3 &vec) { return vec3(s + vec.x, s + vec.y, s + vec.z); }
    friend vec3 operator - (float s, const vec3 &vec) { return vec3(s - vec.x, s - vec.y, s - vec.z); }
    friend vec3 operator * (float s, const vec3 &vec) { return vec3(s * vec.x, s * vec.y, s * vec.z); }
    friend vec3 operator / (float s, const vec3 &vec) { return vec3(s / vec.x, s / vec.y, s / vec.z); }

    vec3 &operator += (const vec3 &vec) { return *this = *this + vec; }
    vec3 &operator -= (const vec3 &vec) { return *this = *this - vec; }
    vec3 &operator *= (const vec3 &vec) { return *this = *this * vec; }
    vec3 &operator /= (const vec3 &vec) { return *this = *this / vec; }
    vec3 &operator += (float s) { return *this = *this + s; }
    vec3 &operator -= (float s) { return *this = *this - s; }
    vec3 &operator *= (float s) { return *this = *this * s; }
    vec3 &operator /= (float s) { return *this = *this / s; }

    bool operator == (const vec3 &vec) const { return x == vec.x && y == vec.y && z == vec.z; }
    bool operator != (const vec3 &vec) const { return x != vec.x || y != vec.y || z != vec.z; }

    friend float length(const vec3 &v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
    friend float dot(const vec3 &a, const vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    friend float max(const vec3 &v) { return fmaxf(fmaxf(v.x, v.y), v.z); }
    friend float min(const vec3 &v) { return fminf(fminf(v.x, v.y), v.z); }
    friend vec3 max(const vec3 &a, const vec3 &b) { return vec3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); }
    friend vec3 min(const vec3 &a, const vec3 &b) { return vec3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); }
    friend vec3 floor(const vec3 &v) { return vec3(floorf(v.x), floorf(v.y), floorf(v.z)); }
    friend vec3 ceil(const vec3 &v) { return vec3(ceilf(v.x), ceilf(v.y), ceilf(v.z)); }
    friend vec3 abs(const vec3 &v) { return vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z)); }
    friend vec3 fract(const vec3 &v) { return v - floor(v); }
    friend vec3 normalized(const vec3 &v) { return v / length(v); }
    friend vec3 cross(const vec3 &a, const vec3 &b) { return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

    friend std::ostream &operator << (std::ostream &out, const vec3 &v) {
        return out << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    }
};

struct vec4 {
    union {
        struct { float x, y, z, w; };
        struct { float s, t, p, q; };
        struct { float r, g, b, a; };
        float xyzw[4];
        float stpq[4];
        float rgba[4];
    };

    vec4() : x(), y(), z(), w() {}
    vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    vec4(const vec2 &xy, float z, float w) : x(xy.x), y(xy.y), z(z), w(w) {}
    vec4(float x, const vec2 &yz, float w) : x(x), y(yz.x), z(yz.y), w(w) {}
    vec4(float x, float y, const vec2 &zw) : x(x), y(y), z(zw.x), w(zw.y) {}
    vec4(const vec2 &xy, const vec2 &zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    vec4(const vec3 &xyz, float w) : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
    vec4(float x, const vec3 &yzw) : x(x), y(yzw.x), z(yzw.y), w(yzw.z) {}
    vec4(const vec4 &xyzw) : x(xyzw.x), y(xyzw.y), z(xyzw.z), w(xyzw.w) {}
    explicit vec4(float f) : x(f), y(f), z(f), w(f) {}

    vec4 operator + () const { return vec4(+x, +y, +z, +w); }
    vec4 operator - () const { return vec4(-x, -y, -z, -w); }

    vec4 operator + (const vec4 &vec) const { return vec4(x + vec.x, y + vec.y, z + vec.z, w + vec.w); }
    vec4 operator - (const vec4 &vec) const { return vec4(x - vec.x, y - vec.y, z - vec.z, w - vec.w); }
    vec4 operator * (const vec4 &vec) const { return vec4(x * vec.x, y * vec.y, z * vec.z, w * vec.w); }
    vec4 operator / (const vec4 &vec) const { return vec4(x / vec.x, y / vec.y, z / vec.z, w / vec.w); }
    vec4 operator + (float s) const { return vec4(x + s, y + s, z + s, w + s); }
    vec4 operator - (float s) const { return vec4(x - s, y - s, z - s, w - s); }
    vec4 operator * (float s) const { return vec4(x * s, y * s, z * s, w * s); }
    vec4 operator / (float s) const { return vec4(x / s, y / s, z / s, w / s); }

    friend vec4 operator + (float s, const vec4 &vec) { return vec4(s + vec.x, s + vec.y, s + vec.z, s + vec.w); }
    friend vec4 operator - (float s, const vec4 &vec) { return vec4(s - vec.x, s - vec.y, s - vec.z, s - vec.w); }
    friend vec4 operator * (float s, const vec4 &vec) { return vec4(s * vec.x, s * vec.y, s * vec.z, s * vec.w); }
    friend vec4 operator / (float s, const vec4 &vec) { return vec4(s / vec.x, s / vec.y, s / vec.z, s / vec.w); }

    vec4 &operator += (const vec4 &vec) { return *this = *this + vec; }
    vec4 &operator -= (const vec4 &vec) { return *this = *this - vec; }
    vec4 &operator *= (const vec4 &vec) { return *this = *this * vec; }
    vec4 &operator /= (const vec4 &vec) { return *this = *this / vec; }
    vec4 &operator += (float s) { return *this = *this + s; }
    vec4 &operator -= (float s) { return *this = *this - s; }
    vec4 &operator *= (float s) { return *this = *this * s; }
    vec4 &operator /= (float s) { return *this = *this / s; }

    bool operator == (const vec4 &vec) const { return x == vec.x && y == vec.y && z == vec.z && w == vec.w; }
    bool operator != (const vec4 &vec) const { return x != vec.x || y != vec.y || z != vec.z || w != vec.w; }

    friend float length(const vec4 &v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w); }
    friend float dot(const vec4 &a, const vec4 &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * a.w; }
    friend float max(const vec4 &v) { return fmaxf(fmaxf(v.x, v.y), fmaxf(v.z, v.w)); }
    friend float min(const vec4 &v) { return fminf(fminf(v.x, v.y), fminf(v.z, v.w)); }
    friend vec4 max(const vec4 &a, const vec4 &b) { return vec4(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z), fmaxf(a.w, b.w)); }
    friend vec4 min(const vec4 &a, const vec4 &b) { return vec4(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z), fminf(a.w, b.w)); }
    friend vec4 floor(const vec4 &v) { return vec4(floorf(v.x), floorf(v.y), floorf(v.z), floorf(v.w)); }
    friend vec4 ceil(const vec4 &v) { return vec4(ceilf(v.x), ceilf(v.y), ceilf(v.z), ceilf(v.w)); }
    friend vec4 abs(const vec4 &v) { return vec4(fabsf(v.x), fabsf(v.y), fabsf(v.z), fabsf(v.w)); }
    friend vec4 fract(const vec4 &v) { return v - floor(v); }
    friend vec4 normalized(const vec4 &v) { return v / length(v); }

    friend std::ostream &operator << (std::ostream &out, const vec4 &v) {
        return out << "vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    }
};

struct mat4 {
    union {
        struct {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };
        float m[16];
    };

    mat4() :
        m00(1), m01(), m02(), m03(),
        m10(), m11(1), m12(), m13(),
        m20(), m21(), m22(1), m23(),
        m30(), m31(), m32(), m33(1) {}
    mat4(const vec4 &r0, const vec4 &r1, const vec4 &r2, const vec4 &r3) :
        m00(r0.x), m01(r0.y), m02(r0.z), m03(r0.w),
        m10(r1.x), m11(r1.y), m12(r1.z), m13(r1.w),
        m20(r2.x), m21(r2.y), m22(r2.z), m23(r2.w),
        m30(r3.x), m31(r3.y), m32(r3.z), m33(r3.w) {}
    mat4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) :
        m00(m00), m01(m01), m02(m02), m03(m03),
        m10(m10), m11(m11), m12(m12), m13(m13),
        m20(m20), m21(m21), m22(m22), m23(m23),
        m30(m30), m31(m31), m32(m32), m33(m33) {}

    mat4 &transpose();
    mat4 &rotateX(float degrees);
    mat4 &rotateY(float degrees);
    mat4 &rotateZ(float degrees);
    mat4 &rotate(float degrees, float x, float y, float z);
    mat4 &rotate(float degrees, const vec3 &v) { return rotate(degrees, v.x, v.y, v.z); }
    mat4 &scale(float x, float y, float z);
    mat4 &scale(const vec3 &v) { return scale(v.x, v.y, v.z); }
    mat4 &translate(float x, float y, float z);
    mat4 &translate(const vec3 &v) { return translate(v.x, v.y, v.z); }
    mat4 &ortho(float l, float r, float b, float t, float n, float f);
    mat4 &frustum(float l, float r, float b, float t, float n, float f);
    mat4 &perspective(float fov, float aspect, float near, float far);
    mat4 &invert();

    mat4 &operator *= (const mat4 &t);
    vec4 operator * (const vec4 &v);
    friend vec4 operator * (const vec4 &v, const mat4 &t);
    friend std::ostream &operator << (std::ostream &out, const mat4 &t);
};

// Supports both 2D and 3D textures (2D textures are just textures with a depth
// of 1). When rendering back and forth between two textures (ping-ponging), it
// is easiest to just call swapWith() after rendering.
struct Texture {
    unsigned int id;
    int target, width, height, depth;

    Texture() : id(), target(), width(), height(), depth() {}
    ~Texture() { glDeleteTextures(1, &id); }

    void bind(int unit = 0) const { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(target, id); }
    void unbind(int unit = 0) const { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(target, 0); }

    // Create a new texture. GL_TEXTURE_2D is used if depth == 1, otherwise
    // GL_TEXTURE_3D is used.
    Texture &create(int width, int height, int depth, int internalFormat, int format, int type, int filter, int wrap, void *data = NULL);

    // Swap the members of this texture with the members of other.
    void swapWith(Texture &other);
};

// A framebuffer object that can take color attachments. Draw calls between
// bind() and unbind() are drawn to the attached textures.
//
// Usage:
//
//     FBO fbo;
//
//     fbo.attachColor(texture).check().bind();
//     // draw stuff
//     fbo.unbind();
//
struct FBO {
    unsigned int id;
    unsigned int renderbuffer;
    bool autoDepth;
    bool resizeViewport;
    int newViewport[4], oldViewport[4];
    int renderbufferWidth, renderbufferHeight;
    std::vector<unsigned int> drawBuffers;

    FBO(bool autoDepth = true, bool resizeViewport = true) : id(), renderbuffer(), autoDepth(autoDepth),
        resizeViewport(resizeViewport), newViewport(), oldViewport(), renderbufferWidth(), renderbufferHeight() {}
    ~FBO() { glDeleteFramebuffers(1, &id); glDeleteRenderbuffers(1, &renderbuffer); }

    // Draw calls between these will be drawn to attachments. If resizeViewport
    // is true this will automatically resize the viewport to the size of the
    // last attached texture.
    void bind();
    void unbind();

    // Draw to texture 2D in the indicated attachment location (or a 2D layer of
    // a 3D texture).
    FBO &attachColor(const Texture &texture, unsigned int attachment = 0, unsigned int layer = 0);

    // Stop drawing to the indicated color attachment
    FBO &detachColor(unsigned int attachment = 0);

    // Call after all attachColor() calls, validates attachments.
    FBO &check();
};

// Use this macro to pass raw GLSL to Shader::shader()
#define glsl(x) "#version 400\n" #x

// Wraps a GLSL shader program and all attached shader stages. Meant to be used
// with the glsl() macro.
//
// Usage:
//
//     // Initialization
//     Shader shader;
//     shader.vertexShader(glsl(
//         in vec4 vertex;
//         void main() {
//             gl_Position = vertex;
//         }
//     )).fragmentShader(glsl(
//         out vec4 color;
//         void main() {
//             color = vec4(1.0, 0.0, 0.0, 1.0);
//         }
//     )).link();
//
//     // Rendering
//     shader.use();
//     // Draw stuff
//     shader.unuse();
//
struct Shader {
    unsigned int id;
    std::vector<unsigned int> stages;

    Shader() : id() {}
    ~Shader();

    Shader &shader(int type, const char *source);
    Shader &vertexShader(const char *source) { return shader(GL_VERTEX_SHADER, source); }
    Shader &fragmentShader(const char *source) { return shader(GL_FRAGMENT_SHADER, source); }
    Shader &geometryShader(const char *source) { return shader(GL_GEOMETRY_SHADER, source); }
    Shader &tessControlShader(const char *source) { return shader(GL_TESS_CONTROL_SHADER, source); }
    Shader &tessEvalShader(const char *source) { return shader(GL_TESS_EVALUATION_SHADER, source); }

    void link();
    void use() const { glUseProgram(id); }
    void unuse() const { glUseProgram(0); }

    unsigned int attribute(const char *name) const { return glGetAttribLocation(id, name); }
    unsigned int uniform(const char *name) const { return glGetUniformLocation(id, name); }

    void uniformInt(const char *name, int i) const { glUniform1i(uniform(name), i); }
    void uniformFloat(const char *name, float f) const { glUniform1f(uniform(name), f); }
    void uniform(const char *name, const vec2 &v) const { glUniform2fv(uniform(name), 1, v.xy); }
    void uniform(const char *name, const vec3 &v) const { glUniform3fv(uniform(name), 1, v.xyz); }
    void uniform(const char *name, const vec4 &v) const { glUniform4fv(uniform(name), 1, v.xyzw); }
    void uniform(const char *name, const mat4 &m) const { glUniformMatrix4fv(uniform(name), 1, true, m.m); }
};

// A vertex buffer containing a certain type. For example, the simplest vertex
// buffer would be Buffer<vec3> but more complex formats could use Buffer<Vertex>
// with struct Vertex { vec3 position; vec2 coord; }. Each Buffer is meant to be
// used with a VAO.
//
// Index buffers should be specified using an integer type directly, i.e.
// Buffer<unsigned short>, since that's what VAO expects.
//
// Usage:
//
//     Buffer<vec3> vertices;
//     vertices << vec3(1, 0, 0) << vec3(0, 1, 0) << vec3(0, 0, 1);
//     vertices.upload();
//
//     Buffer<char> indices;
//     indices << 0 << 1 << 2;
//     indices.upload(GL_ELEMENT_ARRAY_BUFFER);
//
template <typename T>
struct Buffer {
    std::vector<T> data;
    unsigned int id;
    int currentTarget;

    Buffer() : id(), currentTarget() {}
    ~Buffer() {
        glDeleteBuffers(1, &id);
    }

    void bind() const { glBindBuffer(currentTarget, id); }
    void unbind() const { glBindBuffer(currentTarget, 0); }

    void upload(int target = GL_ARRAY_BUFFER, int usage = GL_STATIC_DRAW) {
        if (!id) glGenBuffers(1, &id);
        currentTarget = target;
        bind();
        glBufferData(currentTarget, data.size() * sizeof(T), data.data(), usage);
        unbind();
    }

    unsigned int size() const { return data.size(); }
    Buffer<T> &operator << (const T &t) { data.push_back(t); return *this; }
};

// Convert a C++ type to an OpenGL type enum using TypeToOpenGL<T>::value
template <typename T> struct TypeToOpenGL {};
template <> struct TypeToOpenGL<bool> { enum { value = GL_BOOL }; };
template <> struct TypeToOpenGL<float> { enum { value = GL_FLOAT }; };
template <> struct TypeToOpenGL<double> { enum { value = GL_DOUBLE }; };
template <> struct TypeToOpenGL<int> { enum { value = GL_INT }; };
template <> struct TypeToOpenGL<char> { enum { value = GL_BYTE }; };
template <> struct TypeToOpenGL<short> { enum { value = GL_SHORT }; };
template <> struct TypeToOpenGL<unsigned int> { enum { value = GL_UNSIGNED_INT }; };
template <> struct TypeToOpenGL<unsigned char> { enum { value = GL_UNSIGNED_BYTE }; };
template <> struct TypeToOpenGL<unsigned short> { enum { value = GL_UNSIGNED_SHORT }; };

// Groups a Buffer for vertices with a set of attributes for drawing. Can
// optionally include a Buffer for indices too.
//
// Usage:
//
//     // Initialization
//     VAO vao;
//     vao.create(shader, vertices, indices).attribute<float>("vertices", 3).check();
//
//     // Rendering
//     vao.draw(GL_TRIANGLE_STRIP);
//
struct VAO {
    // A holder for a Buffer so we can query information (i.e. number of vertices
    // for drawing). Type erasure is used so the VAO class doesn't need to be
    // templated.
    struct BufferHolder {
        virtual int currentTarget() const = 0;
        virtual unsigned int size() const = 0;
    };
    template <typename T>
    struct BufferHolderImpl : BufferHolder {
        const Buffer<T> &buffer;
        BufferHolderImpl(const Buffer<T> &buffer) : buffer(buffer) {}
        int currentTarget() const { return buffer.currentTarget; }
        unsigned int size() const { return buffer.size(); }
    };

    // You should not need to access these
    unsigned int id;
    int stride, offset, indexType;
    const Shader *shader;
    const BufferHolder *vertices;
    const BufferHolder *indices;

    VAO() : id(), stride(), offset(), indexType(), shader(), vertices(), indices() {}
    ~VAO() { glDeleteVertexArrays(1, &id); delete vertices; delete indices; }

    // You should not need to bind a VAO directly
    void bind() const { glBindVertexArray(id); }
    void unbind() const { glBindVertexArray(0); }

    // Create a vertex array object referencing a shader and a vertex buffer.
    // The shader is used to query the location of attributes in attribute()
    // and the vertex buffer is used to determine the number of elements to
    // draw in draw() and drawInstanced().
    template <typename Vertex>
    VAO &create(const Shader &shader, const Buffer<Vertex> &vbo) {
        delete vertices;
        delete indices;

        this->shader = &shader;
        vertices = new BufferHolderImpl<Vertex>(vbo);
        indices = NULL;
        stride = sizeof(Vertex);
        indexType = GL_INVALID_ENUM;

        if (!id) glGenVertexArrays(1, &id);
        bind();
        vbo.bind();
        unbind();

        return *this;
    }

    // Create a vertex array object referencing a shader, a vertex buffer, and
    // an index buffer. The shader is used to query the location of attributes
    // in attribute() and the index buffer is used to determine the number of
    // elements to draw in draw() and drawInstanced().
    template <typename Vertex, typename Index>
    VAO &create(const Shader &shader, const Buffer<Vertex> &vbo, const Buffer<Index> &ibo) {
        delete vertices;
        delete indices;

        this->shader = &shader;
        vertices = new BufferHolderImpl<Vertex>(vbo);
        indices = new BufferHolderImpl<Index>(ibo);
        stride = sizeof(Vertex);
        indexType = TypeToOpenGL<Index>::value;

        if (!id) glGenVertexArrays(1, &id);
        bind();
        vbo.bind();
        ibo.bind();
        unbind();

        return *this;
    }

    // Define an attribute called name in the provided shader. This attribute
    // has count elements of type T. If normalized is true, integer types are
    // mapped from 0 to 2^n-1 (where n is the bit count) to values from 0 to 1.
    //
    // Attributes should be declared in the order they are declared in the
    // vertex struct (assuming interleaved data). Call check() after declaring
    // all attributes to make sure the vertex struct is packed.
    template <typename T>
    VAO &attribute(const char *name, int count, bool normalized = false) {
        int location = shader->attribute(name);
        bind();
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location, count, TypeToOpenGL<T>::value, normalized, stride, (char *)NULL + offset);
        unbind();
        offset += count * sizeof(T);
        return *this;
    }

    // Validate VBO modes and attribute byte sizes
    void check();

    // Draw the attached VBOs
    void draw(int mode = GL_TRIANGLES) const {
        bind();
        if (indices) glDrawElements(mode, indices->size(), indexType, NULL);
        else glDrawArrays(mode, 0, vertices->size());
        unbind();
    }

    // Draw the attached VBOs using instancing
    void drawInstanced(int instances, int mode = GL_TRIANGLES) const {
        bind();
        if (indices) glDrawElementsInstanced(mode, indices->size(), indexType, NULL, instances);
        else glDrawArraysInstanced(mode, 0, vertices->size(), instances);
        unbind();
    }
};

#endif // GL4_H
