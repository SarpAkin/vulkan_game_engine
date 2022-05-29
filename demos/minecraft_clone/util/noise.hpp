#pragma once

#include <PerlinNoise.hpp>

class AmplifiedNoise
{
public:
    AmplifiedNoise(double freq, double amp, uint64_t seed)
        : m_freq(freq),m_amp(amp),m_p_noise(seed)
    {

    }

    double noise(double x,double y)const
    {
        return m_p_noise.noise2D(x * m_freq, y * m_freq) * m_amp;
    }

    double noise(double x, double y,double z)const
    {
        return m_p_noise.noise3D(x * m_freq, y * m_freq,z * m_freq) * m_amp;
    }

private:
    double m_freq;
    double m_amp;

    siv::PerlinNoise m_p_noise;
};