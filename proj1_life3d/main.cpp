#include "gl4.h"

float width = 0, height = 0;

bool keyUp = false;
bool keyDown = false;
bool keyLeft = false;
bool keyRight = false;
bool firstPerson = false;
float cameraTransition = 0;
float angleX = 0, angleY = 0;
vec3 eye = vec3(0.5);

Shader updateShader, displayShader;
Texture textureA, textureB;
FBO fbo;

Buffer<vec2> quadVertices;
VAO quadLayout;

Buffer<vec3> cubeVertices;
Buffer<unsigned char> cubeIndices;
VAO cubeLayout;

void randomizeTextures() {
    const int size = 96;
    char data[size * size * size * 2];
    for (size_t i = 0; i < sizeof(data); i++) data[i] = (i & 1) ? 0x1F : 0xFF * ((rand() & 0xFF) < 127);
    textureA.create(size, size, size, GL_RG, GL_RG, GL_UNSIGNED_BYTE, GL_LINEAR, GL_REPEAT, data);
    textureB.create(size, size, size, GL_RG, GL_RG, GL_UNSIGNED_BYTE, GL_LINEAR, GL_REPEAT);
}

void setup() {
    randomizeTextures();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // Update shader
    updateShader.vertexShader(glsl(
        in vec2 vertex;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);
        }
    )).fragmentShader(glsl(
        uniform sampler3D data;
        uniform float startZ;
        uniform float depth;
        in vec2 coord;
        out vec4 colors[8];
        void main() {
            // For each of the 8 slices we are doing this pass
            for (int i = 0; i < 8; i++) {
                // Find the 3D texture coordinate
                vec3 pos = vec3(coord, (startZ + i + 0.5) / depth);
                vec3 delta = vec3(1.0 / depth);

                // Count active neighbors
                vec2 neighbors = vec2(0.0);
                for (int z = -1; z <= 1; z++)
                    for (int y = -1; y <= 1; y++)
                        for (int x = -1; x <= 1; x++)
                            neighbors += texture(data, pos + delta * vec3(x, y, z)).rg;
                vec2 self = texture(data, pos).rg;
                neighbors.r -= self.r;

                // Nice repeating structures (seed is randByte() < 13)
//                float next = mix(float(neighbors.r == 5.0), float(neighbors.r >= 5.0 && neighbors.r <= 7.0), self.r);

                // Generates 3D cloud structure (seed is randByte() < 127)
                float next = mix(float(neighbors.r >= 14.0 && neighbors.r <= 19.0), float(neighbors.r >= 13.0), self.r);

                // Calculate ambient occlusion by blurring the 3D buffer value using averaging over time
                colors[i] = vec4(next, mix(neighbors.r, neighbors.g, 0.99) / 27.0, 0.0, 0.0);
            }
        }
    )).link();

    // Display shader
    displayShader.vertexShader(glsl(
        uniform sampler3D data;
        uniform mat4 matrix;
        in vec3 vertex;
        out vec3 position;
        out vec3 coord;
        void main() {
            // Index into the 3D texture
            const int size = 96;
            vec3 offset = vec3(gl_InstanceID % size, (gl_InstanceID / size) % size, gl_InstanceID / (size * size));
            float value = texture(data, (offset + 0.5) / size).r;

            // If the cell is empty, move it out of the view so it won't be drawn
            if (value > 0.5) {
                gl_Position = matrix * vec4((vertex + offset) / size, 1.0);
                position = vertex;
            } else {
                gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
            }

            // 3D Texture coordinate
            coord = (offset + vertex) / size;
        }
    )).fragmentShader(glsl(
        uniform sampler3D data;
        in vec3 position;
        in vec3 coord;
        out vec4 color;
        void main() {
            // Calculate surface normal
            vec3 normal = normalize(cross(dFdx(position), dFdy(position)));

            // Calculate ambient occlusion
            vec2 value = texture(data, coord).rg;
            float smallScale = clamp(1.5 - value.r, 0.0, 1.0);
            float largeScale = clamp(1.5 - 3.0 * value.g, 0.0, 1.0);
            color = vec4(smallScale * largeScale);

            // No ambient occlusion on boundary fragments
            vec3 boundary = coord + normal * 0.001;
            if (boundary != clamp(boundary, 0.0, 1.0)) color = vec4(1.0);

            // Add some color
            vec3 light = normalize(vec3(1.0, 3.0, 2.0));
            color *= mix(vec4(0.4, 0.5, 0.6, 0.0), vec4(0.6, 0.65, 0.7, 0.0), 0.5 + 0.5 * dot(light, normal));
        }
    )).link();

    // Vertices
    for (int i = 0; i < 8; i++) cubeVertices << vec3(!!(i & 1), !!(i & 2), !!(i & 4));
    for (int i = 0; i < 4; i++) quadVertices << vec2(!!(i & 1), !!(i & 2));
    cubeVertices.upload();
    quadVertices.upload();

    // Indices
    cubeIndices << 0 << 2 << 3 << 1;
    cubeIndices << 4 << 5 << 7 << 6;
    cubeIndices << 0 << 1 << 5 << 4;
    cubeIndices << 2 << 6 << 7 << 3;
    cubeIndices << 0 << 4 << 6 << 2;
    cubeIndices << 1 << 3 << 7 << 5;
    cubeIndices.upload(GL_ELEMENT_ARRAY_BUFFER);

    // Vertex array layout
    cubeLayout.create(displayShader, cubeVertices, cubeIndices).attribute<float>("vertex", 3).check();
    quadLayout.create(updateShader, quadVertices).attribute<float>("vertex", 2).check();
}

void draw() {
    // Set up the camera
    mat4 matrix;
    matrix.perspective(45, width / height, 0.001, 10).translate(0, 0, -2 * (1 - cameraTransition));
    matrix.rotateX(angleX).rotateY(angleY).translate(-eye * cameraTransition - vec3(0.5 * (1 - cameraTransition)));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render the volume using instanced cubes
    displayShader.use();
    displayShader.uniform("matrix", matrix);
    textureA.bind();
    cubeLayout.drawInstanced(textureA.width * textureA.height * textureA.depth, GL_QUADS);
    textureA.unbind();
    displayShader.unuse();

    glutSwapBuffers();
}

void update() {
    const int step = 8;

    // Update a single step
    updateShader.use();
    updateShader.uniformFloat("depth", textureA.depth);
    textureA.bind();
    for (int z = 0; z < textureA.depth; z += step) {
        for (int attachment = 0; attachment < step; attachment++) {
            fbo.attachColor(textureB, attachment, z + attachment);
        }
        fbo.check();

        updateShader.uniformFloat("startZ", z);
        fbo.bind();
        quadLayout.draw(GL_TRIANGLE_STRIP);
        fbo.unbind();
    }
    textureA.unbind();
    updateShader.unuse();
    textureA.swapWith(textureB);

    // Transition from first person to third person and back
    cameraTransition = firstPerson * 0.1 + cameraTransition * 0.9;
    if (!firstPerson) eye = 0.05 + eye * 0.9;

    // Move around in first person
    if (firstPerson) {
        const float speed = 0.005;
        const float deg2rad = M_PI / 180;
        float sx = sin(angleX * deg2rad), cx = cos(angleX * deg2rad);
        float sy = sin(angleY * deg2rad), cy = cos(angleY * deg2rad);
        eye += vec3(cy, 0, -sy) * (keyRight - keyLeft) * speed;
        eye += vec3(sy * cx, -sx, cy * cx) * (keyDown - keyUp) * speed;
    }

    draw();
}

// For calculating mouse deltas
int oldX, oldY;

void mousemove(int x, int y) {
    const float speed = firstPerson ? 0.5 : 1;
    angleY -= (x - oldX) * speed;
    angleX -= (y - oldY) * speed;
    oldX = x;
    oldY = y;
    angleX = fmaxf(-90, fminf(90, angleX));
}

void mousedown(int, int, int x, int y) {
    oldX = x;
    oldY = y;
}

void keydown(unsigned char key, int, int) {
    switch (key) {
    case 27: exit(0); break;
    case ' ': randomizeTextures(); break;
    case 'c': firstPerson = !firstPerson; break;
    case 'w': keyUp = true; break;
    case 'a': keyLeft = true; break;
    case 's': keyDown = true; break;
    case 'd': keyRight = true; break;
    }
}

void keyup(unsigned char key, int, int) {
    switch (key) {
    case 'w': keyUp = false; break;
    case 'a': keyLeft = false; break;
    case 's': keyDown = false; break;
    case 'd': keyRight = false; break;
    }
}

void resize(int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, w, h);
}

int main(int argc, char *argv[]) {
    enum { WIDTH = 800, HEIGHT = 600 };
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutCreateWindow("cs195v - life");
    glutReshapeWindow(WIDTH, HEIGHT);
    glutDisplayFunc(draw);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutReshapeFunc(resize);
    glutIdleFunc(update);
    glutMotionFunc(mousemove);
    glutMouseFunc(mousedown);
    setup();
    resize(WIDTH, HEIGHT);
    glutMainLoop();
    return 0;
}
