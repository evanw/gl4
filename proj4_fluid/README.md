## Project 4: SPH Fluid Simulation

Controls:

* Drag mouse: rotate camera
* Mouse wheel: zoom camera
* 1 through 4: different initial configurations
* U: push all particles in a random direction
* P: pause simulation
* K: kill the velocity of all particles
* E: explode the simulation

## Introduction

This project implements a fluid simulation using lots of tiny spherical particles and a method called Smoothed Particle Hydrodynamics. It uses the simple O(n^2) algorithm that computes the force on each particle due to all other particles. However, the GPU can still simulate over 16,000 particles at real-time framerates.

## Implementation

This was implemented using OpenGL 4 with Verlet integration. Under Verlet integration, the next position of the particle is calculated using only the previous two positions and the acceleration: next = 2 * current - previous + acceleration. I stored this information for all 16,384 particles in three 128x128 textures (for the previous, current, and next positions).

The simulation was implemented using [Lagrangian Fluid Dynamics Using Smoothed Particle Hydrodynamics](http://image.diku.dk/projects/media/kelager.06.pdf) as a reference. Although the computations technically require a per-particle mass density to be computed as a separate pass before computing particle forces, re-using the mass density from the previous frame gave a noticable speedup and didn't have a visible effect on the simulation. The mass density is stored in the w-component of the position to avoid extra texture fetches in the update step.

Particles are constrained to an inside-out cube and bounce off the sides with an elasticity of 0.5. I also added two additional volume types: upright cylinder and axis-aligned box.

## Rendering

The particles were rendered using point primitives with a size inversely proportional to the distance from the camera. Points were rendered as spheres using the distance from the center of the point to gl_FragCoord. A per-pixel normal in world-space was reconstructed and used for top-down diffuse lighting. Screen-Space Ambient Occlusion (SSAO) was added as a second pass using the reconstructed eye-space normal.
