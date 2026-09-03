// Validation for the direction samplers in core/. Standalone -- not part of the
// renderer build, and it links nothing from src/.
//
//   clang++ -std=c++17 -O2 -Iinclude tools/sampling_check.cpp -o build/sampling_check
//   ./build/sampling_check
//
// A direction sampler that is subtly wrong still produces a plausible-looking
// image, just one that is quietly lit incorrectly, and tracking that back from a
// finished render is painful. Each check below compares against a value that can
// be worked out on paper, so the sampler is measured rather than eyeballed.

#include "raytracer/core/rtweekend.h"
#include "raytracer/core/onb.h"
#include "raytracer/core/rng.h"

#include <iomanip>
#include <string>

static int failures = 0;

static void check(const std::string& name, double got, double expected, double tol) {
    bool ok = std::fabs(got - expected) <= tol;
    if (!ok) failures++;
    std::cout << (ok ? "  pass  " : "  FAIL  ")
              << std::left << std::setw(46) << name
              << "got " << std::fixed << std::setprecision(6) << got
              << "   expected " << expected
              << "   tol " << tol << "\n";
}

static void check_true(const std::string& name, bool ok) {
    if (!ok) failures++;
    std::cout << (ok ? "  pass  " : "  FAIL  ") << name << "\n";
}

int main() {
    std::cout << "Direction sampler validation\n";
    std::cout << "----------------------------------------------------------------\n";

    const int N = 1000000;

    // 1. Cosine-weighted sampling, checked by integrating a function whose value
    //    over the hemisphere is known exactly.
    //
    //      integral of cos^3(theta) dw  over the hemisphere  =  pi/2
    //
    //    Sampling with density p = cos(theta)/pi, each sample contributes
    //    f/p = pi*cos^2(theta), so the mean of that must land on pi/2.
    //    A sampler with the wrong exponent or a missing square root misses this.
    {
        RNG rng(12345u);
        double sum = 0.0;
        for (int i = 0; i < N; i++) {
            double cos_theta = rng.random_cosine_direction().z();
            sum += pi * cos_theta * cos_theta;
        }
        check("cosine density integrates cos^3 to pi/2", sum / N, pi / 2, 0.005);
    }

    // 2. Every generated direction must be a unit vector in the +Z hemisphere.
    {
        RNG rng(999u);
        bool hemisphere = true, unit_length = true;
        for (int i = 0; i < N; i++) {
            vec3 d = rng.random_cosine_direction();
            if (d.z() < 0.0) hemisphere = false;
            if (std::fabs(d.length() - 1.0) > 1e-9) unit_length = false;
        }
        check_true("all cosine directions lie in the +Z hemisphere", hemisphere);
        check_true("all cosine directions are unit length", unit_length);
    }

    // 3. The basis must be orthonormal for any normal it is handed, including
    //    normals aligned with an axis, where a careless seed vector for the
    //    cross product collapses to zero.
    {
        RNG rng(7u);
        bool orthonormal = true, w_matches_normal = true;

        vec3 normals[] = {
            vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0),
            vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1),
            unit_vector(vec3(0.9, 0.1, 0.05))   // close to the x axis
        };

        auto verify = [&](const vec3& n) {
            onb basis(n);
            if (std::fabs(basis.u().length() - 1) > 1e-9) orthonormal = false;
            if (std::fabs(basis.v().length() - 1) > 1e-9) orthonormal = false;
            if (std::fabs(basis.w().length() - 1) > 1e-9) orthonormal = false;
            if (std::fabs(dot(basis.u(), basis.v())) > 1e-9) orthonormal = false;
            if (std::fabs(dot(basis.u(), basis.w())) > 1e-9) orthonormal = false;
            if (std::fabs(dot(basis.v(), basis.w())) > 1e-9) orthonormal = false;
            if (std::fabs(dot(basis.w(), unit_vector(n)) - 1) > 1e-9) w_matches_normal = false;
        };

        for (const auto& n : normals) verify(n);
        for (int i = 0; i < 10000; i++) verify(rng.random_unit_vector());

        check_true("basis is orthonormal for axis-aligned and random normals", orthonormal);
        check_true("basis w() points along the normal it was built from", w_matches_normal);
    }

    // 4. A cosine direction rotated onto a surface must end up on the outward
    //    side of that surface. If transform() were wrong, light would scatter
    //    into the geometry and surfaces would render far too dark.
    {
        RNG rng(2024u);
        bool correct_side = true;
        for (int i = 0; i < 100000; i++) {
            vec3 n = rng.random_unit_vector();
            onb basis(n);
            vec3 scattered = basis.transform(rng.random_cosine_direction());
            if (dot(scattered, n) < -1e-9) correct_side = false;
        }
        check_true("transformed directions stay on the normal's side", correct_side);
    }

    std::cout << "----------------------------------------------------------------\n";
    if (failures == 0) {
        std::cout << "ALL CHECKS PASSED\n";
        return 0;
    }
    std::cout << failures << " CHECK(S) FAILED\n";
    return 1;
}
