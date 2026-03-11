#include <chrono>
#include <iostream>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

// =========================
// Object-Oriented (AoS)
// =========================
struct Particle_OOP {
    float x, y, z;       // position
    float vx, vy, vz;    // velocity
    float ax, ay, az;    // acceleration
};

// Integrate velocity from acceleration (OOP)
void system_integrate_velocity_oop(std::vector<Particle_OOP>& particles, float dt) {
    for (auto& p : particles) {
        p.vx += p.ax * dt;
        p.vy += p.ay * dt;
        p.vz += p.az * dt;
    }
}

// Integrate position from velocity (OOP)
void system_integrate_position_oop(std::vector<Particle_OOP>& particles, float dt) {
    for (auto& p : particles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
    }
}

// "Render" system that only reads position (OOP)
float system_render_oop(const std::vector<Particle_OOP>& particles) {
    float sum = 0.0f;
    for (const auto& p : particles) {
        sum += p.x * 0.1f + p.y * 0.2f + p.z * 0.3f;
    }
    return sum;
}

// =========================
// Data-Oriented (SoA)
// =========================
struct ParticleSystem_DOP {
    std::vector<float> x, y, z;      // positions
    std::vector<float> vx, vy, vz;   // velocities
    std::vector<float> ax, ay, az;   // accelerations

    explicit ParticleSystem_DOP(std::size_t count)
        : x(count), y(count), z(count),
          vx(count), vy(count), vz(count),
          ax(count), ay(count), az(count) {}
};

void system_integrate_velocity_dop(ParticleSystem_DOP& s, float dt) {
    const std::size_t n = s.x.size();
    for (std::size_t i = 0; i < n; ++i) {
        s.vx[i] += s.ax[i] * dt;
        s.vy[i] += s.ay[i] * dt;
        s.vz[i] += s.az[i] * dt;
    }
}

void system_integrate_position_dop(ParticleSystem_DOP& s, float dt) {
    const std::size_t n = s.x.size();
    for (std::size_t i = 0; i < n; ++i) {
        s.x[i] += s.vx[i] * dt;
        s.y[i] += s.vy[i] * dt;
        s.z[i] += s.vz[i] * dt;
    }
}

float system_render_dop(const ParticleSystem_DOP& s) {
    const std::size_t n = s.x.size();
    float sum = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        // Only touch position data, not velocities/accelerations
        sum += s.x[i] * 0.1f + s.y[i] * 0.2f + s.z[i] * 0.3f;
    }
    return sum;
}

// =========================
// Benchmark helpers
// =========================
template <typename F>
auto time_ms(F&& f) {
    auto start = Clock::now();
    f();
    auto dur = Clock::now() - start;
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
}

int main() {
    const std::size_t PARTICLE_COUNT = 200'000;
    const int PHYSICS_STEPS = 200;
    const int RENDER_STEPS  = 5'000;
    const float dt = 0.016f;

    // ============
    // Init OOP
    // ============
    std::vector<Particle_OOP> oop(PARTICLE_COUNT);
    for (std::size_t i = 0; i < PARTICLE_COUNT; ++i) {
        oop[i].x = static_cast<float>(i) * 0.001f;
        oop[i].y = static_cast<float>(i) * 0.002f;
        oop[i].z = static_cast<float>(i) * 0.003f;
        oop[i].vx = 1.0f;
        oop[i].vy = 2.0f;
        oop[i].vz = 3.0f;
        oop[i].ax = 0.1f;
        oop[i].ay = 0.2f;
        oop[i].az = 0.3f;
    }

    // ============
    // Init DOP
    // ============
    ParticleSystem_DOP dop(PARTICLE_COUNT);
    for (std::size_t i = 0; i < PARTICLE_COUNT; ++i) {
        dop.x[i] = static_cast<float>(i) * 0.001f;
        dop.y[i] = static_cast<float>(i) * 0.002f;
        dop.z[i] = static_cast<float>(i) * 0.003f;
        dop.vx[i] = 1.0f;
        dop.vy[i] = 2.0f;
        dop.vz[i] = 3.0f;
        dop.ax[i] = 0.1f;
        dop.ay[i] = 0.2f;
        dop.az[i] = 0.3f;
    }

    // ============
    // OOP benchmark
    // ============
    float oop_render_sum = 0.0f;
    auto oop_ms = time_ms([&] {
        // Physics: position + velocity + acceleration
        for (int i = 0; i < PHYSICS_STEPS; ++i) {
            system_integrate_velocity_oop(oop, dt);
            system_integrate_position_oop(oop, dt);
        }

        // Render-heavy workload: only needs positions
        for (int i = 0; i < RENDER_STEPS; ++i) {
            oop_render_sum += system_render_oop(oop);
        }
    });

    // ============
    // DOP benchmark
    // ============
    float dop_render_sum = 0.0f;
    auto dop_ms = time_ms([&] {
        // Physics: position + velocity + acceleration
        for (int i = 0; i < PHYSICS_STEPS; ++i) {
            system_integrate_velocity_dop(dop, dt);
            system_integrate_position_dop(dop, dt);
        }

        // Render-heavy workload: only needs positions
        for (int i = 0; i < RENDER_STEPS; ++i) {
            dop_render_sum += system_render_dop(dop);
        }
    });

    // Prevent dead-code elimination
    std::cout << "Check sums: OOP=" << oop_render_sum
              << " DOP=" << dop_render_sum << "\n";

    std::cout << "OOP time: " << oop_ms << " ms\n";
    std::cout << "DOP time: " << dop_ms << " ms\n";
    std::cout << "DOP is " << static_cast<double>(oop_ms) / dop_ms << "x faster\n";

    return 0;
}
