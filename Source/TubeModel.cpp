#include "TubeModel.h"
#include "TubeCoefficients.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace glitch
{
namespace
{
template<size_t N> double chebyshev(const std::array<double,N>& c,double x) noexcept
{
    double b1=0,b2=0;
    for(size_t i=N-1;i>0;--i) { const double b=2*x*b1-b2+c[i];b2=b1;b1=b; }
    return x*b1-b2+c[0];
}
}
double TubeModel::outputGain(int units) noexcept
{
    const double p=std::clamp(units,0,1000000)/1000000.0;
    return 16*p*p*p;
}
void TubeModel::prepare(double rate) noexcept
{
    denominator=tubeCalibration::denominator;rateGain=1;
    if(rate>0 && rate!=48000)
    {
        // Bilinear mapping of the measured pole locations. This preserves the
        // inferred analog frequencies; it is not yet cross-rate parity evidence.
        constexpr double radius=0.99626977,angle=0.00372212,lowPole=-0.74847556;
        const auto convert=[rate](std::complex<double> z)
        {
            const auto s=96000.0*(z-1.0)/(z+1.0);
            return (2*rate+s)/(2*rate-s);
        };
        const auto hp=convert(std::polar(radius,angle));
        const double lp=convert(lowPole).real(),a1=-2*hp.real(),a2=std::norm(hp);
        denominator={1,a1-lp,a2-a1*lp,-a2*lp};
        const double oldNormal=(1+2*radius*std::cos(angle)+radius*radius)*(1-lowPole);
        rateGain=(1-a1+a2)*(1-lp)/oldNormal;
    }
    reset();
}
void TubeModel::reset() noexcept { states={}; }
void TubeModel::setParameters(int driveUnits,int outputUnits) noexcept
{
    const double d=std::clamp(driveUnits,0,1000000)/1000000.0;
    negativeDry=std::pow(std::clamp(1-d/0.75,0.0,1.0),2);
    positiveDry=std::pow(std::clamp(1-(d-0.25)/0.75,0.0,1.0),2);
    for(size_t i=0;i<13;++i)
    {
        negative[i]=chebyshev(tubeCalibration::negative[i],2*d-1);
        positive[i]=chebyshev(tubeCalibration::positive[i],2*d-1);
    }
    gain=outputGain(outputUnits)/outputGain(338989);
}
double TubeModel::process(double input,int channel) noexcept
{
    const bool neg=input<0;
    const double u=std::min(std::abs(input),1.0),dry=neg ? negativeDry : positiveDry;
    double wet=u>=1 ? 1 : std::clamp((neg ? u*u*u : u)*chebyshev(neg ? negative : positive,2*u-1),0.0,1.0);
    if(neg) wet=-wet;
    const double x=tubeCalibration::numeratorGain*(dry*input+(1-dry)*wet);
    auto& s=states[(size_t)channel];
    const double y=x-s.input[0]-s.input[1]+s.input[2]
                  -denominator[1]*s.output[0]-denominator[2]*s.output[1]-denominator[3]*s.output[2];
    s.input={x,s.input[0],s.input[1]};s.output={y,s.output[0],s.output[1]};
    return y*gain*rateGain;
}
}
