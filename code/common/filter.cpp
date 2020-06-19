#include "filter.h"

#include <cassert>
#include <cmath>

static constexpr auto _LN2 = 0.69314718055994530942;
static constexpr auto _PI = 3.14159265358979323846;

namespace Filter {

void CalculateBiquad(const Params &b, Coeffs &c) {
    const auto sample_rate = b.sample_rate;
    const auto cutoff_freq = b.cutoff_freq;
    const auto Q = b.Q;
    const auto peak_gain = b.peak_gain;

    double a0, a1, a2, b0, b1, b2;

    const auto omega = 2 * _PI * cutoff_freq / sample_rate;
    const auto cs = std::cos(omega);
    const auto sn = std::sin(omega);
    const auto alpha = sn * std::sinh(_LN2 / 2 * Q * omega / sn);
    switch (b.type) {
        case BiquadType::LowPass: {
            const auto cs_a = 1 - cs;
            const auto cs_b = cs_a / 2;

            b0 = cs_b;
            b1 = cs_a;
            b2 = cs_b;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        }
        case BiquadType::HighPass: {
            const auto cs_a = 1 + cs;
            const auto cs_b = cs_a / 2;

            b0 = cs_b;
            b1 = -cs_a;
            b2 = cs_b;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        }
        case BiquadType::BandPass: {
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        }
        case BiquadType::Notch: {
            b0 = 1;
            b1 = -2 * cs;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * cs;
            a2 = 1 - alpha;
            break;
        }
        case BiquadType::Peak: {
            const auto A = std::pow(10.0, peak_gain / 40.0);
            const auto alpha_m_a = alpha * A;
            const auto alpha_d_a = alpha / A;

            b0 = 1 + alpha_m_a;
            b1 = -2 * cs;
            b2 = 1 - alpha_m_a;
            a0 = 1 + alpha_d_a;
            a1 = b1;
            a2 = 1 - alpha_d_a;
            break;
        }
        case BiquadType::LowShelf: {
            const auto A = std::pow(10.0, peak_gain / 40.0);
            const auto beta = std::sqrt(A + A);
            const auto ap1 = A + 1;
            const auto am1 = A - 1;
            const auto ap1_cs = ap1 * cs;
            const auto am1_cs = am1 * cs;
            const auto beta_sn = beta * sn;

            b0 = A * (ap1 - am1_cs + beta_sn);
            b1 = 2 * A * (am1 - ap1_cs);
            b2 = A * (ap1 - am1_cs - beta_sn);
            a0 = ap1 + am1_cs + beta_sn;
            a1 = -2 * (am1 + ap1_cs);
            a2 = ap1 + am1_cs - beta_sn;
            break;
        }
        case BiquadType::HighShelf: {
            const auto A = std::pow(10.0, peak_gain / 40.0);
            const auto beta = std::sqrt(A + A);
            const auto ap1 = A + 1;
            const auto am1 = A - 1;
            const auto ap1_cs = ap1 * cs;
            const auto am1_cs = am1 * cs;
            const auto beta_sn = beta * sn;

            b0 = A * (ap1 + am1_cs + beta_sn);
            b1 = -2 * A * (am1 + ap1_cs);
            b2 = A * (ap1 + am1_cs - beta_sn);
            a0 = ap1 - am1_cs + beta_sn;
            a1 = 2 * (am1 - ap1_cs);
            a2 = ap1 - am1_cs - beta_sn;
            break;
        }
        default:
            b0 = b1 = b2 = a0 = a1 = a2 = 0;
            assert(0);
            break;
    }

    // use a0 to normalise
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
}

void CalculateRBJ(const Params &p, Coeffs &c) {
    const auto type = static_cast<RBJType>(p.type);
    const double frequency = p.cutoff_freq;
    const double sample_rate = p.sample_rate;
    const double q = p.Q;
    const double db_gain = p.peak_gain;
    const bool q_is_bandwidth = p.q_is_bandwidth;

    double alpha {}, a0 {}, a1 {}, a2 {}, b0 {}, b1 {}, b2 {};

    if (type == RBJType::Peaking || type == RBJType::LowShelf || type == RBJType::HighShelf) {
        const double A = std::pow(10.0, (db_gain / 40.0));
        const double omega = 2.0 * _PI * frequency / sample_rate;
        const double tsin = std::sin(omega);
        const double tcos = std::cos(omega);

        if (q_is_bandwidth) {
            alpha = tsin * std::sinh(std::log(2.0) / 2.0 * q * omega / tsin);
        } else {
            alpha = tsin / (2.0 * q);
        }

        const double beta = std::sqrt(A) / q;

        switch (type) {
            case RBJType::Peaking: {
                b0 = 1.0 + alpha * A;
                b1 = -2.0 * tcos;
                b2 = 1.0 - alpha * A;
                a0 = 1.0 + alpha / A;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha / A;
                break;
            }
            case RBJType::LowShelf: {
                b0 = A * ((A + 1.0) - (A - 1.0) * tcos + beta * tsin);
                b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * tcos);
                b2 = A * ((A + 1.0) - (A - 1.0) * tcos - beta * tsin);
                a0 = (A + 1.0) + (A - 1.0) * tcos + beta * tsin;
                a1 = -2.0 * ((A - 1.0) + (A + 1.0) * tcos);
                a2 = (A + 1.0) + (A - 1.0) * tcos - beta * tsin;
                break;
            }
            case RBJType::HighShelf: {
                b0 = A * ((A + 1.0) + (A - 1.0) * tcos + beta * tsin);
                b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * tcos);
                b2 = A * ((A + 1.0) + (A - 1.0) * tcos - beta * tsin);
                a0 = (A + 1.0) - (A - 1.0) * tcos + beta * tsin;
                a1 = 2.0 * ((A - 1.0) - (A + 1.0) * tcos);
                a2 = (A + 1.0) - (A - 1.0) * tcos - beta * tsin;
                break;
            }
        }
    } else {
        const double omega = 2.0 * _PI * frequency / sample_rate;
        const double tsin = std::sin(omega);
        const double tcos = std::cos(omega);

        if (q_is_bandwidth) {
            alpha = tsin * std::sinh(std::log(2.0) / 2.0 * q * omega / tsin);
        } else {
            alpha = tsin / (2.0 * q);
        }

        switch (type) {
            case RBJType::LowPass: {
                b0 = (1.0 - tcos) / 2.0;
                b1 = 1.0 - tcos;
                b2 = (1.0 - tcos) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
            case RBJType::HighPass: {
                b0 = (1.0 + tcos) / 2.0;
                b1 = -(1.0 + tcos);
                b2 = (1.0 + tcos) / 2.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
            case RBJType::BandPassCSG: {
                b0 = tsin / 2.0;
                b1 = 0.0;
                b2 = -tsin / 2;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
            case RBJType::BandPassCZPG: {
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
            case RBJType::Notch: {
                b0 = 1.0;
                b1 = -2.0 * tcos;
                b2 = 1.0;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
            case RBJType::AllPass: {
                b0 = 1.0 - alpha;
                b1 = -2.0 * tcos;
                b2 = 1.0 + alpha;
                a0 = 1.0 + alpha;
                a1 = -2.0 * tcos;
                a2 = 1.0 - alpha;
                break;
            }
        }
    }

    if (a0 == 0) {
        a0 = 1;
    }
    // use a0 to normalise
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
}

void SetParamsAndCoeffs(Type filter_type,
                        Params &b,
                        Coeffs &c,
                        int type,
                        double sample_rate,
                        double cutoff_freq,
                        double Q,
                        double gain_db) {
    const auto nyquist = sample_rate / 2;
    b.type = type;
    b.sample_rate = sample_rate;
    b.cutoff_freq = std::min(nyquist, cutoff_freq);
    b.Q = Q;
    b.peak_gain = gain_db;

    switch (filter_type) {
        case Type::Biquad: {
            CalculateBiquad(b, c);
            break;
        }
        case Type::RBJ: {
            CalculateRBJ(b, c);
            break;
        }
        default: assert(0);
    }
}

double Process(Data &d, const Coeffs &c, const double in) {
    const auto out = c.b0 * in + c.b1 * d.in1 + c.b2 * d.in2 - c.a1 * d.out1 - c.a2 * d.out2;

    d.in2 = d.in1;
    d.in1 = in;
    d.out2 = d.out1;
    d.out1 = out;

    return out;
}

} // namespace Filter
