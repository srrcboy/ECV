// Copyright 2011 Ethan Eade. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
//    1. Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY ETHAN EADE ``AS IS'' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ETHAN EADE OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
// 
// The views and conclusions contained in the software and
// documentation are those of the authors and should not be
// interpreted as representing official policies, either expressed or
// implied, of Ethan Eade.
#include <ecv/scale_space.hpp>
#include <ecv/convolution.hpp>
#include <ecv/gaussian_yvv.hpp>
#include <ecv/subsample.hpp>
#include <ecv/resample.hpp>
#include <latl/ldlt.hpp>
#include <cmath>
#include <cassert>
#include <functional>
#include <iostream>

using namespace ecv;
using namespace std;
using namespace scale_space;

struct ecv::scale_space::PyramidBuilder::State
{
    double k;
    YvV_Params yvv0, yvv;
};

ecv::scale_space::PyramidBuilder::PyramidBuilder()
{
    state = new State();
}

ecv::scale_space::PyramidBuilder::~PyramidBuilder()
{
    delete state;
}
            
void ecv::scale_space::PyramidBuilder::init(double scale_factor,
                                            double preblur_sigma,
                                            double assumed_blur_sigma)
{
    state->k = scale_factor;
    state->yvv0.init(preblur_sigma);
    state->yvv.init(sqrt(state->k*state->k - 1) * assumed_blur_sigma);
}

template <class T, class D>
static
void subtract(const Image<T>& a, const Image<T>& b,
              Image<D>& a_minus_b)
{
    assert(a.size() == b.size());
    a_minus_b.resize(a.size());

    for (int i=0; i<a.height(); ++i) {
        const T *pa = a[i];
        const T *pb = b[i];
        D *o = a_minus_b[i];
        for (int j=0; j<a.width(); ++j) {
            o[j] = (D)(pa[j] - pb[j]);
        }
    }    
}

void ecv::scale_space::PyramidBuilder::compute(const Image<float>& im,
                                               Pyramid<float>& pyr,
                                               int min_dim)
{
    int levels = 1;
    int s = im.width() < im.height() ? im.width () : im.height();
    double f = 1/state->k;
    s = (int)(s*f);
    while (s >= min_dim) {
        ++levels;
        s = (int)(s*f);
    }
    compute(im, levels, pyr);
}

void ecv::scale_space::PyramidBuilder::compute(const Image<float>& im,
                                               int levels,
                                               Pyramid<float>& pyr)
{
    assert(levels > 0);
    
    pyr.scale_factor = 4.0/3;
    pyr.level.resize(levels);
    pyr.diff.resize(levels-1);
        
    pyr.level[0] = im.copy();
    yvv_blur(pyr.level[0], state->yvv0);

    for (int i=1; i<levels; ++i) {
        Image<float> smooth = pyr.level[i-1].copy();
        yvv_blur(smooth, state->yvv);
        subtract(pyr.level[i-1], smooth, pyr.diff[i-1]);
        subsample_three_fourths(smooth, pyr.level[i]);
    }
}

struct ecv::scale_space::PyramidBuilder_8bit::State
{
    double dscale;
    double inv_dscale;
    double sigma;

    BicubicScaler scaler;

    double sigma0;
    int taps0;
    int kernel0[4];

    double sigma1;
    int taps1;
    int kernel1[4];

    void init(int max_dim, double scale_factor, double s0, double as0)
    {
        dscale = scale_factor;
        inv_dscale = 1.0 / dscale;
        scaler.init(max_dim, inv_dscale);
        sigma = s0;

        sigma0 = sqrt(s0*s0 - as0*as0);    
        sigma1 = sigma*sqrt(dscale*dscale - 1.0);

        taps0 = sigma0 == 0.0 ? 0
            : (sigma0 < 0.7 ? 3
               : (sigma0 < 1.05 ? 5
                  : 7));
        
        taps1 = sigma1 == 0.0 ? 0
            : (sigma1 < 0.7 ? 3
               : (sigma1 < 1.05 ? 5
                  : 7));

        if (taps0)
            compute_gaussian_kernel(sigma0, taps0/2+1, 1<<12, kernel0, GaussianKernel::SAMPLED_PDF);
        
        if (taps1)
            compute_gaussian_kernel(sigma1, taps1/2+1, 1<<12, kernel1, GaussianKernel::SAMPLED_PDF);
    }
};

ecv::scale_space::PyramidBuilder_8bit::PyramidBuilder_8bit()
{
    state = new State();
}

ecv::scale_space::PyramidBuilder_8bit::~PyramidBuilder_8bit()
{
    delete state;
}

void ecv::scale_space::PyramidBuilder_8bit::init(int max_dim,
                                                 double scale_factor,
                                                 double level0_sigma,
                                                 double assumed_sigma)
{
    state->init(max_dim, scale_factor, level0_sigma, assumed_sigma);
}

void ecv::scale_space::PyramidBuilder_8bit::compute(const Image<uint8_t> &im,
                                                    std::vector<Image<uint8_t> > &pyr,
                                                    int min_dim) const
{
    int num_levels = 0;
    int s = im.width() < im.height() ? im.width () : im.height();
    while (s >= min_dim) {
        ++num_levels;
        s = (int)(s*state->inv_dscale);
    }

    pyr.resize(num_levels);
    if (num_levels == 0)
        return;

    switch (state->taps0) {
    case 0: pyr[0] = im.copy(); break;
    case 3: convolve_symmetric_3tap_12bit(im, state->kernel0, pyr[0]); break;
    case 5: convolve_symmetric_5tap_12bit(im, state->kernel0, pyr[0]); break;
    case 7: convolve_symmetric_7tap_12bit(im, state->kernel0, pyr[0]); break;
    }
    
    for (int i=1; i<num_levels; ++i) {
        Image<uint8_t> smooth;
        switch (state->taps1) {
        case 0: smooth = pyr[i-1].copy(); break;
        case 3: convolve_symmetric_3tap_12bit(pyr[i-1], state->kernel1, smooth); break;
        case 5: convolve_symmetric_5tap_12bit(pyr[i-1], state->kernel1, smooth); break;
        case 7: convolve_symmetric_7tap_12bit(pyr[i-1], state->kernel1, smooth); break;
        }

        if (state->dscale == 1.5)
            subsample_two_thirds_bicubic(smooth, pyr[i]);
        else if (state->dscale == 1.25)
            subsample_four_fifths_bicubic(smooth, pyr[i]);
        else
            bicubic_scale(smooth, state->scaler, pyr[i]);
    }
}



template <class T> static
void sample_3x3(const Image<T>& im, float x, float y, float win[3*3])
{
    int lx = (int)x;
    int ly = (int)y;
    x -= lx;
    y -= ly;
    const T *p = im[ly-1] + lx-1;
    const int s = im.stride();
        
    for (int i=0; i<3; ++i, p += s) {
        for (int j=0; j<3; ++j) {
            T a = p[j], b=p[j+1], c=p[j+s], d=p[j+1+s];
            float top = a + (b-a) * x;
            float bot = c + (d-c) * x;
            win[i*3+j] = top + (bot - top)*y;
        }
    }
}

template <int N, class T, class Op>
bool is_extremum(T x, const T* y, const Op& op)
{
    for (int i=0; i<N; ++i)
        if (!op(x, y[i]))
            return false;
    return true;
}

static
bool find_peak(const float finer[3*3], const float curr[3*3], const float coarser[3*3],
               float H[3*3], float offset[3], float& peak_val)
{
    using namespace latl;

    float twice_mid = 2 * curr[4];
    
    Vector<3,float> g;
    g[0] = 0.5f * (curr[5] - curr[3]);
    g[1] = 0.5f * (curr[7] - curr[1]);
    g[2] = 0.5f * (coarser[4] - finer[4]);

    H[0] = curr[5] + curr[3] - twice_mid;
    H[1] = 0.25f * ((curr[8] - curr[6]) - (curr[2] - curr[0]));
    H[2] = 0.25f * ((coarser[5] - coarser[3]) - (finer[5] - finer[3]));
    
    H[3] = H[1];
    H[4] = curr[7] + curr[1] - twice_mid;
    H[5] = 0.25f * ((coarser[7] - coarser[1]) - (finer[7] - finer[1]));

    H[6] = H[2];
    H[7] = H[5];
    H[8] = coarser[4] + finer[4] - twice_mid;

    LDLT<3,float> ldlt;
    ldlt.compute(RefMatrix<3,3,3,1,float>(H));
    if (!ldlt.is_full_rank())
        return false;

    Vector<3,float> x = ldlt.inverse_times(-g);
    
    if (latl::abs(x[0]) > 0.8f ||
        latl::abs(x[1]) > 0.8f ||
        latl::abs(x[2]) > 0.8f)
        return false;

    offset[0] = x[0];
    offset[1] = x[1];
    offset[2] = x[2];

    peak_val = curr[4] + 0.5f * (g * x);

    return true;
}

void ecv::scale_space::find_extrema(const Pyramid<float>& pyr, float thresh,
                                    std::vector<Point>& extrema)
{
    const int BORDER = 2;

    float coarser[3*3];
    float mid[3*3];
    float finer[3*3];
    float H[3*3];
    float offset[3];

    const float s_coarser = 1 / pyr.scale_factor;
    const float s_finer = pyr.scale_factor;
    const float ln_dscale = logf(pyr.scale_factor);

    extrema.clear();
    for (size_t lev=1; lev+1<pyr.diff.size(); ++lev)
    {
        const Image<float>& d = pyr.diff[lev];
        const int s = d.stride();
        const float level_scale = powf(pyr.scale_factor, lev);
            
        for (int i=BORDER; i+BORDER<d.height(); ++i)
        {
            const float *di = d[i] + BORDER;
            for (int j=BORDER; j+BORDER<d.width(); ++j, ++di)
            {
                float v = di[0];
                if (v < -thresh) {
                    // Minimum?
                    if (v > di[  -1] || v > di[ 1] ||
                        v > di[-s-1] || v > di[-s] || v > di[-s+1] ||
                        v > di[ s-1] || v > di[ s] || v > di[ s+1])
                        continue;

                    sample_3x3(pyr.diff[lev-1], j*s_finer, i*s_finer, finer);
                    if (!is_extremum<9>(v, finer, std::less<float>()))
                        continue;
                    
                    sample_3x3(pyr.diff[lev+1], j*s_coarser, i*s_coarser, coarser);
                    if (!is_extremum<9>(v, coarser, std::less<float>()))
                        continue;
                    
                } else if (v > thresh) {
                    // Maximum?
                    if (v < di[  -1] || v < di[ 1] ||
                        v < di[-s-1] || v < di[-s] || v < di[-s+1] ||
                        v < di[ s-1] || v < di[ s] || v < di[ s+1])
                        continue;
                    
                    sample_3x3(pyr.diff[lev-1], j*s_finer, i*s_finer, finer);
                    if (!is_extremum<9>(v, finer, std::greater<float>()))
                        continue;
                    
                    sample_3x3(pyr.diff[lev+1], j*s_coarser, i*s_coarser, coarser);
                    if (!is_extremum<9>(v, coarser, std::greater<float>()))
                        continue;
                } else
                    continue;

                for (int k=0; k<3; ++k) {
                    mid[k] = di[-s+k-1];
                    mid[3+k] = di[k-1];
                    mid[6+k] = di[s+k-1];
                }
                
                float strength;
                if (!find_peak(finer, mid, coarser, H, offset, strength))
                    continue;

                {
                    const float r = 10.0f;
                    float tr = H[0] + H[4];
                    float det = H[0]*H[4] - H[1]*H[1];
                    if (det < 0)
                        continue;

                    if (tr*tr*r > (r+1)*(r+1)*det)
                        continue;
                }
                
                Point p;
                p.level = lev;
                p.level_x = j + offset[0];
                p.level_y = i + offset[1];
                p.level_scale = expf(offset[2] * ln_dscale);
                
                p.strength = strength;
                p.scale = level_scale * p.level_scale;
                p.x = level_scale * p.level_x;
                p.y = level_scale * p.level_y;

                extrema.push_back(p);
            }
        }
    }
}
