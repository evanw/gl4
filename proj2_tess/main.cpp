#include <GL/glut.h>
#include "gl4.h"

float width = 800, height = 600;
float angleX = 0, angleY = 0;
vec3 eye;

Shader terrainShader, fogShader;
float maxTessLevel = 64;

Buffer<vec3> gridVertices;
VAO gridLayout;

Buffer<vec2> quadVertices;
VAO quadLayout;

FBO fbo;
Texture colorTexture;
Texture positionTexture;

void setup() {
    terrainShader.vertexShader(glsl(
        uniform vec3 eye;
        in vec3 vertex;
        out vec3 vPosition;
        out float distance;
        void main() {
            vPosition = vertex;
            distance = length(vPosition - eye);
        }
    )).tessControlShader(glsl(
        uniform mat4 matrix;
        uniform vec3 eye;
        uniform float maxTessLevel;
        layout(vertices=4) out;
        in vec3 vPosition[];
        in float distance[];
        out vec3 tcPosition[];
        float tessLevel(float distance) {
            return maxTessLevel / (1.0 + distance);
        }
        void main() {
            // Wrap patch to always be centered around the eye
            vec3 delta = (vPosition[0] - eye) * 0.03125;
            delta.xz -= floor(delta.xz + 0.5);
            delta = delta / 0.03125 + eye - vPosition[0];

            tcPosition[gl_InvocationID] = vPosition[gl_InvocationID] + delta;
            if (gl_InvocationID == 0) {
                vec3 v0 = vPosition[0] + delta;
                vec3 v1 = vPosition[1] + delta;
                vec3 v2 = vPosition[2] + delta;
                vec3 v3 = vPosition[3] + delta;

                // When the eye is looking up, use the top AABB points
                if (matrix[1][2] > 0.0) {
                    v0.y++;
                    v1.y++;
                    v2.y++;
                    v3.y++;
                }

                vec4 t0 = matrix * vec4(v0, 1.0);
                vec4 t1 = matrix * vec4(v1, 1.0);
                vec4 t2 = matrix * vec4(v2, 1.0);
                vec4 t3 = matrix * vec4(v3, 1.0);
                t0.x /= t0.w;
                t1.x /= t1.w;
                t2.x /= t2.w;
                t3.x /= t3.w;
                if (
                    // Frustum culling
                    (t0.x < -1.0 && t1.x < -1.0 && t2.x < -1.0 && t3.x < -1.0) ||
                    (t0.x > +1.0 && t1.x > +1.0 && t2.x > +1.0 && t3.x > +1.0) ||
                    (t0.z < -1.0 && t1.z < -1.0 && t2.z < -1.0 && t3.z < -1.0)
                ) {
                    gl_TessLevelInner[0] = 0.0;
                    gl_TessLevelInner[1] = 0.0;
                    gl_TessLevelOuter[0] = 0.0;
                    gl_TessLevelOuter[1] = 0.0;
                    gl_TessLevelOuter[2] = 0.0;
                    gl_TessLevelOuter[3] = 0.0;
                } else {
                    float d0 = length(v0 - eye);
                    float d1 = length(v1 - eye);
                    float d2 = length(v2 - eye);
                    float d3 = length(v3 - eye);
                    float tessLevelInner = tessLevel((d0 + d1 + d2 + d3) * 0.25);
                    gl_TessLevelInner[0] = tessLevelInner;
                    gl_TessLevelInner[1] = tessLevelInner;
                    gl_TessLevelOuter[0] = tessLevel((d0 + d2) * 0.5);
                    gl_TessLevelOuter[1] = tessLevel((d0 + d1) * 0.5);
                    gl_TessLevelOuter[2] = tessLevel((d1 + d3) * 0.5);
                    gl_TessLevelOuter[3] = tessLevel((d2 + d3) * 0.5);
                }
            }
        }
    )).tessEvalShader(glsl(
        layout(quads, fractional_even_spacing) in;
        in vec3 tcPosition[];
        out vec3 point;
        uniform mat4 matrix;

        //
        // Description : Array and textureless GLSL 2D simplex noise function.
        //      Author : Ian McEwan, Ashima Arts.
        //  Maintainer : ijm
        //     Lastmod : 20110822 (ijm)
        //     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
        //               Distributed under the MIT License. See LICENSE file.
        //               https://github.com/ashima/webgl-noise
        //

        vec3 mod289(vec3 x) {
          return x - floor(x * (1.0 / 289.0)) * 289.0;
        }

        vec2 mod289(vec2 x) {
          return x - floor(x * (1.0 / 289.0)) * 289.0;
        }

        vec3 permute(vec3 x) {
          return mod289(((x*34.0)+1.0)*x);
        }

        float snoise(vec2 v)
          {
          const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                              0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                             -0.577350269189626,  // -1.0 + 2.0 * C.x
                              0.024390243902439); // 1.0 / 41.0
        // First corner
          vec2 i  = floor(v + dot(v, C.yy) );
          vec2 x0 = v -   i + dot(i, C.xx);

        // Other corners
          vec2 i1;
          //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
          //i1.y = 1.0 - i1.x;
          i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
          // x0 = x0 - 0.0 + 0.0 * C.xx ;
          // x1 = x0 - i1 + 1.0 * C.xx ;
          // x2 = x0 - 1.0 + 2.0 * C.xx ;
          vec4 x12 = x0.xyxy + C.xxzz;
          x12.xy -= i1;

        // Permutations
          i = mod289(i); // Avoid truncation effects in permutation
          vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
                + i.x + vec3(0.0, i1.x, 1.0 ));

          vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
          m = m*m ;
          m = m*m ;

        // Gradients: 41 points uniformly over a line, mapped onto a diamond.
        // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

          vec3 x = 2.0 * fract(p * C.www) - 1.0;
          vec3 h = abs(x) - 0.5;
          vec3 ox = floor(x + 0.5);
          vec3 a0 = x - ox;

        // Normalise gradients implicitly by scaling m
        // Approximation of: m *= inversesqrt( a0*a0 + h*h );
          m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

        // Compute final noise value at P
          vec3 g;
          g.x  = a0.x  * x0.x  + h.x  * x0.y;
          g.yz = a0.yz * x12.xz + h.yz * x12.yw;
          return 130.0 * dot(m, g);
        }

        // Return all octaves directly because they are used to calculate
        // the diffuse color in the fragment shader. Calculating diffuse
        // color in the tess eval shader leads to horrible interpolation
        // because the abs() value doesn't interpolate across the origin.
        float terrain(vec2 coord) {
            float height = 0.0;
            float weight = 0.5;
            vec2 offset = vec2(0.0);
            coord *= 0.25;
            for (int i = 0; i < 7; i++) {
                height += abs(snoise(coord + offset)) * weight;
                offset += vec2(7.434387, 1.4567845);
                coord *= 2.0;
                weight *= 0.5;
            }
            return height * height * height;
        }

        void main() {
            // Bilinear interpolation
            vec3 p01 = mix(tcPosition[0], tcPosition[1], gl_TessCoord.x);
            vec3 p23 = mix(tcPosition[2], tcPosition[3], gl_TessCoord.x);
            point = mix(p01, p23, gl_TessCoord.y);

            // Compute vertex height
            point.y = terrain(point.xz);
        }
    )).geometryShader(glsl(
        layout(triangles) in;
        layout(triangle_strip, max_vertices = 3) out;
        uniform mat4 matrix;
        in vec3 point[];
        out vec3 p;
        out vec3 baryCoord;
        void main() {
            baryCoord = vec3(1.0, 0.0, 0.0);
            p = point[0];
            gl_Position = matrix * vec4(point[0], 1);
            EmitVertex();

            baryCoord = vec3(0.0, 1.0, 0.0);
            p = point[1];
            gl_Position = matrix * vec4(point[1], 1);
            EmitVertex();

            baryCoord = vec3(0.0, 0.0, 1.0);
            p = point[2];
            gl_Position = matrix * vec4(point[2], 1);
            EmitVertex();

            EndPrimitive();
        }
    )).fragmentShader(glsl(
        // We need high precision for normal calculation via derivatives
        precision highp float;

        //
        // Description : Array and textureless GLSL 2D simplex noise function.
        //      Author : Ian McEwan, Ashima Arts.
        //  Maintainer : ijm
        //     Lastmod : 20110822 (ijm)
        //     License : Copyright (C) 2011 Ashima Arts. All rights reserved.
        //               Distributed under the MIT License. See LICENSE file.
        //               https://github.com/ashima/webgl-noise
        //

        vec3 mod289(vec3 x) {
          return x - floor(x * (1.0 / 289.0)) * 289.0;
        }

        vec2 mod289(vec2 x) {
          return x - floor(x * (1.0 / 289.0)) * 289.0;
        }

        vec3 permute(vec3 x) {
          return mod289(((x*34.0)+1.0)*x);
        }

        float snoise(vec2 v)
          {
          const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                              0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                             -0.577350269189626,  // -1.0 + 2.0 * C.x
                              0.024390243902439); // 1.0 / 41.0
        // First corner
          vec2 i  = floor(v + dot(v, C.yy) );
          vec2 x0 = v -   i + dot(i, C.xx);

        // Other corners
          vec2 i1;
          //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
          //i1.y = 1.0 - i1.x;
          i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
          // x0 = x0 - 0.0 + 0.0 * C.xx ;
          // x1 = x0 - i1 + 1.0 * C.xx ;
          // x2 = x0 - 1.0 + 2.0 * C.xx ;
          vec4 x12 = x0.xyxy + C.xxzz;
          x12.xy -= i1;

        // Permutations
          i = mod289(i); // Avoid truncation effects in permutation
          vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
                + i.x + vec3(0.0, i1.x, 1.0 ));

          vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
          m = m*m ;
          m = m*m ;

        // Gradients: 41 points uniformly over a line, mapped onto a diamond.
        // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

          vec3 x = 2.0 * fract(p * C.www) - 1.0;
          vec3 h = abs(x) - 0.5;
          vec3 ox = floor(x + 0.5);
          vec3 a0 = x - ox;

        // Normalise gradients implicitly by scaling m
        // Approximation of: m *= inversesqrt( a0*a0 + h*h );
          m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

        // Compute final noise value at P
          vec3 g;
          g.x  = a0.x  * x0.x  + h.x  * x0.y;
          g.yz = a0.yz * x12.xz + h.yz * x12.yw;
          return 130.0 * dot(m, g);
        }

        // Return the terrain height and the "ambient occlusion" factor.
        vec2 terrain(vec2 coord) {
            float height = 0.0;
            float weight = 0.5;
            float minValue = 1.0;
            vec2 offset = vec2(0.0);
            coord *= 0.25;
            for (int i = 0; i < 7; i++) {
                float value = abs(snoise(coord + offset));
                minValue = min(value, minValue);
                height += value * weight;
                offset += vec2(7.434387, 1.4567845);
                coord *= 2.0;
                weight *= 0.5;
            }
            return vec2(height * height * height, minValue);
        }

        uniform float wireframe;
        in vec3 p;
        in vec3 baryCoord;
        out vec4 color;
        out vec3 position;

        void main() {
            // Calculate terrain height and ambient occlusion simultaneously
            vec2 tuple = terrain(p.xz);
            position = vec3(p.x, tuple.x, p.z);

            // Calculate the normal using the value of position in adjacent pixels
            vec3 normal = normalize(cross(dFdx(position), dFdy(position)));

            // Do color and lighting calculations
            const vec3 light = vec3(0.707106781, 0.707106781, 0.0);
            float weight = (max(0.0, dot(normal, light)) * 0.8 + 0.2) * (tuple.y * 0.75 + 0.25);
            float grassRock = clamp((position.y - normal.y * 0.25) * 50.0 + 5.0, 0.0, 1.0);
            color = vec4(mix(vec3(0.75 + min(1.0, position.y * 2.0), 1.0, 0.0), vec3(1.0), grassRock) * weight, 1.0);

            // Calculate an anti-aliased triangle wireframe using the screen-space derivatives of the barycentric coordinates
            vec3 g = smoothstep(vec3(0.0), fwidth(baryCoord * 0.875), abs(fract(baryCoord - 0.5) - 0.5));
            color.a = mix(1.0, 0.5 + 0.5 * min(min(g.x, g.y), g.z), wireframe);
        }
    )).link();

    fogShader.vertexShader(glsl(
        uniform mat4 invMatrix;
        in vec2 vertex;
        out vec3 ray;
        out vec2 coord;
        void main() {
            coord = vertex;
            gl_Position = vec4(vertex * 2.0 - 1.0, 0.0, 1.0);

            // Calculate the view ray
            vec4 far = invMatrix * vec4(gl_Position.xy, 1.0, 1.0);
            vec4 near = invMatrix * vec4(gl_Position.xy, 0.5, 1.0);
            ray = normalize(far.xyz / far.w - near.xyz / near.w);
        }
    )).fragmentShader(glsl(
        uniform vec3 eye;
        uniform sampler2D colorTexture;
        uniform sampler2D positionTexture;
        in vec2 coord;
        in vec3 ray;
        out vec4 color;
        void main() {
            color = texture(colorTexture, coord);
            vec3 position = texture(positionTexture, coord).xyz;

            const float scale = 10.0;
            float weight;
            if (position == vec3(0.0)) {
                // Infinite distance exponential height-based fog
                float slope = ray.y;
                if (slope > 0.0) {
                    // Finite total density upwards
                    float densityIntegral = exp(-eye.y * scale) / slope;
                    weight = 1.0 - 1.0 / (1.0 + densityIntegral);
                } else {
                    // Special-case this because the calculation is infinite
                    weight = 1.0;
                }
                color = vec4(0.0, 0.0, 0.0, 1.0);
            } else {
                // Finite distance exponential height-based fog
                vec3 look = position - eye;
                float len = length(look);
                float slope = look.y / len;
                float densityIntegral = exp(-position.y * scale) * (exp(scale * len * slope) - 1) / slope;
                weight = 1.0 - 1.0 / (1.0 + densityIntegral);
            }

            // Apply wireframe after fog
            color = vec4(mix(color.rgb, vec3(0.8, 0.85, 1.0), weight) * color.a, 1.0);
        }
    )).link();

    fogShader.use();
    fogShader.uniformInt("colorTexture", 0);
    fogShader.uniformInt("positionTexture", 1);
    fogShader.unuse();

    const int size = 128;
    const float scale = 0.125;
    for (int z = -size; z < size; z++) {
        for (int x = -size; x < size; x++) {
            gridVertices << vec3(x, 0, z) * scale;
            gridVertices << vec3(x, 0, z + 1) * scale;
            gridVertices << vec3(x + 1, 0, z) * scale;
            gridVertices << vec3(x + 1, 0, z + 1) * scale;
        }
    }
    gridVertices.upload();
    gridLayout.create(terrainShader, gridVertices).attribute<float>("vertex", 3).check();

    // Vertices
    quadVertices << vec2(0, 0) << vec2(1, 0) << vec2(0, 1) << vec2(1, 1);
    quadVertices.upload();
    quadLayout.create(fogShader, quadVertices).attribute<float>("vertex", 2).check();
}

bool left = false;
bool right = false;
bool forward = false;
bool backward = false;
bool wireframe = false;

void draw() {
    // Set up the camera
    mat4 matrix;
    mat4 modelview;
    matrix.perspective(45, width / height, 0.001, 100);
    modelview.rotateX(angleX).rotateY(angleY).translate(-eye);
    matrix *= modelview;

    fbo.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    terrainShader.use();
    terrainShader.uniformFloat("maxTessLevel", maxTessLevel);
    terrainShader.uniformFloat("wireframe", wireframe);
    terrainShader.uniform("matrix", matrix);
    terrainShader.uniform("eye", eye);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    gridLayout.draw(GL_PATCHES);
    terrainShader.unuse();
    glDisable(GL_DEPTH_TEST);
    fbo.unbind();

    fogShader.use();
    fogShader.uniform("invMatrix", matrix.invert());
    fogShader.uniform("eye", eye);
    colorTexture.bind(0);
    positionTexture.bind(1);
    quadLayout.draw(GL_TRIANGLE_STRIP);
    positionTexture.unbind(1);
    colorTexture.unbind(0);
    fogShader.unuse();

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

void mousedown(int, int, int x, int y) {
    oldX = x;
    oldY = y;
}

void keydown(unsigned char key, int, int) {
    if (key == 27) exit(0);
    if (key == '\t') wireframe = !wireframe;
    if (key == '=') maxTessLevel *= 1.1;
    if (key == '-') maxTessLevel /= 1.1;
    if (maxTessLevel < 2) maxTessLevel = 2;
    if (maxTessLevel > 256) maxTessLevel = 256;

    if (key == 'a') left = true;
    if (key == 'd') right = true;
    if (key == 'w') forward = true;
    if (key == 's') backward = true;
}

void keyup(unsigned char key, int, int) {
    if (key == 'a') left = false;
    if (key == 'd') right = false;
    if (key == 'w') forward = false;
    if (key == 's') backward = false;
}

#include <sys/time.h>
#include <time.h>

float Seconds() {
    // jump through the hoops to get the current number of microseconds since the last second tick (0 <= microseconds < 1000000)
    timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int microseconds = tv.tv_usec;

    // store the old_microseconds in a static variable, but set it to microseconds the first time so the first call to Seconds() returns 0
    static unsigned int old_microseconds = 0xFFFFFFFF;
    if (old_microseconds == 0xFFFFFFFF) old_microseconds = microseconds;

    // handle overflow so we don't generate a huge seconds value when microseconds wraps back to 0
    if (old_microseconds > microseconds) old_microseconds -= 1000000;

    float seconds = (float)(microseconds - old_microseconds) / 1000000.0f;
    old_microseconds = microseconds;
    return seconds;
}

void update() {
    const float seconds = Seconds();
    float ax = angleX * (M_PI / 180);
    float ay = angleY * (M_PI / 180);
    vec3 lr = vec3(cos(ay), 0, -sin(ay));
    vec3 fb = vec3(-sin(ay) * cos(ax), sin(ax), -cos(ay) * cos(ax));
    eye += (fb * (forward - backward) + lr * (right - left)) * seconds;
    draw();
}

void resize(int w, int h) {
    width = w;
    height = h;
    glViewport(0, 0, w, h);

    colorTexture.create(w, h, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, GL_CLAMP_TO_EDGE);
    positionTexture.create(w, h, 1, GL_RGB32F, GL_RGB, GL_FLOAT, GL_NEAREST, GL_CLAMP_TO_EDGE);
    fbo.attachColor(colorTexture, 0).attachColor(positionTexture, 1).check();
}

int main(int argc, char *argv[]) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutCreateWindow("Example");
    glutReshapeWindow(width, height);
    glutDisplayFunc(draw);
    glutKeyboardFunc(keydown);
    glutKeyboardUpFunc(keyup);
    glutMotionFunc(mousemove);
    glutMouseFunc(mousedown);
    glutReshapeFunc(resize);
    glutIdleFunc(update);
    setup();
    glutMainLoop();
    return 0;
}
