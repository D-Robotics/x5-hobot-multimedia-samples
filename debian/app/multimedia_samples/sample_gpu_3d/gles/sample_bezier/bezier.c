#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bmp.h"
#include "vdk_common.h"

// Vertex Shader
const char* vertexShaderSource = R"(
    attribute vec2 a_Position;
    void main() {
        gl_Position = vec4(a_Position, 0.0, 1.0);
    }
)";

// Fragment Shader
const char* fragmentShaderSource = R"(
    precision mediump float;
    void main() {
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
    }
)";

// Control points for Bézier curve
float controlPoints[] = { -0.8f, -0.8f, 0.0f, 0.8f, 0.8f, -0.8f };

void calculateBezierPoints(float* points, int segments, float* controlPoints) {
    float t, x, y;
    for (int i = 0; i <= segments; ++i) {
        t = (float)i / segments;
        float u = 1 - t;

        // Quadratic Bézier: B(t) = (1-t)^2 * P0 + 2(1-t)t * P1 + t^2 * P2
        x = u * u * controlPoints[0] +
            2 * u * t * controlPoints[2] +
            t * t * controlPoints[4];
        y = u * u * controlPoints[1] +
            2 * u * t * controlPoints[3] +
            t * t * controlPoints[5];

        points[i * 2] = x;
        points[i * 2 + 1] = y;
    }
}
// Main function
int main() {

	// Set env for vdk
	if (setenv("VDK_PLATFORM", "gbm", 1) != 0) {
		perror("setenv failed");
		return 1;
	}

	if (setenv("GBM_CRTC_ID", "31", 1) != 0) {
        perror("setenv failed");
        return 1;
    }

	if (setenv("GBM_CONNECTOR_ID", "75", 1) != 0) {
        perror("setenv failed");
        return 1;
    }

    // Initialize GLES、EGL、DRM、GBM(omitted for brevity)
	vdkEGL VdkEgl;
	vdk_init(&VdkEgl);
	vdkShowWindow(VdkEgl.window);

    // Compile shaders and link program
    GLuint program = glCreateProgram();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    glAttachShader(program, vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    glUseProgram(program);

    // Calculate Bézier curve points
    int segments = 100;
    float* points = (float*)malloc(segments * 2 * sizeof(float));
    calculateBezierPoints(points, segments, controlPoints);

    // Create Vertex Buffer Object(VBO)
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, segments * 2 * sizeof(float), points, GL_STATIC_DRAW);

    GLint positionLocation = glGetAttribLocation(program, "a_Position");
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);

	// Create Framebuffer Object (FBO)
	GLuint fbo, texture, depthBuffer;

	// Create framebuffer
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Create color attachment
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	// Create depth attachment
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);

	// Check framebuffer status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer error: %x\n", status);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			printf("OpenGL error: 0x%x\n", error);
		}
		return -1;
	}

    // Render to FBO
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawArrays(GL_LINE_STRIP, 0, segments);

    // Read pixels from FBO
    unsigned char* pixels = (unsigned char*)malloc(512 * 512 * 4);
    glReadPixels(0, 0, 512, 512, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Save to file (bmp format)
	SaveBmpImage("bezier.bmp", 512,512,pixels);

    // Cleanup
    free(points);
    free(pixels);
    glDeleteProgram(program);
    glDeleteBuffers(1, &vbo);
    glDeleteFramebuffers(1, &fbo);

	// destroy vdk
	vdkFinishEGL(&VdkEgl);
    return 0;
}
