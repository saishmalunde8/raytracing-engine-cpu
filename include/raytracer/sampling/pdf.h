#ifndef PDF_H
#define PDF_H

#include "raytracer/core/rtweekend.h"
#include "raytracer/core/onb.h"
#include "raytracer/core/rng.h"

// A probability density over directions.
//
// Two operations, and the split between them matters:
//
//   generate()  draws a direction. It is sampling, so it consumes the caller's
//               RNG and must be handed one.
//   value()     reports how likely this density was to produce a direction it
//               is given. It is pure arithmetic on a direction that already
//               exists -- no randomness -- so it takes no RNG.
//
// Keeping value() RNG-free means it can be called on a direction that some
// other density produced. That is what makes mixing densities possible: ask one
// to generate, then ask both how likely that direction was.
class pdf {
  public:
    virtual ~pdf() {}

    virtual double value(const vec3& direction) const = 0;
    virtual vec3 generate(RNG& rng) const = 0;
};

// Uniform over the whole sphere. Every direction equally likely, so the density
// is constant: one over the sphere's total solid angle.
class sphere_pdf : public pdf {
  public:
    sphere_pdf() {}

    double value(const vec3& direction) const override {
        return 1 / (4 * pi);
    }

    vec3 generate(RNG& rng) const override {
        return rng.random_unit_vector();
    }
};

// Cosine-weighted over the hemisphere around a normal. Favours directions near
// the normal, which is where a diffuse surface sends most of its light, so
// samples get spent where they carry the most energy.
class cosine_pdf : public pdf {
  public:
    cosine_pdf(const vec3& w) : uvw(w) {}

    double value(const vec3& direction) const override {
        auto cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0, cosine_theta / pi);
    }

    vec3 generate(RNG& rng) const override {
        return uvw.transform(rng.random_cosine_direction());
    }

  private:
    onb uvw;
};

#endif
