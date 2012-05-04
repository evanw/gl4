## Project 1: Conway's Game of Life in 3D

Controls:

* Drag mouse: rotate camera
* Spacebar: randomize cells
* C: toggle first/third person camera
* WASD: move camera in first person

## Introduction

Conway's Game of Life is a cellular automaton developed by John Conway. The simulation takes place on a grid of cells that are in two possible states, alive or dead. Each step, the grid is evolved using simple rules that determine the state of the cell at the next step:

* Any dead cell becomes alive if it has 3 live neighbors.
* Any live cell becomes dead if it has 1 or 4 live neighbors.

The neighbor count above checks all 8 neighbors. This produces interesting patterns that may replicate or travel and has been studied extensively.

## Extending to 3D

The algorithm can be trivially extended to 3D, except the rules must be changed to maintain insteresting behavior. Each cell has 26 neighbors in 3D instead of 8 in 2D. I experimented with many rules but there were only a few that produced results worth noting.

The first rule set resurrects dead cells if they have exactly 5 neighbors and kills live cells if they have less than 5 or exactly 8 neighbors. It was most interesting when the grid was initialized with each cell having a 5% chance of being alive. Unfortunately the directionless nature of the rules caused each pattern to grow outward in all directions which made for boring visuals.

The second rule set resurrects dead cells if they have from 14 to 19 neighbors and kills live cells if they have less than 13 neighbors. When the grid is initialized with each cell having a 50% chance of being alive, the population shrinks over time and converges to stable 3D structures.

## Implementation

My implementation uses OpenGL 4 and stores the 96x96x96 grid of cells in a 3D texture. I actually need two textures so I can read from one and write to the other (ping-pong rendering). Each step involves rendering to every 2D slice of the 3D write texture with a shader that counts the live neighbors in the 3D read texture and applies the rules to decide the next cell state. The grid domain is automatically wrapped by using the GL_REPEAT texture wrap mode along all 3 axes.

The grid is visualized using instanced cubes with one instance per grid cell. If a cell is empty the vertex shader kills the instance by moving all vertices to (-2, -2, -2). A grid size of 96x96x96 was a good tradeoff between simulation detail and rendering speed.

## Ambient occlusion

Ambient occlusion is a darkening effect that fakes indirect illumination (light rays bouncing off multiple surfaces before being seen by the viewer). Since our data is essentially voxels, ambient occlusion is actually easy to calculate. And since we already need to count all live neighbors for each cell we can actually get ambient occlusion for free!

Short-range ambient occlusion, or direct corner darkening, can be added with one sample of the 3D texture using trilinear filtering. This value will be 1/8 for completely exposed corners and 7/8 for completely enclosed corners (since trilinear filtering interpolates between 8 texture lookups). This means 1 - trilinear_sample will cause corners to become darker. I ended up using clamp(1.5 - trilinear_sample, 0.0, 1.0) as the final short-range ambient occlusion factor.

Long-range ambient occlusion darkens corners at larger scales. This will darken the ground under overhangs and the surfaces inside caves. One way to compute this is to blur the texture (in 3D) and use 1 - blurred_value to darken large-scale corners. However, instead of doing an expensive 3D blur every frame, we can instead compute the blur as a repeated series of small blurs over time. This way we get the blur for free since we are essentially doing a small blur by counting the neighbors. I added another channel to my 3D texture (now using the GL_RG format) to store this blurred value. The blurred value is updated using texture.g = mix(average.g, average.r, 0.01) and the final long-range ambient occlusion factor I used was clamp(1.5 - 3.0 * texture.g, 0.0, 1.0).
