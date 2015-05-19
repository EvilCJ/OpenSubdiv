//
//   Copyright 2013 Pixar
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//   with the following modification; you may not use this file except in
//   compliance with the Apache License and the following modification to it:
//   Section 6. Trademarks. is deleted and replaced with:
//
//   6. Trademarks. This License does not grant permission to use the trade
//      names, trademarks, service marks, or product names of the Licensor
//      and its affiliates, except as required to comply with Section 4(c) of
//      the License and to reproduce the content of the NOTICE file.
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License with the above modification is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//   KIND, either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#include "../osd/cpuKernel.h"
#include "../osd/tbbKernel.h"
#include "../osd/vertexDescriptor.h"

#include <cassert>
#include <cstdlib>
#include <tbb/parallel_for.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

namespace Osd {

#define grain_size  200

template <class T> T *
elementAtIndex(T * src, int index, VertexBufferDescriptor const &desc) {

    return src + index * desc.stride;
}

static inline void
clear(float *dst, VertexBufferDescriptor const &desc) {

    assert(dst);
    memset(dst, 0, desc.length*sizeof(float));
}

static inline void
addWithWeight(float *dst, const float *src, int srcIndex, float weight,
              VertexBufferDescriptor const &desc) {

    assert(src and dst);
    src = elementAtIndex(src, srcIndex, desc);
    for (int k = 0; k < desc.length; ++k) {
        dst[k] += src[k] * weight;
    }
}

static inline void
copy(float *dst, int dstIndex, const float *src,
     VertexBufferDescriptor const &desc) {

    assert(src and dst);

    dst = elementAtIndex(dst, dstIndex, desc);
    memcpy(dst, src, desc.length*sizeof(float));
}


class TBBStencilKernel {

    VertexBufferDescriptor _srcDesc;
    VertexBufferDescriptor _dstDesc;
    float const * _vertexSrc;
    float * _vertexDst;

    int const * _sizes;
    int const * _offsets,
              * _indices;
    float const * _weights;


public:
    TBBStencilKernel(float const *src,
                     VertexBufferDescriptor srcDesc,
                     float *dst,
                     VertexBufferDescriptor dstDesc,
                     int const * sizes, int const * offsets,
                     int const * indices, float const * weights) :
         _srcDesc(srcDesc),
         _dstDesc(dstDesc),
         _vertexSrc(src),
         _vertexDst(dst),
         _sizes(sizes),
         _offsets(offsets),
         _indices(indices),
         _weights(weights) { }

    TBBStencilKernel(TBBStencilKernel const & other) {
        _srcDesc    = other._srcDesc;
        _dstDesc    = other._dstDesc;
        _sizes      = other._sizes;
        _offsets    = other._offsets;
        _indices    = other._indices;
        _weights    = other._weights;
        _vertexSrc  = other._vertexSrc;
        _vertexDst  = other._vertexDst;
    }

    void operator() (tbb::blocked_range<int> const &r) const {
#define USE_SIMD
#ifdef USE_SIMD
        if (_srcDesc.length==4 and _srcDesc.stride==4 and _dstDesc.stride==4) {

            // SIMD fast path for aligned primvar data (4 floats)
            int offset = _offsets[r.begin()];
            ComputeStencilKernel<4>(_vertexSrc, _vertexDst,
                _sizes, _indices+offset, _weights+offset, r.begin(), r.end());

        } else if (_srcDesc.length==8 and _srcDesc.stride==4 and _dstDesc.stride==4) {

            // SIMD fast path for aligned primvar data (8 floats)
            int offset = _offsets[r.begin()];
            ComputeStencilKernel<8>(_vertexSrc, _vertexDst,
                _sizes, _indices+offset, _weights+offset, r.begin(), r.end());

        } else {
#else
        {
#endif
            int const * sizes = _sizes;
            int const * indices = _indices;
            float const * weights = _weights;

            if (r.begin()>0) {
                sizes += r.begin();
                indices += _offsets[r.begin()];
                weights += _offsets[r.begin()];
            }

            // Slow path for non-aligned data
            float * result = (float*)alloca(_srcDesc.length * sizeof(float));

            for (int i=r.begin(); i<r.end(); ++i, ++sizes) {

                clear(result, _dstDesc);

                for (int j=0; j<*sizes; ++j) {
                    addWithWeight(result, _vertexSrc, *indices++, *weights++, _srcDesc);
                }

                copy(_vertexDst, i, result, _dstDesc);
            }
        }
    }
};

void
TbbEvalStencils(float const * src,
                VertexBufferDescriptor const &srcDesc,
                float * dst,
                VertexBufferDescriptor const &dstDesc,
                int const * sizes,
                int const * offsets,
                int const * indices,
                float const * weights,
                int start, int end) {

    if (start > 0) {
        sizes += start;
        indices += offsets[start];
        weights += offsets[start];
    }
    src += srcDesc.offset;
    dst += dstDesc.offset;

    TBBStencilKernel kernel(src, srcDesc, dst, dstDesc,
                            sizes, offsets, indices, weights);

    tbb::blocked_range<int> range(start, end, grain_size);

    tbb::parallel_for(range, kernel);
}

}  // end namespace Osd

}  // end namespace OPENSUBDIV_VERSION
}  // end namespace OpenSubdiv
