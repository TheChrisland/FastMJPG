#include "../include/GLAD.h"
#include "../include/GLFW3.h"
#include "../include/VideoRenderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* VIDEO_RENDERER_VERTEX_SHADER = "#version 330 core\n"
                                                  "\n"
                                                  "layout(location = 0) in vec3 aPos;\n"
                                                  "layout(location = 1) in vec2 aTexCoord;\n"
                                                  "out vec2 TexCoord;\n"
                                                  "\n"
                                                  "void main() {\n"
                                                  "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                                  "    TexCoord    = aTexCoord;\n"
                                                  "}\n";

static const char* VIDEO_RENDERER_FRAGMENT_SHADER = "#version 330 core\n"
                                                    "\n"
                                                    "uniform sampler2D image_texture;\n"
                                                    "in vec2           TexCoord;\n"
                                                    "out vec4          FragColor;\n"
                                                    "\n"
                                                    "void main() {\n"
                                                    "    FragColor = texture(image_texture, TexCoord);\n"
                                                    "}\n";

static void glFrameBufferSizeCallback(GLFWwindow* window, int windowWidth, int windowHeight) {
    VideoRenderer* videoRenderer     = glfwGetWindowUserPointer(window);
    float          windowWidthFloat  = (float)windowWidth;
    float          windowHeightFloat = (float)windowHeight;
    float          sourceWidthFloat  = (float)videoRenderer->sourceWidth;
    float          sourceHeightFloat = (float)videoRenderer->sourceHeight;
    glViewport(0, 0, windowWidth, windowHeight);
    float ratioWidth  = windowWidthFloat / sourceWidthFloat;
    float ratioHeight = windowHeightFloat / sourceHeightFloat;
    float imageRatio;
    if (ratioWidth < ratioHeight) {
        imageRatio = ratioWidth;
    } else {
        imageRatio = ratioHeight;
    }
    float scaledImageWidth      = sourceWidthFloat * imageRatio;
    float scaledImageHeight     = sourceHeightFloat * imageRatio;
    float padX                  = (windowWidthFloat - scaledImageWidth) / 2.0f;
    float padY                  = (windowHeightFloat - scaledImageHeight) / 2.0f;
    float leftX                 = (padX / windowWidthFloat * 2.0f) - 1.0f;
    float rightX                = ((padX + scaledImageWidth) / windowWidthFloat * 2.0f) - 1.0f;
    float topY                  = (padY / windowHeightFloat * 2.0f) - 1.0f;
    float bottomY               = ((padY + scaledImageHeight) / windowHeightFloat * 2.0f) - 1.0f;
    videoRenderer->vertices[0]  = leftX;
    videoRenderer->vertices[1]  = bottomY;
    videoRenderer->vertices[5]  = rightX;
    videoRenderer->vertices[6]  = bottomY;
    videoRenderer->vertices[10] = leftX;
    videoRenderer->vertices[11] = topY;
    videoRenderer->vertices[15] = rightX;
    videoRenderer->vertices[16] = topY;
    glBindBuffer(GL_ARRAY_BUFFER, videoRenderer->vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(videoRenderer->vertices), videoRenderer->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static unsigned int loadGLShader(const char* shaderSource, int shaderType) {
    unsigned int shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, (const char* const*)&shaderSource, NULL);
    glCompileShader(shader);
    int  success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "Error: Shader compilation failed. %s\n%s\n", shaderSource, infoLog);
        exit(EXIT_FAILURE);
    }
    return shader;
}

static unsigned int loadGLProgram() {
    unsigned int vertexShader   = loadGLShader(VIDEO_RENDERER_VERTEX_SHADER, GL_VERTEX_SHADER);
    unsigned int fragmentShader = loadGLShader(VIDEO_RENDERER_FRAGMENT_SHADER, GL_FRAGMENT_SHADER);
    unsigned int shaderProgram  = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    int  success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "Error: shader linking failed. %s\n%s\n%s\n", VIDEO_RENDERER_VERTEX_SHADER, VIDEO_RENDERER_FRAGMENT_SHADER, infoLog);
        exit(EXIT_FAILURE);
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

VideoRenderer* VideoRendererCreate(unsigned int sourceWidth, unsigned int sourceHeight, unsigned int windowWidth, unsigned int windowHeight, char* windowTitle) {
    VideoRenderer* videoRenderer = malloc(sizeof(VideoRenderer));
    if (videoRenderer == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for VideoRenderer.\n");
        exit(EXIT_FAILURE);
    }
    memset(videoRenderer, 0, sizeof(VideoRenderer));
    videoRenderer->sourceWidth  = sourceWidth;
    videoRenderer->sourceHeight = sourceHeight;
    videoRenderer->windowWidth  = windowWidth;
    videoRenderer->windowHeight = windowHeight;
    videoRenderer->windowTitle  = windowTitle;
    videoRenderer->vertices[0]  = -1.0f;
    videoRenderer->vertices[1]  = 1.0f;
    videoRenderer->vertices[2]  = 0.0f;
    videoRenderer->vertices[3]  = 0.0f;
    videoRenderer->vertices[4]  = 0.0f;
    videoRenderer->vertices[5] = 1.0f;
    videoRenderer->vertices[6] = 1.0f;
    videoRenderer->vertices[7] = 0.0f;
    videoRenderer->vertices[8] = 1.0f;
    videoRenderer->vertices[9] = 0.0f;
    videoRenderer->vertices[10] = -1.0f;
    videoRenderer->vertices[11] = -1.0f;
    videoRenderer->vertices[12] = 0.0f;
    videoRenderer->vertices[13] = 0.0f;
    videoRenderer->vertices[14] = 1.0f;
    videoRenderer->vertices[15] = 1.0f;
    videoRenderer->vertices[16] = -1.0f;
    videoRenderer->vertices[17] = 0.0f;
    videoRenderer->vertices[18] = 1.0f;
    videoRenderer->vertices[19] = 1.0f;
    videoRenderer->indices[0] = 0;
    videoRenderer->indices[1] = 1;
    videoRenderer->indices[2] = 2;
    videoRenderer->indices[3] = 1;
    videoRenderer->indices[4] = 3;
    videoRenderer->indices[5] = 2;
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    videoRenderer->window = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
    if (videoRenderer->window == NULL) {
        fprintf(stderr, "Error: Failed to create glfw window.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(videoRenderer->window);
    glfwSetWindowUserPointer(videoRenderer->window, videoRenderer);
    glfwSetFramebufferSizeCallback(videoRenderer->window, glFrameBufferSizeCallback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize glad.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    videoRenderer->shaderProgram = loadGLProgram();
    glGenTextures(1, &videoRenderer->texture);
    glBindTexture(GL_TEXTURE_2D, videoRenderer->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sourceWidth, sourceHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glGenVertexArrays(1, &videoRenderer->vertexArrayObject);
    glGenBuffers(1, &videoRenderer->vertexBufferObject);
    glGenBuffers(1, &videoRenderer->elementBufferObject);
    glBindVertexArray(videoRenderer->vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, videoRenderer->vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(videoRenderer->vertices), videoRenderer->vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, videoRenderer->elementBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(videoRenderer->indices), videoRenderer->indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glFrameBufferSizeCallback(videoRenderer->window, videoRenderer->windowWidth, videoRenderer->windowHeight);
    return videoRenderer;
}

void VideoRendererRender(VideoRenderer* videoRenderer, void* frame) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoRenderer->sourceWidth, videoRenderer->sourceHeight, GL_RGB, GL_UNSIGNED_BYTE, frame);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(videoRenderer->shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, videoRenderer->elementBufferObject);
    glBindVertexArray(videoRenderer->vertexArrayObject);
    glBindTexture(GL_TEXTURE_2D, videoRenderer->texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glfwSwapBuffers(videoRenderer->window);
    glfwPollEvents();
}

void VideoRendererFree(VideoRenderer* videoRenderer) {
    glfwPollEvents();
    glfwDestroyWindow(videoRenderer->window);
    glDeleteVertexArrays(1, &videoRenderer->vertexArrayObject);
    glDeleteBuffers(1, &videoRenderer->vertexBufferObject);
    glDeleteBuffers(1, &videoRenderer->elementBufferObject);
    glDeleteProgram(videoRenderer->shaderProgram);
    glfwTerminate();
    free(videoRenderer);
}