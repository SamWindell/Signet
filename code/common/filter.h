#pragma once

#include <algorithm>

namespace Filter {

struct Data {
    double out1 = 0, out2 = 0, in1 = 0, in2 = 0;
};

struct Coeffs {
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

static constexpr double default_q_factor = 0.70710678118; // sqrt(2)/2

struct Params {
    int type = 0;
    double sample_rate = 44100;
    double cutoff_freq = 10000;
    double Q = default_q_factor;
    double peak_gain = 0;
    bool q_is_bandwidth = false;
};

enum class Type {
    Biquad,
    RBJ,
};

enum class BiquadType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LowShelf,
    HighShelf,
};

enum class RBJType {
    LowPass,
    HighPass,
    BandPassCSG,
    BandPassCZPG,
    Notch,
    AllPass,
    Peaking,
    LowShelf,
    HighShelf,
};

void SetParamsAndCoeffs(Type filter_type,
                        Params &b,
                        Coeffs &c,
                        int type,
                        double sample_rate,
                        double cutoff_freq,
                        double Q,
                        double gain_db);

double Process(Data &d, const Coeffs &c, const double in);

} // namespace Filter
