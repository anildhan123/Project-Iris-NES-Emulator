#include "iris/picture_pass.hpp"

#include "iris/log.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <string>

namespace iris {
namespace {

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

constexpr unsigned int kFragmentShader = 0x8B30;
constexpr unsigned int kVertexShader = 0x8B31;
constexpr unsigned int kCompileStatus = 0x8B81;
constexpr unsigned int kLinkStatus = 0x8B82;
constexpr unsigned int kInfoLogLength = 0x8B84;
constexpr unsigned int kArrayBuffer = 0x8892;
constexpr unsigned int kStaticDraw = 0x88E4;
constexpr unsigned int kCurrentProgram = 0x8B8D;
constexpr unsigned int kVertexArrayBinding = 0x85B5;
constexpr unsigned int kFramebuffer = 0x8D40;
constexpr unsigned int kFramebufferBinding = 0x8CA6;
constexpr unsigned int kColorAttachment0 = 0x8CE0;
constexpr unsigned int kFramebufferComplete = 0x8CD5;
constexpr unsigned int kTexture0 = 0x84C0;
constexpr unsigned int kRgba8 = 0x8058;
constexpr int kMaxOutputWidth = 7680;
constexpr int kMaxOutputHeight = 4320;

// Original sampling and color shader. It reads the emulated texture and writes a separate image.
constexpr const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexcoord;
out vec2 vUv;
void main() {
    vUv = aTexcoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)";

constexpr const char* kFragmentShaderSource = R"(#version 330 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uSource;
uniform vec2 uSourceSize;
uniform int uMode;
uniform float uSharpness;
uniform float uScanline;
uniform vec4 uGrade;

vec3 fetchTexel(ivec2 coord) {
    ivec2 size = ivec2(uSourceSize);
    coord = clamp(coord, ivec2(0), size - ivec2(1));
    return texelFetch(uSource, coord, 0).rgb;
}

vec3 sampleNearest(vec2 uv) {
    return fetchTexel(ivec2(floor(uv * uSourceSize)));
}

vec3 sampleLinear(vec2 uv) {
    vec2 tex = uv * uSourceSize - vec2(0.5);
    ivec2 base = ivec2(floor(tex));
    vec2 fraction = fract(tex);
    vec3 a = fetchTexel(base);
    vec3 b = fetchTexel(base + ivec2(1, 0));
    vec3 c = fetchTexel(base + ivec2(0, 1));
    vec3 d = fetchTexel(base + ivec2(1, 1));
    return mix(mix(a, b, fraction.x), mix(c, d, fraction.x), fraction.y);
}

vec3 sampleSharp(vec2 uv) {
    vec2 tex = uv * uSourceSize - vec2(0.5);
    ivec2 base = ivec2(floor(tex));
    vec2 fraction = fract(tex);
    float amount = max(uSharpness, 1.0);
    fraction = clamp((fraction - vec2(0.5)) * amount + vec2(0.5), 0.0, 1.0);
    vec3 a = fetchTexel(base);
    vec3 b = fetchTexel(base + ivec2(1, 0));
    vec3 c = fetchTexel(base + ivec2(0, 1));
    vec3 d = fetchTexel(base + ivec2(1, 1));
    return mix(mix(a, b, fraction.x), mix(c, d, fraction.x), fraction.y);
}

vec3 sampleCrt(vec2 uv) {
    vec3 color = sampleLinear(uv);
    float row = fract(uv.y * uSourceSize.y);
    float band = 0.55 + 0.45 * sin(row * 3.14159265);
    float strength = clamp(uScanline, 0.0, 1.0);
    return color * mix(1.0, band, strength);
}

vec3 gradeColor(vec3 color) {
    color = (color - vec3(0.5)) * uGrade.y + vec3(0.5 + uGrade.x);
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, uGrade.z);
    color = max(color, vec3(0.0));
    color = pow(color, vec3(1.0 / max(uGrade.w, 0.05)));
    return clamp(color, 0.0, 1.0);
}

void main() {
    vec3 color = sampleNearest(vUv);
    if (uMode == 1) {
        color = sampleLinear(vUv);
    } else if (uMode == 2) {
        color = sampleSharp(vUv);
    } else if (uMode == 3) {
        color = sampleCrt(vUv);
    }
    fragColor = vec4(gradeColor(color), 1.0);
}
)";

using GlCreateShader = unsigned int(APIENTRY*)(unsigned int type);
using GlShaderSource = void(APIENTRY*)(unsigned int shader, int count, const char* const* source, const int* length);
using GlCompileShader = void(APIENTRY*)(unsigned int shader);
using GlGetShaderiv = void(APIENTRY*)(unsigned int shader, unsigned int pname, int* params);
using GlGetShaderInfoLog = void(APIENTRY*)(unsigned int shader, int maxLength, int* length, char* infoLog);
using GlDeleteShader = void(APIENTRY*)(unsigned int shader);
using GlCreateProgram = unsigned int(APIENTRY*)();
using GlAttachShader = void(APIENTRY*)(unsigned int program, unsigned int shader);
using GlLinkProgram = void(APIENTRY*)(unsigned int program);
using GlGetProgramiv = void(APIENTRY*)(unsigned int program, unsigned int pname, int* params);
using GlGetProgramInfoLog = void(APIENTRY*)(unsigned int program, int maxLength, int* length, char* infoLog);
using GlDeleteProgram = void(APIENTRY*)(unsigned int program);
using GlUseProgram = void(APIENTRY*)(unsigned int program);
using GlGetUniformLocation = int(APIENTRY*)(unsigned int program, const char* name);
using GlUniform1i = void(APIENTRY*)(int location, int value);
using GlUniform1f = void(APIENTRY*)(int location, float value);
using GlUniform2f = void(APIENTRY*)(int location, float x, float y);
using GlUniform4f = void(APIENTRY*)(int location, float x, float y, float z, float w);
using GlGenVertexArrays = void(APIENTRY*)(int count, unsigned int* arrays);
using GlBindVertexArray = void(APIENTRY*)(unsigned int array);
using GlDeleteVertexArrays = void(APIENTRY*)(int count, const unsigned int* arrays);
using GlGenBuffers = void(APIENTRY*)(int count, unsigned int* buffers);
using GlBindBuffer = void(APIENTRY*)(unsigned int target, unsigned int buffer);
using GlBufferData = void(APIENTRY*)(unsigned int target, std::intptr_t size, const void* data, unsigned int usage);
using GlDeleteBuffers = void(APIENTRY*)(int count, const unsigned int* buffers);
using GlEnableVertexAttribArray = void(APIENTRY*)(unsigned int index);
using GlVertexAttribPointer = void(APIENTRY*)(
    unsigned int index, int size, unsigned int type, unsigned char normalized, int stride, const void* pointer);
using GlGenFramebuffers = void(APIENTRY*)(int count, unsigned int* ids);
using GlBindFramebuffer = void(APIENTRY*)(unsigned int target, unsigned int framebuffer);
using GlDeleteFramebuffers = void(APIENTRY*)(int count, const unsigned int* ids);
using GlFramebufferTexture2D = void(APIENTRY*)(
    unsigned int target, unsigned int attachment, unsigned int textarget, unsigned int texture, int level);
using GlCheckFramebufferStatus = unsigned int(APIENTRY*)(unsigned int target);
using GlDrawArrays = void(APIENTRY*)(unsigned int mode, int first, int count);
using GlActiveTexture = void(APIENTRY*)(unsigned int texture);

template <typename Function>
bool loadFunction(Function& function, const char* name) {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4191)
#endif
    function = reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return function != nullptr;
}

}  // namespace

struct PicturePass::State {
    GlCreateShader createShader = nullptr;
    GlShaderSource shaderSource = nullptr;
    GlCompileShader compileShader = nullptr;
    GlGetShaderiv getShaderiv = nullptr;
    GlGetShaderInfoLog getShaderInfoLog = nullptr;
    GlDeleteShader deleteShader = nullptr;
    GlCreateProgram createProgram = nullptr;
    GlAttachShader attachShader = nullptr;
    GlLinkProgram linkProgram = nullptr;
    GlGetProgramiv getProgramiv = nullptr;
    GlGetProgramInfoLog getProgramInfoLog = nullptr;
    GlDeleteProgram deleteProgram = nullptr;
    GlUseProgram useProgram = nullptr;
    GlGetUniformLocation getUniformLocation = nullptr;
    GlUniform1i uniform1i = nullptr;
    GlUniform1f uniform1f = nullptr;
    GlUniform2f uniform2f = nullptr;
    GlUniform4f uniform4f = nullptr;
    GlGenVertexArrays genVertexArrays = nullptr;
    GlBindVertexArray bindVertexArray = nullptr;
    GlDeleteVertexArrays deleteVertexArrays = nullptr;
    GlGenBuffers genBuffers = nullptr;
    GlBindBuffer bindBuffer = nullptr;
    GlBufferData bufferData = nullptr;
    GlDeleteBuffers deleteBuffers = nullptr;
    GlEnableVertexAttribArray enableVertexAttribArray = nullptr;
    GlVertexAttribPointer vertexAttribPointer = nullptr;
    GlGenFramebuffers genFramebuffers = nullptr;
    GlBindFramebuffer bindFramebuffer = nullptr;
    GlDeleteFramebuffers deleteFramebuffers = nullptr;
    GlFramebufferTexture2D framebufferTexture2D = nullptr;
    GlCheckFramebufferStatus checkFramebufferStatus = nullptr;
    GlDrawArrays drawArrays = nullptr;
    GlActiveTexture activeTexture = nullptr;

    unsigned int program = 0;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int framebuffer = 0;
    unsigned int output = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    int sourceLocation = -1;
    int sourceSizeLocation = -1;
    int modeLocation = -1;
    int sharpnessLocation = -1;
    int scanlineLocation = -1;
    int gradeLocation = -1;
    bool ready = false;

    ~State() { destroy(); }

    void destroy() {
        if (framebuffer != 0 && deleteFramebuffers != nullptr) {
            deleteFramebuffers(1, &framebuffer);
            framebuffer = 0;
        }
        if (output != 0) {
            glDeleteTextures(1, &output);
            output = 0;
        }
        if (vbo != 0 && deleteBuffers != nullptr) {
            deleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (vao != 0 && deleteVertexArrays != nullptr) {
            deleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (program != 0 && deleteProgram != nullptr) {
            deleteProgram(program);
            program = 0;
        }
        ready = false;
    }

    [[nodiscard]] bool load() {
        return loadFunction(createShader, "glCreateShader") && loadFunction(shaderSource, "glShaderSource") &&
               loadFunction(compileShader, "glCompileShader") && loadFunction(getShaderiv, "glGetShaderiv") &&
               loadFunction(getShaderInfoLog, "glGetShaderInfoLog") && loadFunction(deleteShader, "glDeleteShader") &&
               loadFunction(createProgram, "glCreateProgram") && loadFunction(attachShader, "glAttachShader") &&
               loadFunction(linkProgram, "glLinkProgram") && loadFunction(getProgramiv, "glGetProgramiv") &&
               loadFunction(getProgramInfoLog, "glGetProgramInfoLog") &&
               loadFunction(deleteProgram, "glDeleteProgram") && loadFunction(useProgram, "glUseProgram") &&
               loadFunction(getUniformLocation, "glGetUniformLocation") && loadFunction(uniform1i, "glUniform1i") &&
               loadFunction(uniform1f, "glUniform1f") && loadFunction(uniform2f, "glUniform2f") &&
               loadFunction(uniform4f, "glUniform4f") && loadFunction(genVertexArrays, "glGenVertexArrays") &&
               loadFunction(bindVertexArray, "glBindVertexArray") &&
               loadFunction(deleteVertexArrays, "glDeleteVertexArrays") && loadFunction(genBuffers, "glGenBuffers") &&
               loadFunction(bindBuffer, "glBindBuffer") && loadFunction(bufferData, "glBufferData") &&
               loadFunction(deleteBuffers, "glDeleteBuffers") &&
               loadFunction(enableVertexAttribArray, "glEnableVertexAttribArray") &&
               loadFunction(vertexAttribPointer, "glVertexAttribPointer") &&
               loadFunction(genFramebuffers, "glGenFramebuffers") &&
               loadFunction(bindFramebuffer, "glBindFramebuffer") &&
               loadFunction(deleteFramebuffers, "glDeleteFramebuffers") &&
               loadFunction(framebufferTexture2D, "glFramebufferTexture2D") &&
               loadFunction(checkFramebufferStatus, "glCheckFramebufferStatus") &&
               loadFunction(drawArrays, "glDrawArrays") && loadFunction(activeTexture, "glActiveTexture");
    }

    [[nodiscard]] unsigned int compile(unsigned int type, const char* source, std::string& error) {
        const unsigned int shader = createShader(type);
        shaderSource(shader, 1, &source, nullptr);
        compileShader(shader);
        int status = 0;
        getShaderiv(shader, kCompileStatus, &status);
        if (status == 0) {
            int length = 0;
            getShaderiv(shader, kInfoLogLength, &length);
            std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
            getShaderInfoLog(shader, static_cast<int>(log.size()), nullptr, log.data());
            error = log;
            deleteShader(shader);
            return 0;
        }
        return shader;
    }

    [[nodiscard]] bool buildProgram() {
        std::string error;
        const unsigned int vertex = compile(kVertexShader, kVertexShaderSource, error);
        if (vertex == 0) {
            log::error(std::string("Presentation vertex shader failed: ") + error);
            return false;
        }
        const unsigned int fragment = compile(kFragmentShader, kFragmentShaderSource, error);
        if (fragment == 0) {
            deleteShader(vertex);
            log::error(std::string("Presentation fragment shader failed: ") + error);
            return false;
        }
        program = createProgram();
        attachShader(program, vertex);
        attachShader(program, fragment);
        linkProgram(program);
        deleteShader(vertex);
        deleteShader(fragment);
        int status = 0;
        getProgramiv(program, kLinkStatus, &status);
        if (status == 0) {
            int length = 0;
            getProgramiv(program, kInfoLogLength, &length);
            std::string log(static_cast<std::size_t>(length > 0 ? length : 1), '\0');
            getProgramInfoLog(program, static_cast<int>(log.size()), nullptr, log.data());
            log::error(std::string("Presentation shader link failed: ") + log);
            deleteProgram(program);
            program = 0;
            return false;
        }
        sourceLocation = getUniformLocation(program, "uSource");
        sourceSizeLocation = getUniformLocation(program, "uSourceSize");
        modeLocation = getUniformLocation(program, "uMode");
        sharpnessLocation = getUniformLocation(program, "uSharpness");
        scanlineLocation = getUniformLocation(program, "uScanline");
        gradeLocation = getUniformLocation(program, "uGrade");
        return sourceLocation >= 0 && sourceSizeLocation >= 0 && modeLocation >= 0 && sharpnessLocation >= 0 &&
               scanlineLocation >= 0 && gradeLocation >= 0;
    }

    [[nodiscard]] bool buildGeometry() {
        const float vertices[] = {
            -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f,
            -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        };
        genVertexArrays(1, &vao);
        genBuffers(1, &vbo);
        bindVertexArray(vao);
        bindBuffer(kArrayBuffer, vbo);
        bufferData(kArrayBuffer, static_cast<std::intptr_t>(sizeof(vertices)), vertices, kStaticDraw);
        enableVertexAttribArray(0);
        vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<int>(4 * sizeof(float)), nullptr);
        enableVertexAttribArray(1);
        vertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE, static_cast<int>(4 * sizeof(float)), reinterpret_cast<const void*>(2 * sizeof(float)));
        bindVertexArray(0);
        bindBuffer(kArrayBuffer, 0);

        glGenTextures(1, &output);
        glBindTexture(GL_TEXTURE_2D, output);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        genFramebuffers(1, &framebuffer);
        return vao != 0 && vbo != 0 && output != 0 && framebuffer != 0;
    }

    [[nodiscard]] bool resize(int width, int height) {
        if (width == outputWidth && height == outputHeight) {
            return true;
        }
        glBindTexture(GL_TEXTURE_2D, output);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(kRgba8), width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        bindFramebuffer(kFramebuffer, framebuffer);
        framebufferTexture2D(kFramebuffer, kColorAttachment0, GL_TEXTURE_2D, output, 0);
        const unsigned int status = checkFramebufferStatus(kFramebuffer);
        bindFramebuffer(kFramebuffer, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (status != kFramebufferComplete) {
            log::error("Presentation target could not be created.");
            return false;
        }
        outputWidth = width;
        outputHeight = height;
        return true;
    }
};

PicturePass::PicturePass() = default;
PicturePass::~PicturePass() = default;

bool PicturePass::initialize() {
    if (state_ != nullptr) {
        return state_->ready;
    }
    auto state = std::make_unique<State>();
    if (!state->load()) {
        log::error("OpenGL shader functions are unavailable. The picture will stay crisp pixels.");
        return false;
    }
    if (!state->buildProgram() || !state->buildGeometry()) {
        return false;
    }
    state->ready = true;
    state_ = std::move(state);
    log::info("Presentation shader ready.");
    return true;
}

bool PicturePass::ready() const { return state_ != nullptr && state_->ready; }

std::uint32_t PicturePass::texture() const { return state_ == nullptr ? 0 : state_->output; }

bool PicturePass::draw(
    std::uint32_t source,
    int sourceWidth,
    int sourceHeight,
    int outputWidth,
    int outputHeight,
    const PictureAdjustments& adjustments) {
    if (!ready() || source == 0 || sourceWidth <= 0 || sourceHeight <= 0 || outputWidth <= 0 || outputHeight <= 0 ||
        outputWidth > kMaxOutputWidth || outputHeight > kMaxOutputHeight) {
        return false;
    }
    if (!state_->resize(outputWidth, outputHeight)) {
        return false;
    }

    int previousViewport[4] = {};
    int previousTexture = 0;
    int previousFramebuffer = 0;
    int previousProgram = 0;
    int previousVao = 0;
    int previousActive = 0;
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(static_cast<unsigned int>(kFramebufferBinding), &previousFramebuffer);
    glGetIntegerv(static_cast<unsigned int>(kCurrentProgram), &previousProgram);
    glGetIntegerv(static_cast<unsigned int>(kVertexArrayBinding), &previousVao);
    glGetIntegerv(0x84E0, &previousActive);
    const unsigned char blend = glIsEnabled(GL_BLEND);
    const unsigned char scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    int mode = 0;
    if (adjustments.filter == PictureFilter::Smooth) {
        mode = 1;
    } else if (adjustments.filter == PictureFilter::Sharp) {
        mode = 2;
    } else if (adjustments.filter == PictureFilter::Crt) {
        mode = 3;
    }

    state_->bindFramebuffer(kFramebuffer, state_->framebuffer);
    glViewport(0, 0, outputWidth, outputHeight);
    state_->useProgram(state_->program);
    state_->uniform1i(state_->sourceLocation, 0);
    state_->uniform2f(state_->sourceSizeLocation, static_cast<float>(sourceWidth), static_cast<float>(sourceHeight));
    state_->uniform1i(state_->modeLocation, mode);
    state_->uniform1f(state_->sharpnessLocation, adjustments.sharpness);
    state_->uniform1f(state_->scanlineLocation, adjustments.scanline);
    state_->uniform4f(
        state_->gradeLocation, adjustments.brightness, adjustments.contrast, adjustments.saturation, adjustments.gamma);
    state_->activeTexture(kTexture0);
    glBindTexture(GL_TEXTURE_2D, source);
    state_->bindVertexArray(state_->vao);
    state_->drawArrays(GL_TRIANGLES, 0, 6);

    state_->bindVertexArray(static_cast<unsigned int>(previousVao));
    state_->bindFramebuffer(kFramebuffer, static_cast<unsigned int>(previousFramebuffer));
    state_->useProgram(static_cast<unsigned int>(previousProgram));
    state_->activeTexture(static_cast<unsigned int>(previousActive));
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(previousTexture));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    if (blend != 0) {
        glEnable(GL_BLEND);
    }
    if (scissor != 0) {
        glEnable(GL_SCISSOR_TEST);
    }
    return true;
}

}  // namespace iris
