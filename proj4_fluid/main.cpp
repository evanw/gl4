#include <GL/glut.h>
#include "gl4.h"

const int bufferWidth = 128;
const int bufferHeight = 128;
const vec3 gridSize = vec3(0.5);

bool paused = false;
float accumulation = 0;
float width = 800, height = 600;
float angleX = -45, angleY = 45, zoomZ = length(gridSize) * 1.5;

Buffer<vec3> point;
Buffer<vec2> quad;
VAO pointLayout;
VAO quadLayout;

Shader updateShader;
Shader drawShader;
FBO bufferFBO;
FBO screenFBO;

Texture prevPositions;
Texture currPositions;
Texture nextPositions;

Texture positionDiffuseTexture;
Texture normalTexture;
Texture accumulationTextureA;
Texture accumulationTextureB;

Shader ssaoShader;
Shader textureMappingShader;

inline float frand() {
    return (float)rand() / (float)RAND_MAX;
}

enum Scene {
    RandomColumn,
    GridColumn,
    Drop,
    Top,

    SceneCount
};

void reset(Scene scene) {
    std::vector<vec4> points;
    for (int i = 0; i < bufferWidth * bufferHeight; i++) {
        vec4 point;
        const int grid = 32;
        switch (scene) {
        case RandomColumn:
            point = vec4(frand() * 0.5, frand(), frand() * 0.5, 0);
            break;
        case GridColumn:
            point = vec4((i % grid) / float(grid - 1), (i / grid / grid) / float(grid - 1), (i / grid % grid) / float(grid - 1), 0);
            break;
        case Drop:
            if ((rand() & 0xF) == 0) {
                point = vec4(frand() * 0.25, frand() * 0.25 + 0.75, frand() * 0.25, 0);
            } else {
                point = vec4(frand(), frand() * 0.25, frand(), 0);
            }
            break;
        case Top:
            point = vec4(frand() * 0.5, frand() * 0.1 + 0.9, frand() * 0.5, 0);
            break;
        default:
            return;
        }
        point = point * vec4(gridSize, 1);
        point.w = 10000000;
        points.push_back(point);
    }

    prevPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());
    currPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());
    nextPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, points.data());

    updateShader.use();
    updateShader.uniformInt("collideWithObjects", scene == Top);
    updateShader.unuse();

    accumulation = 0;
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
        uniform bool collideWithObjects;
        uniform vec3 gridSize;
        uniform sampler2D prevPositions;
        uniform sampler2D currPositions;
        uniform int bufferWidth;
        uniform int bufferHeight;
        in vec2 coord;
        out vec4 nextPosition;

        const float pi = 3.14159265;

        // The mass of an individual particle (affects mass density)
        const float particleMass = 1.0;

        // Particles will only affect other particles up to this distance
        const float maxRadius = 0.01;

        // Controls how viscous the fluid is
        const float viscosityCoefficient = 10.0;

        // A gas constant that depends on the temperature
        const float pressureConstant = 5.0e-6;

        // Rest density is a fake constant introduced by SPH for numerical stability
        const float restDensity = 1.0e-3;

        // The initial downward acceleration
        const float gravity = 9.8e-5;

        // Controls for surface tension
        const float surfaceTensionConstant = 2.0e-2;
        const float normalThreshold = 1000.0;

        // Assumes 0 <= radius <= maxRadius
        float massDensityKernel(float radius) {
            return 315.0 / (64.0 * pi * pow(maxRadius, 9.0)) * pow(pow(maxRadius, 2.0) - pow(radius, 2.0), 3.0);
        }
        float pressureKernel(float radius) {
            return -45.0 / (pi * pow(maxRadius, 6.0)) * pow(maxRadius - radius, 2.0);
        }
        float viscosityKernel(float radius) {
            return 45.0 / (pi * pow(maxRadius, 6.0)) * (maxRadius - radius);
        }
        float normalKernel(float radius) {
            return -945.0 / (32.0 * pi * pow(maxRadius, 9.0)) * radius * pow(pow(maxRadius, 2.0) - pow(radius, 2.0), 2.0);
        }
        float surfaceTensionKernel(float radius) {
            return -945.0 / (32.0 * pi * pow(maxRadius, 9.0)) * (pow(maxRadius, 2.0) - pow(radius, 2.0)) * (3.0 * pow(maxRadius, 2.0) - 7.0 * pow(radius, 2.0));
        }

        // Define the environment
        const float elasticity = 0.5;

        vec3 pushOutOfCylinder(vec3 oldPoint, vec3 newPoint, vec3 center, float radius, float height) {
            if (newPoint.y >= center.y && newPoint.y <= center.y + height) {
                vec2 delta = newPoint.xz - center.xz;
                if (length(delta) <= radius) {
                    if (oldPoint.y <= center.y) {
                        newPoint.y = min(center.y, oldPoint.y - abs(newPoint.y - oldPoint.y) * elasticity);
                    } else if (oldPoint.y >= center.y + height) {
                        newPoint.y = max(center.y + height, oldPoint.y + abs(newPoint.y - oldPoint.y) * elasticity);
                    } else {
                        vec3 normal = normalize(vec3(delta, 0.0).xzy);
                        newPoint = oldPoint + reflect(newPoint - oldPoint, normal) * elasticity;
                        newPoint.xz = center.xz + normal.xz * radius;
                    }
                }
            }
            return newPoint;
        }

        vec3 pushOutOfBox(vec3 oldPoint, vec3 newPoint, vec3 minCoord, vec3 maxCoord) {
            vec3 clamped = clamp(newPoint, minCoord, maxCoord);
            if (clamped == newPoint) {
                vec3 delta = (oldPoint - minCoord) / (maxCoord - minCoord) - 0.5;
                vec3 choice = abs(delta);
                float largest = max(max(choice.x, choice.y), choice.z);
                if (choice.x == largest) {
                    newPoint.x = oldPoint.x + (oldPoint.x - newPoint.x) * elasticity;
                    newPoint.x = (delta.x > 0) ? max(newPoint.x, maxCoord.x) : min(newPoint.x, minCoord.x);
                } else if (choice.y == largest) {
                    newPoint.y = oldPoint.y + (oldPoint.y - newPoint.y) * elasticity;
                    newPoint.y = (delta.y > 0) ? max(newPoint.y, maxCoord.y) : min(newPoint.y, minCoord.y);
                } else {
                    newPoint.z = oldPoint.z + (oldPoint.z - newPoint.z) * elasticity;
                    newPoint.z = (delta.z > 0) ? max(newPoint.z, maxCoord.z) : min(newPoint.z, minCoord.z);
                }
            }
            return newPoint;
        }

        void main() {
            // Format is (x, y, z, massDensity)
            vec4 prevPosition = texture(prevPositions, coord);
            vec4 currPosition = texture(currPositions, coord);

            // Acceleration is initially due to gravity
            vec3 force = vec3(0.0, -currPosition.w * gravity, 0.0);

            // Calculate contributions from all pairs of particles
            float massDensity = 0.0;
            float surfaceTensionMagnitude = 0.0;
            vec3 normal = vec3(0.0);
            vec3 pressureForce = vec3(0.0);
            vec3 viscosityForce = vec3(0.0);
            for (int x = 0; x < bufferWidth; x++) {
                for (int y = 0; y < bufferHeight; y++) {
                    vec4 currPairPosition = texelFetch(currPositions, ivec2(x, y), 0);
                    vec3 delta = currPairPosition.xyz - currPosition.xyz;
                    float distance = length(delta);
                    if (distance < maxRadius) {
                        massDensity += particleMass * massDensityKernel(distance);
                        if (currPosition != currPairPosition) {
                            vec3 unitDelta = delta / (distance + 0.000000001);
                            float scale = particleMass / currPairPosition.w;

                            // Compute pressure
                            float meanPressure = pressureConstant * (currPosition.w + currPairPosition.w - 2.0 * restDensity) / 2.0;
                            pressureForce += scale * meanPressure * unitDelta * pressureKernel(distance);

                            // Compute viscosity
                            vec4 prevPairPosition = texelFetch(prevPositions, ivec2(x, y), 0);
                            vec3 velocityDelta = currPairPosition.xyz - prevPairPosition.xyz - currPosition.xyz + prevPosition.xyz;
                            viscosityForce += scale * viscosityCoefficient * velocityDelta * viscosityKernel(distance);

                            // Compute surface tension
                            normal += scale * unitDelta * normalKernel(distance);
                            surfaceTensionMagnitude += scale * surfaceTensionKernel(distance);
                        }
                    }
                }
            }
            force += pressureForce;
            force += viscosityForce;
            if (dot(normal, normal) > normalThreshold) {
                force -= surfaceTensionConstant * surfaceTensionMagnitude * normalize(normal);
            }

            // Move the point by the velocity. Division by zero (massDensity) is
            // avoided since the above loop calculates the non-zero contribution
            // from the current listener on itself.
            nextPosition.xyz = 2 * currPosition.xyz - prevPosition.xyz + force / massDensity;
            nextPosition.w = massDensity;

            // Start off with the new position
            vec3 oldPosition = currPosition.xyz / gridSize;
            vec3 newPosition = nextPosition.xyz / gridSize;
            vec3 velocity = newPosition - oldPosition;

            // Bounce off the walls of the box
            vec3 clamped = clamp(newPosition, 0.0, 1.0);
            if (clamped.x != newPosition.x) velocity.x = -velocity.x * elasticity;
            if (clamped.y != newPosition.y) velocity.y = -velocity.y * elasticity;
            if (clamped.z != newPosition.z) velocity.z = -velocity.z * elasticity;

            // Update the velocity
            newPosition = oldPosition + velocity;

            // Make sure we stay outside the objects in the box
            if (collideWithObjects) {
                newPosition = pushOutOfCylinder(oldPosition, newPosition, vec3(0.5, -1, 0.5), 0.25, 3);
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, 0.8, 0.5), vec3(0.5, 1, 2));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, 0.7, -1), vec3(0.5, 0.9, 0.5));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(0.5, 0.6, -1), vec3(2, 0.8, 0.5));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(0.5, 0.5, 0.5), vec3(2, 0.7, 2));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, 0.4, 0.5), vec3(0.5, 0.6, 2));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, 0.3, -1), vec3(0.5, 0.5, 0.5));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(0.5, 0.2, -1), vec3(2, 0.4, 0.5));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(0.5, -1, 0.5), vec3(2, 0.3, 2));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, -1, 0.5), vec3(0.5, 0.2, 2));
                newPosition = pushOutOfBox(oldPosition, newPosition, vec3(-1, -1, -1), vec3(0.5, 0.1, 0.5));
            }

            // Make sure we stay inside the box
            nextPosition.xyz = clamp(newPosition, 0.0, 1.0) * gridSize;
        }
    )).link();

    drawShader.vertexShader(glsl(
        uniform int bufferWidth;
        uniform int bufferHeight;
        uniform sampler2D currPositions;
        uniform mat4 projection;
        uniform mat4 modelview;
        uniform vec2 screenSize;
        out vec4 position;
        out vec4 eyeSpace;
        out float pointSize;
        const float radius = 0.01;
        void main() {
            vec2 coord = vec2(
                float(gl_InstanceID % bufferWidth) / float(bufferWidth),
                float(gl_InstanceID / bufferWidth) / float(bufferHeight)
            );
            vec3 currPosition = texture(currPositions, coord).xyz;
            eyeSpace = modelview * vec4(currPosition, 1.0);
            position = gl_Position = projection * modelview * vec4(currPosition, 1.0);
            pointSize = gl_PointSize = screenSize.y / -eyeSpace.z * radius;
        }
    )).fragmentShader(glsl(
        uniform vec2 screenSize;
        uniform mat4 projection;
        uniform mat4 modelview;
        in vec4 position;
        in vec4 eyeSpace;
        in float pointSize;
        out vec4 positionDiffuse;
        out vec3 normal;
        const float radius = 0.01;
        void main() {
            vec2 screen = (0.5 + 0.5 * position.xy / position.w) * screenSize;
            vec2 delta = (screen - gl_FragCoord.xy) / pointSize * 2.0;
            if (length(delta) > 1.0) discard;
            vec3 eyeSpaceNormal = vec3(-delta.x, -delta.y, sqrt(1 - dot(delta, delta)));
            vec3 eyeSpacePos = eyeSpace.xyz + radius * eyeSpaceNormal;
            vec4 clipSpacePos = projection * vec4(eyeSpacePos, 1.0);
            vec3 worldSpaceNormal = normalize((vec4(eyeSpaceNormal, 0.0) * modelview).xyz);
            float diffuse = worldSpaceNormal.y * 0.5 + 0.5;
            positionDiffuse = vec4(eyeSpacePos, diffuse);
            normal = eyeSpaceNormal;
            gl_FragDepth = clipSpacePos.z / clipSpacePos.w;
        }
    )).link();

    ssaoShader.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform bool paused;
        uniform float accumulation;
        uniform float frame;
        uniform vec3 gridSize;
        uniform sampler2D positionDiffuseTexture;
        uniform sampler2D normalTexture;
        uniform sampler2D accumulationTexture;
        in vec2 coord;
        out vec4 color;
        float random(vec3 scale, float seed) {
            return fract(sin(dot(gl_FragCoord.xyz + seed, scale)) * 43758.5453 + seed);
        }
        float calcAO(vec2 offset, vec3 position, vec3 normal) {
            const float scale = 1.0;
            const float bias = -0.5;
            vec3 delta = texture(positionDiffuseTexture, coord + offset).xyz - position;
            vec3 direction = normalize(delta);
            float distance = length(delta) * scale;
            return max(0.0, dot(normal, direction) - bias) / (1.0 + distance * distance);
        }
        float ssao(vec3 position, vec3 normal) {
            const vec2[4] vectors = vec2[](vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0));
            const int iterations = 4;
            float ao = 0.0;
            float radius = length(gridSize) * 0.05 / position.z;
            vec2 aspect = vec2(dFdx(coord).x / dFdy(coord).y, 1.0);
            for (int i = 0; i < iterations; i++) {
                const float pi = 3.14159265;
                const float invSqrt2 = 0.707106781;
                float angle = random(vec3(7878.34758, 8945.34, 2784.3758678) + position * 923.0 + normal * 3487.0, float(i) + frame) * pi * 2.0;
                vec2 randomVector = vec2(cos(angle), sin(angle));
                vec2 offset1 = reflect(vectors[i], randomVector) * radius;
                vec2 offset2 = vec2(dot(offset1, vec2(invSqrt2, -invSqrt2)), dot(offset1, vec2(invSqrt2)));
                ao += calcAO(aspect * offset1 * 0.25, position, normal);
                ao += calcAO(aspect * offset1 * 0.75, position, normal);
                ao += calcAO(aspect * offset2 * 0.5, position, normal);
                ao += calcAO(aspect * offset2, position, normal);
            }
            return 1.0 - ao / float(iterations) * 0.25;
        }
        void main() {
            vec4 positionDiffuse = texture(positionDiffuseTexture, coord);
            if (positionDiffuse == vec4(0.0)) {
                color = vec4(0.0);
            } else {
                vec3 normal = texture(normalTexture, coord).xyz;
                vec3 position = positionDiffuse.xyz;
                float ambient = ssao(position, normal);
                float diffuse = positionDiffuse.w;
                color = vec4(ambient * diffuse);
            }
            if (paused) {
                vec4 old = texture(accumulationTexture, coord);
                color = (color + old * accumulation) / (accumulation + 1);
            }
        }
    )).link();

    textureMappingShader.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform sampler2D data;
        in vec2 coord;
        out vec4 color;
        void main() {
            color = texture(data, coord);
        }
    )).link();

    point << vec3();
    point.upload();
    pointLayout.create(drawShader, point).attribute<float>("vertex", 3).check();

    quad << vec2(0, 0) << vec2(1, 0) << vec2(0, 1) << vec2(1, 1);
    quad.upload();
    quadLayout.create(updateShader, quad).attribute<float>("vertex", 2).check();

    reset(Top);

    drawShader.use();
    drawShader.uniformInt("bufferWidth", bufferWidth);
    drawShader.uniformInt("bufferHeight", bufferHeight);
    drawShader.unuse();

    updateShader.use();
    updateShader.uniform("gridSize", gridSize);
    updateShader.uniformInt("bufferWidth", bufferWidth);
    updateShader.uniformInt("bufferHeight", bufferHeight);
    updateShader.uniformInt("prevPositions", 0);
    updateShader.uniformInt("currPositions", 1);
    updateShader.unuse();

    ssaoShader.use();
    ssaoShader.uniform("gridSize", gridSize);
    ssaoShader.uniformInt("positionDiffuseTexture", 0);
    ssaoShader.uniformInt("normalTexture", 1);
    ssaoShader.uniformInt("accumulationTexture", 2);
    ssaoShader.unuse();
}

void draw() {
    // Set up the camera
    mat4 projection, modelview;
    projection.perspective(45, width / height, 0.01, 1000);
    modelview.translate(0, 0, -zoomZ).rotateX(angleX).rotateY(angleY).translate(-gridSize * vec3(0.5, 0.25, 0.5));

    screenFBO.attachColor(positionDiffuseTexture, 0).attachColor(normalTexture, 1).check();
    screenFBO.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    drawShader.use();
    drawShader.uniform("screenSize", vec2(width, height));
    drawShader.uniform("projection", projection);
    drawShader.uniform("modelview", modelview);
    currPositions.bind();
    pointLayout.drawInstanced(currPositions.width * currPositions.height, GL_POINTS);
    currPositions.unbind();
    drawShader.unuse();
    glDisable(GL_DEPTH_TEST);
    screenFBO.detachColor(1).unbind();

    if (paused) {
        screenFBO.attachColor(accumulationTextureA).check();
        screenFBO.bind();
        accumulationTextureB.bind(2);
    }

    static float frame = 0;
    ssaoShader.use();
    ssaoShader.uniformFloat("accumulation", accumulation++);
    ssaoShader.uniformInt("paused", paused);
    ssaoShader.uniformFloat("frame", frame++);
    positionDiffuseTexture.bind(0);
    normalTexture.bind(1);
    quadLayout.draw(GL_TRIANGLE_STRIP);
    normalTexture.unbind(1);
    positionDiffuseTexture.unbind(0);
    ssaoShader.unuse();

    if (paused) {
        accumulationTextureB.unbind(2);
        screenFBO.unbind();
        accumulationTextureA.swapWith(accumulationTextureB);

        textureMappingShader.use();
        accumulationTextureA.bind();
        quadLayout.draw(GL_TRIANGLE_STRIP);
        accumulationTextureA.unbind();
        textureMappingShader.unuse();
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
    accumulation = 0;
}

void mousedown(int button, int, int x, int y) {
    if (button == 3) zoomZ /= 1.1;
    else if (button == 4) zoomZ *= 1.1;
    oldX = x;
    oldY = y;
    accumulation = 0;
}

void keydown(unsigned char key, int, int) {
    if (key == 27) exit(0);
    if (key >= '1' && key <= '1' + SceneCount) reset(Scene(key - '1'));

    if (key == 'k' || key == 'K') {
        std::vector<vec4> data(bufferWidth * bufferHeight);
        currPositions.bind();
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &data[0]);
        currPositions.unbind();
        prevPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, data.data());
    }

    if (key == 'p' || key == 'P') {
        paused = !paused;
        accumulation = 0;
    }

    if (key == 'u' || key == 'U') {
        std::vector<vec4> data(bufferWidth * bufferHeight);
        currPositions.bind();
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &data[0]);
        currPositions.unbind();
        float angle = frand() * M_PI * 2;
        vec2 dir = vec2(cos(angle), sin(angle)) * 0.005;
        for (size_t i = 0; i < data.size(); i++) {
            vec4 &v = data[i];
            v.x += dir.x;
            v.z += dir.y;
        }
        prevPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, data.data());
    }

    if (key == 'e' || key == 'E') {
        std::vector<vec4> data(bufferWidth * bufferHeight);
        currPositions.bind();
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &data[0]);
        currPositions.unbind();
        for (size_t i = 0; i < data.size(); i++) {
            vec4 &v = data[i];
            float theta = frand() * M_PI * 2;
            float phi = asinf(frand() * 2 - 1);
            vec3 dir = vec3(cos(theta) * cos(phi), sin(phi), sin(theta) * cos(phi)) * length(gridSize) * 0.025;
            v.x += dir.x;
            v.y += dir.y;
            v.z += dir.z;
        }
        prevPositions.create(bufferWidth, bufferHeight, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, data.data());
    }
}

void update() {
    if (!paused) {
        bufferFBO.attachColor(nextPositions).check();
        bufferFBO.bind();
        updateShader.use();
        prevPositions.bind(0);
        currPositions.bind(1);
        quadLayout.draw(GL_TRIANGLE_STRIP);
        currPositions.unbind(1);
        prevPositions.unbind(0);
        updateShader.unuse();
        bufferFBO.unbind();

        prevPositions.swapWith(currPositions);
        currPositions.swapWith(nextPositions);
    }

    draw();
}

void resize(int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, w, h);

    std::vector<float> zero(w * h * 3);
    positionDiffuseTexture.create(w, h, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    normalTexture.create(w, h, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    accumulationTextureA.create(w, h, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, zero.data());
    accumulationTextureB.create(w, h, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE, zero.data());
    accumulation = 0;
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
