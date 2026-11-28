// Version 1 base record for a GPU Gaussian page. Higher SH coefficients and
// verified source ordinals belong to separate page streams.
#pragma once

#include <Core/Core.h>
#include <array>

namespace Upp {

struct Scene3DGaussianParameters {
    // Source centre already transformed by the asset's raw-to-local axis scales.
    // Rotation and log-scale remain in the raw source frame. Apply those same
    // axis scales to the reconstructed covariance before instance placement.
    std::array<double, 3> center{}, log_scale{}, dc{};
    std::array<double, 4> rotation_wxyz{1, 0, 0, 0};
    double opacity = 0; // Activated opacity in [0, 1].
};

// Eight 32-bit words, 32-byte stride, 4-byte alignment. Serialize each word
// little endian. No native 16-bit GPU storage or arithmetic is required.
// Word 0–2: float32 centre. Words 3–4 (low): three 15-bit quaternion components,
// a 2-bit missing-component index and one reserved bit.
// Words 4 (high)–5: unsigned 16-bit log-scales spanning [-100, 20].
// Word 6 (low): UNORM16 opacity. Words 6 (high)–7: raw 3DGS DC coefficients in
// IEEE binary16, before the SH basis, clipping or colour conversion.
struct Scene3DGaussianRecord {
    static constexpr int Version = 1;
    std::array<uint32, 8> words{};
};
static_assert(sizeof(Scene3DGaussianRecord) == 32 && alignof(Scene3DGaussianRecord) == 4);

// Failed operations leave the output unchanged. Finite DC outside binary16
// range requires a wide representation or the compatibility preview.
bool EncodeScene3DGaussian(const Scene3DGaussianParameters&, Scene3DGaussianRecord&);
bool DecodeScene3DGaussian(const Scene3DGaussianRecord&, Scene3DGaussianParameters&);

} // namespace Upp
