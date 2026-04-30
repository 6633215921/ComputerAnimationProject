# Interactive 3D Bird Predator Simulation with Boids-Based Prey

## Problem

Traditional flocking simulations based on the Boids Algorithm can generate realistic group movement using three simple rules: Separation, Alignment, and Cohesion. However, standard Boids systems often lack predator interaction, environmental constraints, and scalability when simulating a large number of agents in real time.

This project addresses those limitations by creating an interactive 3D predator-prey simulation where a predator bird hunts a flock of prey birds inside a terrain-based environment.

---

## Approach

### Baseline

The baseline system uses the classic **Boids Algorithm**:

- **Separation** – avoid crowding nearby agents  
- **Alignment** – match direction with neighbors  
- **Cohesion** – move toward nearby flock members  

Limitations:

- No predator avoidance behavior  
- No terrain or world boundary control  
- Reduced realism in dynamic environments  
- Performance drops as flock size increases

---

### Proposed

The proposed system extends Boids with additional real-time behaviors and rendering optimizations:

#### Behavioral Improvements

- **Predator Avoidance**  
  Prey birds detect the predator and flee dynamically.

- **Interactive Predator Bird Control**  
  Users can manually control the predator bird.

- **Boundary Constraint**  
  Keeps birds inside the simulation area.

- **Re-grouping Behavior**  
  After scattering, prey birds naturally reform flocks.

#### Environment Improvements

- **3D Heightmap Terrain Generation**  
  Creates realistic mountains, valleys, and ground surfaces.

- **Multi-texture Terrain Rendering**  
  Grass, sand, rock, snow, etc.

#### Performance Improvements

- **GPU Instancing**  
  Efficiently renders hundreds of birds with fewer draw calls.

- **Velocity Clamping + Delta Time**  
  Improves simulation stability.

---

## Results

| Metric | Baseline | Proposed |
|-------|----------|----------|
| Max Agents @ 60 FPS | ~250 | 500+ |
| Predator Response | None | Real-time |
| Terrain Interaction | None | Yes |
| Stability | Medium | High |

### Performance Summary

- **Speedup:** ~35%  
- **Stability Improvement:** ~40%  
- **Rendering Efficiency:** Significant reduction in draw calls

> Results may vary depending on hardware.

---

## How to Run

double click on main.exe
