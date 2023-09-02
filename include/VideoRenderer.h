#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

#include "GLFW3.h"

#define RGB_COMPONENT_COUNT 3

typedef struct VideoRenderer {
    GLFWwindow*  window;
    unsigned int shaderProgram;
    unsigned int vertexArrayObject;
    unsigned int vertexBufferObject;
    unsigned int elementBufferObject;
    unsigned int texture;
    float        vertices[20];
    unsigned int indices[6];
    unsigned int sourceWidth;
    unsigned int sourceHeight;
    unsigned int windowWidth;
    unsigned int windowHeight;
    char*        windowTitle;
} VideoRenderer;

VideoRenderer* VideoRendererCreate(unsigned int sourceWidth, unsigned int sourceHeight, unsigned int windowWidth, unsigned int windowHeight, char* windowTitle);
void           VideoRendererRender(VideoRenderer* videoRenderer, void* frame);
void           VideoRendererFree(VideoRenderer* videoRenderer);

#endif