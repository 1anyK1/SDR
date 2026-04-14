#include <iostream>
#include <vector>
#include <complex.h>
#include <numbers> 

std::vector<int> ted(std::vector<std::complex<double>> &matched, int samples_per_symbol)
{
    int K1, K2, p1, p2 = 0;
    double BnTs = 0.1;
    double Kp = 0.02;
    double zeta = sqrt(2) / 2;
    double theta = (BnTs / samples_per_symbol) / (zeta + (0.25 / zeta));
    K1 = -4 * zeta * theta / ( (1 + 2 * zeta * theta + pow(theta,2)) * Kp);
    K2 = -4 * pow(theta,2) / ( (1 + 2 * zeta * theta + pow(theta,2))* Kp);
    int tau = 0;
    double err;
    std::vector<int> errof;

    errof.push_back(0);
    for (int i = 0; i < matched.size() - samples_per_symbol; i += samples_per_symbol)
    {
        err = (matched[i + samples_per_symbol + tau].real() - matched[i + tau]).real() * matched[i + (samples_per_symbol / 2) + tau].real() + (matched[i + samples_per_symbol + tau].imag() - matched[i + tau]).imag() * matched[i + (samples_per_symbol / 2) + tau].imag(); 
        p1 = err * K1;
        p2 =  p2 + p1 + err * K2;

        if (p2 > 1)
        {
            p2 = p2 - 1;
        }

        if (p2 < -1)
        {
            p2 = p2 + 1;
        }
        tau = round(p2 * samples_per_symbol);
        errof.push_back(i + samples_per_symbol + tau);
    }
    return errof;
}

std::vector<std::complex<double>> symbol_sync(std::vector<std::complex<double>> &matched, int nsps)
{
    std::vector<int> ted_idxs = ted(matched, nsps);
    // for (int i = 0; i < ted_idxs.size(); i++){
    //     std::cout << ted_idxs[i] << " ";
    // }
    // std::cout << "" << std::endl;

    std::vector<std::complex<double>> symb_samples;
    symb_samples.resize(ted_idxs.size());
    for (int i = 0; i < ted_idxs.size(); i++)
    {
        symb_samples[i] = matched[ted_idxs[i]];
    }
    return symb_samples;
}