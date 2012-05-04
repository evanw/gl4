#include <GL/glut.h>
#include "gl4.h"

enum PostProcess {
    None,
    Accumulation,
    Bokeh,
    PostProcessCount
};

const int bufferWidth = 128;
const int bufferHeight = 128;

bool paused = false;
PostProcess postProcess = None;
float width = 800, height = 600;
float angleX = 0, angleY = 0, zoomZ = 10;

Buffer<vec3> point;
Buffer<vec2> quad;
VAO pointLayout;
VAO quadLayout;

Shader updateShader;
Shader drawShader;
FBO fbo;

Texture prevPositions;
Texture currPositions;
Texture nextPositions;

Texture renderTarget;
Texture accumulationTexture;
Texture bokehScratchA;
Texture bokehScratchB;

Shader accumulationShader;
Shader bokehFirstPass;
Shader bokehSecondPass;

inline float frand() {
    return (float)rand() / (float)RAND_MAX;
}

void reset() {
    std::vector<vec3> points;
    float offset = frand() * 100;
    for (int i = 0; i < bufferWidth * bufferHeight; i++) {
        float t = 20 * (offset + (float)i / (float)(bufferWidth * bufferHeight));
        if (i & 1) t += 100;
        mat4 matrix;
        matrix.rotateX(t * 93).rotateY(t * 37).rotateZ(t * 17);
        matrix.rotateX(t * 5).rotateY(t * 147).rotateZ(t * 71);
        vec4 vertex = matrix * vec4(0, 0, 1, 0);
        vertex.x = i & 1 ? vertex.x + 1 : -1 - vertex.x;
        points.push_back(vec3(vertex.x, vertex.y, vertex.z) * 4);
    }

    prevPositions.create(bufferWidth, bufferHeight, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());
    currPositions.create(bufferWidth, bufferHeight, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());
    nextPositions.create(bufferWidth, bufferHeight, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());
}

void setup() {
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    updateShader.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        precision highp float;
        uniform sampler2D prevPositions;
        uniform sampler2D currPositions;
        uniform int bufferWidth;
        uniform int bufferHeight;
        in vec2 coord;
        out vec3 nextPosition;
        void main() {
            vec3 prevPosition = texture(prevPositions, coord).xyz;
            vec3 currPosition = texture(currPositions, coord).xyz;
            vec3 acceleration = vec3(0.0);
            for (int x = 0; x < bufferWidth; x++) {
                for (int y = 0; y < bufferHeight; y++) {
                    vec3 position = texelFetch(currPositions, ivec2(x, y), 0).xyz;
                    vec3 dir = position - currPosition;
                    acceleration += dir / pow(dot(dir, dir) + 0.01, 1.5);
                }
            }
            nextPosition = 2 * currPosition - prevPosition + acceleration * 0.0000001;
        }
    )).link();

    drawShader.vertexShader(glsl(
        uniform int bufferWidth;
        uniform int bufferHeight;
        uniform sampler2D prevPositions;
        uniform sampler2D currPositions;
        uniform mat4 matrix;
        uniform mat4 modelview;
        uniform vec2 screenSize;
        out vec4 position;
        out float pointSize;
        out vec3 color;
        void main() {
            vec2 coord = vec2(
                float(gl_InstanceID % bufferWidth) / float(bufferWidth),
                float(gl_InstanceID / bufferWidth) / float(bufferHeight)
            );
            vec3 prevPosition = texture(prevPositions, coord).xyz;
            vec3 currPosition = texture(currPositions, coord).xyz;
            vec4 eyeSpace = modelview * vec4(currPosition, 1.0);
            position = gl_Position = matrix * vec4(currPosition, 1.0);
            pointSize = gl_PointSize = screenSize.y / length(eyeSpace) * 0.1;

            float t = length(currPosition - prevPosition);
            color = mix(vec3(0.5, 0.2, 1.0), vec3(1.0, 0.7, 0.2), t * 20.0);
        }
    )).fragmentShader(glsl(
        uniform vec2 screenSize;
        in vec4 position;
        in float pointSize;
        in vec3 color;
        out vec4 finalColor;
        void main() {
            vec2 screen = (0.5 + 0.5 * position.xy / position.w) * screenSize;
            float fade = clamp(pointSize * 0.5 - length(screen - gl_FragCoord.xy), 0.0, 1.0);
            finalColor = vec4(color * fade, 1.0);
        }
    )).link();

    accumulationShader.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform sampler2D renderTarget;
        in vec2 coord;
        out vec4 color;
        void main() {
            color = vec4(texture(renderTarget, coord).rgb, 0.01);
        }
    )).link();

    bokehFirstPass.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform sampler2D renderTarget;
        in vec2 coord;
        out vec4 colorA;
        out vec4 colorB;
        void main() {
            vec2 scale = 0.01 * vec2(dFdx(coord).x / dFdy(coord).y, 1.0);
            colorA = colorB = vec4(0.0);
            for (float t = 0.0; t <= 1.0; t += 1.0 / 32.0) {
                colorA += pow(texture(renderTarget, coord + vec2(0.0, 1.0) * t * scale), vec4(2.0));
                colorB += pow(texture(renderTarget, coord + vec2(-0.866025404, -0.5) * t * scale), vec4(2.0));
            }
            colorB = (colorA + colorB) / 66.0;
            colorA /= 33.0;
        }
    )).link();

    bokehSecondPass.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform sampler2D renderTargetA;
        uniform sampler2D renderTargetB;
        in vec2 coord;
        out vec4 color;
        void main() {
            vec2 scale = 0.01 * vec2(dFdx(coord).x / dFdy(coord).y, 1.0);
            color = vec4(0.0);
            for (float t = 0.0; t <= 1.0; t += 1.0 / 32.0) {
                color += texture(renderTargetA, coord + vec2(-0.866025404, -0.5) * t * scale);
                color += texture(renderTargetB, coord + vec2(0.866025404, -0.5) * t * scale) * 2.0;
            }
            color = pow(color / 99.0, vec4(0.5));
        }
    )).link();

    point << vec3();
    point.upload();
    pointLayout.create(drawShader, point).attribute<float>("vertex", 3).check();

    quad << vec2(0, 0) << vec2(1, 0) << vec2(0, 1) << vec2(1, 1);
    quad.upload();
    quadLayout.create(updateShader, quad).attribute<float>("vertex", 2).check();

    reset();

    drawShader.use();
    drawShader.uniformInt("bufferWidth", bufferWidth);
    drawShader.uniformInt("bufferHeight", bufferHeight);
    drawShader.uniformInt("prevPositions", 0);
    drawShader.uniformInt("currPositions", 1);
    drawShader.unuse();

    updateShader.use();
    updateShader.uniformInt("bufferWidth", bufferWidth);
    updateShader.uniformInt("bufferHeight", bufferHeight);
    updateShader.uniformInt("prevPositions", 0);
    updateShader.uniformInt("currPositions", 1);
    updateShader.unuse();

    bokehSecondPass.use();
    bokehSecondPass.uniformInt("renderTargetA", 0);
    bokehSecondPass.uniformInt("renderTargetB", 1);
    bokehSecondPass.unuse();
}

void draw() {
    // Set up the camera
    mat4 matrix, modelview;
    matrix.perspective(45, width / height, 0.01, 1000);
    modelview.translate(0, 0, -zoomZ).rotateX(angleX).rotateY(angleY);
    matrix *= modelview;

    if (postProcess) {
        fbo.attachColor(renderTarget).check();
        fbo.bind();
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
    drawShader.use();
    drawShader.uniform("screenSize", vec2(width, height));
    drawShader.uniform("matrix", matrix);
    drawShader.uniform("modelview", modelview);
    prevPositions.bind(0);
    currPositions.bind(1);
    pointLayout.drawInstanced(currPositions.width * currPositions.height, GL_POINTS);
    currPositions.unbind(1);
    prevPositions.unbind(0);
    drawShader.unuse();
    glDisable(GL_BLEND);

    if (postProcess) {
        fbo.unbind();

        if (postProcess == Accumulation) {
            fbo.attachColor(accumulationTexture).check();
            fbo.bind();
            accumulationShader.use();
            renderTarget.bind();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            quadLayout.draw(GL_TRIANGLE_STRIP);
            glDisable(GL_BLEND);
            renderTarget.unbind();
            accumulationShader.unuse();
            fbo.unbind();

            accumulationShader.use();
            accumulationTexture.bind();
            quadLayout.draw(GL_TRIANGLE_STRIP);
            accumulationTexture.unbind();
            accumulationShader.unuse();
        } else if (postProcess == Bokeh) {
            fbo.attachColor(bokehScratchA, 0).attachColor(bokehScratchB, 1).check();
            fbo.bind();
            bokehFirstPass.use();
            renderTarget.bind();
            quadLayout.draw(GL_TRIANGLE_STRIP);
            renderTarget.unbind();
            bokehFirstPass.unuse();
            fbo.unbind();
            fbo.detachColor(1);

            bokehSecondPass.use();
            bokehScratchA.bind(0);
            bokehScratchB.bind(1);
            quadLayout.draw(GL_TRIANGLE_STRIP);
            bokehScratchB.unbind(1);
            bokehScratchA.unbind(0);
            bokehSecondPass.unuse();
        }
    }

    glutSwapBuffers();
}

// For calculating mouse deltas
int oldX, oldY;

void mousemove(int x, int y) {
    angleY -= x - oldX;
    angleX -= y - oldY;
    oldX = x;
    oldY = y;
    angleX = fmaxf(-90, fminf(90, angleX));
}

void mousedown(int button, int, int x, int y) {
    if (button == 3) zoomZ /= 1.1;
    else if (button == 4) zoomZ *= 1.1;
    oldX = x;
    oldY = y;
}

void keydown(unsigned char key, int, int) {
    if (key == 27) exit(0);
    if (key == 'r' || key == 'R') reset();
    if (key == 'p' || key == 'P') paused = !paused;
    if (key == 'o' || key == 'O') postProcess = (PostProcess)((postProcess + 1) % PostProcessCount);
}

void update() {
    if (!paused) {
        fbo.attachColor(nextPositions).check();

        fbo.bind();
        updateShader.use();
        prevPositions.bind(0);
        currPositions.bind(1);
        quadLayout.draw(GL_TRIANGLE_STRIP);
        currPositions.unbind(1);
        prevPositions.unbind(0);
        updateShader.unuse();
        fbo.unbind();

        prevPositions.swapWith(currPositions);
        currPositions.swapWith(nextPositions);
    }

    draw();
}

void resize(int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, w, h);
    renderTarget.create(width, height, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_INT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    accumulationTexture.create(width, height, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    bokehScratchA.create(width, height, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    bokehScratchB.create(width, height, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);

    fbo.attachColor(accumulationTexture).check();
    fbo.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    fbo.unbind();
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutCreateWindow("Example");
    glutReshapeWindow(width, height);
    glutDisplayFunc(draw);
    glutKeyboardFunc(keydown);
    glutMotionFunc(mousemove);
    glutMouseFunc(mousedown);
    glutReshapeFunc(resize);
    glutIdleFunc(update);
    setup();
    resize(width, height);
    glutMainLoop();
    return 0;
}
