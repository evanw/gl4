## Project 2: Noise and Tessellation

Controls:

* Drag mouse: rotate camera
* Tab: toggle wireframe
* WASD: move camera
* = or -: Increase or decrease the tessellation level

## Noise

My terrain uses riged multifractal noise on top of the [2D simplex noise](https://github.com/ashima/webgl-noise/blob/master/src/noise2D.glsl) implementation by Ian McEwan. Seven octaves are used where every step scales the coordinate by 2 and the weight by 0.5. The creases in the noise are caused by wrapping a call to absolute value around the noise lookup.

## Tessellation

A base 128x128 grid of quads is tessellated every frame using a tesselation shader. The tesselation level is proportional to 1 / (1 + distance_from_eye). Fractional spacing is used to smoothly blend between detail levels. Cracks were avoided by calculating the tessellation level for an edge at its midpoint so neighboring quads will compute the same value.

## Shading

Crease darkening (pseudo ambient-occlusion) was computed from the terrain noise function. Each crease is the result of an absolute value at a certain octave level. The minimum of all unweighted octave levels will be a value that is zero on creases and positive in between creases. Multiplying this value by the color performs crease darkening. This needs to be done in the fragment shader to prevent artifacts due to vertex interpolation, so both the tess evaluation shader and the fragment shader have copies of the terrain noise function.

Normals are calculated cheaply by reusing the terrain height calculation used in crease darkening. The normal for a pixel is calculated using normalize(cross(dFdx(position), dFdy(position))) with the position obtained from the terrain function. This is an approximate normal calculated using screen-space partial derivatives, which are implemented with a finite difference from two neighboring shader units (one in x and one in y). Using this method causes slight artifacts but is much better than interpolating per-vertex normals.

## Post-processing

Variable-density fog was added as a post-process. The first pass writes to two render targets, one for color and one for position. The second pass renders a fullscreen quad and computes fog as the integral of e^-y over the line of sight from the eye to the position.
