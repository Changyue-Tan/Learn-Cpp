#include <chrono>
#include <iostream>
#include <vector>

// =========================
// Object-Oriented Particle
// =========================
class Particle {
public:
    float x, y, z;     // position
    float vx, vy, vz;  // velocity

    // Update position based on velocity
    void update(float dt) {
        x += vx * dt;
        y += vy * dt;
        z += vz * dt;
    }
};

// =========================
// Data-Oriented Particle System
// =========================
struct ParticleSystem {
    std::vector<float> x, y, z;     // positions
    std::vector<float> vx, vy, vz;  // velocities

    // Update all particles
    void update(float dt) {
        size_t count = x.size();
        for (size_t i = 0; i < count; ++i) {
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            z[i] += vz[i] * dt;
        }
    }

    // Resize all vectors to hold 'count' particles
    void resize(size_t count) {
        x.resize(count);
        y.resize(count);
        z.resize(count);
        vx.resize(count);
        vy.resize(count);
        vz.resize(count);
    }
};

int main() {
    const size_t PARTICLE_COUNT = 1'000'000; // number of particles
    const int ITERATIONS = 10'000;           // simulation steps
    const float dt = 0.016f;                 // time step (~60 FPS)

    // =========================
    // OOP Benchmark
    // =========================
    std::vector<Particle> particles(PARTICLE_COUNT); // allocate particles

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto &p : particles) {
            p.update(dt);  // update each particle
        }
    }

    auto oop_duration = std::chrono::high_resolution_clock::now() - start;

    // =========================
    // DOP Benchmark
    // =========================
    ParticleSystem system;
    system.resize(PARTICLE_COUNT); // allocate all particle arrays

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        system.update(dt); // update all particles
    }

    auto dop_duration = std::chrono::high_resolution_clock::now() - start;

    // =========================
    // Print results
    // =========================
    auto oop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(oop_duration).count();
    auto dop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dop_duration).count();

    std::cout << "OOP time: " << oop_ms << " ms\n";
    std::cout << "DOP time: " << dop_ms << " ms\n";
    std::cout << "DOP is " << static_cast<double>(oop_ms) / dop_ms << "x faster\n";

    return 0;
}
