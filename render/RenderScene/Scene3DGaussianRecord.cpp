#include "Scene3DGaussianRecord.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Upp {
namespace {

constexpr double LogMin = -100, LogMax = 20;
constexpr double QuaternionRange = 0.7071067811865475244;
constexpr int QuaternionBias = 16383;
constexpr uint32 QuaternionMask = 0x7fff;
constexpr uint32 HalfMask = 0xffff;
static_assert(sizeof(float) == sizeof(uint32) && std::numeric_limits<float>::is_iec559);

// Float32 intermediate with round-to-nearest-even binary16 conversion.
uint32 EncodeHalf(float value)
{
    const uint32 bits = std::bit_cast<uint32>(value);
    const uint32 sign = (bits >> 16) & 0x8000;
    const uint32 mantissa = bits & 0x7fffff;
    const int exponent = int((bits >> 23) & 255) - 127;
    if(exponent < -25) return sign;
    if(exponent < -14) {
        const uint32 significand = 0x800000 | mantissa;
        const int shift = -exponent - 1;
        const uint32 round = ((uint32(1) << (shift - 1)) - 1) + ((significand >> shift) & 1);
        return sign | ((significand + round) >> shift);
    }
    const uint32 rounded = (mantissa + 0xfff + ((mantissa >> 13) & 1)) >> 13;
    return sign | ((uint32(exponent + 15) << 10) + rounded);
}

bool DecodeHalf(uint32 bits, double& value)
{
    const uint32 exponent = (bits >> 10) & 31;
    const uint32 mantissa = bits & 1023;
    if(exponent == 31) return false;
    value = exponent ? std::ldexp(double(1024 + mantissa), int(exponent) - 25)
                     : std::ldexp(double(mantissa), -24);
    if(bits & 0x8000) value = -value;
    return true;
}

} // namespace

bool EncodeScene3DGaussian(const Scene3DGaussianParameters& input, Scene3DGaussianRecord& out)
{
    Scene3DGaussianRecord result;
    auto& word = result.words;
    uint32 scale[3], dc[3];
    for(int i = 0; i < 3; ++i) {
        if(!std::isfinite(input.center[i]) ||
           std::abs(input.center[i]) > std::numeric_limits<float>::max() ||
           !std::isfinite(input.log_scale[i]) || input.log_scale[i] < LogMin ||
           input.log_scale[i] > LogMax || !std::isfinite(input.dc[i]) ||
           std::abs(input.dc[i]) > 65504.)
            return false;
        word[i] = std::bit_cast<uint32>(float(input.center[i]));
        scale[i] = uint32(std::lround((input.log_scale[i] - LogMin) * HalfMask / (LogMax - LogMin)));
        dc[i] = EncodeHalf(float(input.dc[i]));
    }
    if(!std::isfinite(input.opacity) || input.opacity < 0 || input.opacity > 1) return false;
    const auto& rotation = input.rotation_wxyz;
    int largest = 0;
    for(int i = 0; i < 4; ++i) {
        if(!std::isfinite(rotation[i])) return false;
        if(std::abs(rotation[i]) > std::abs(rotation[largest])) largest = i;
    }
    const double norm = std::hypot(std::hypot(rotation[0], rotation[1]),
                                   std::hypot(rotation[2], rotation[3]));
    if(!std::isfinite(norm) || norm < 1e-9) return false;
    const double sign = rotation[largest] < 0 ? -1. : 1.;
    uint32 q[3];
    int at = 0;
    for(int i = 0; i < 4; ++i)
        if(i != largest) {
            const double component = sign * (rotation[i] / norm);
            const double fraction = std::clamp(component / QuaternionRange, -1., 1.);
            q[at++] = uint32(std::lround(fraction * QuaternionBias) + QuaternionBias);
        }
    word[3] = q[0] | (q[1] << 15) | ((q[2] & 3) << 30);
    word[4] = (q[2] >> 2) | (uint32(largest) << 13) | (scale[0] << 16);
    word[5] = scale[1] | (scale[2] << 16);
    word[6] = uint32(std::lround(input.opacity * HalfMask)) | (dc[0] << 16);
    word[7] = dc[1] | (dc[2] << 16);
    out = result;
    return true;
}

bool DecodeScene3DGaussian(const Scene3DGaussianRecord& input, Scene3DGaussianParameters& out)
{
    const auto& word = input.words;
    if(word[4] & 0x8000) return false;
    Scene3DGaussianParameters result;
    const uint32 scale[] = {word[4] >> 16, word[5] & HalfMask, word[5] >> 16};
    const uint32 dc[] = {word[6] >> 16, word[7] & HalfMask, word[7] >> 16};
    for(int i = 0; i < 3; ++i) {
        result.center[i] = std::bit_cast<float>(word[i]);
        if(!std::isfinite(result.center[i]) || !DecodeHalf(dc[i], result.dc[i])) return false;
        result.log_scale[i] = LogMin + scale[i] * (LogMax - LogMin) / HalfMask;
    }
    result.opacity = double(word[6] & HalfMask) / HalfMask;
    const int largest = int((word[4] >> 13) & 3);
    const uint32 q[] = {word[3] & QuaternionMask, (word[3] >> 15) & QuaternionMask,
                       ((word[3] >> 30) & 3) | ((word[4] & 0x1fff) << 2)};
    int at = 0;
    double sum = 0;
    for(int i = 0; i < 4; ++i)
        if(i != largest) {
            if(q[at] == QuaternionMask) return false;
            const double component = (int(q[at++]) - QuaternionBias) * (QuaternionRange / QuaternionBias);
            result.rotation_wxyz[i] = component;
            sum += component * component;
        }
    if(sum > 1.) return false;
    result.rotation_wxyz[largest] = std::sqrt(1. - sum);
    out = result;
    return true;
}

} // namespace Upp
