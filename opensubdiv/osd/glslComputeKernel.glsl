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

//------------------------------------------------------------------------------

uniform int batchStart = 0;
uniform int batchEnd = 0;
uniform int srcOffset = 0;
uniform int dstOffset = 0;

layout(binding=0) buffer src_buffer      { float    srcVertexBuffer[]; };
layout(binding=1) buffer dst_buffer      { float    dstVertexBuffer[]; };
layout(binding=2) buffer stencilSizes    { int      _sizes[];   };
layout(binding=3) buffer stencilOffsets  { int      _offsets[]; };
layout(binding=4) buffer stencilIndices  { int      _indices[]; };
layout(binding=5) buffer stencilWeights  { float    _weights[]; };

layout(local_size_x=WORK_GROUP_SIZE, local_size_y=1, local_size_z=1) in;

//------------------------------------------------------------------------------

struct Vertex {
    float vertexData[LENGTH];
};

void clear(out Vertex v) {
    for (int i = 0; i < LENGTH; ++i) {
        v.vertexData[i] = 0;
    }
}

Vertex readVertex(int index) {
    Vertex v;
    int vertexIndex = srcOffset + index * SRC_STRIDE;
    for (int i = 0; i < LENGTH; ++i) {
        v.vertexData[i] = srcVertexBuffer[vertexIndex + i];
    }
    return v;
}

void writeVertex(int index, Vertex v) {
    int vertexIndex = dstOffset + index * DST_STRIDE;
    for (int i = 0; i < LENGTH; ++i) {
        dstVertexBuffer[vertexIndex + i] = v.vertexData[i];
    }
}

void addWithWeight(inout Vertex v, const Vertex src, float weight) {
    for (int i = 0; i < LENGTH; ++i) {
        v.vertexData[i] += weight * src.vertexData[i];
    }
}

//------------------------------------------------------------------------------
void main() {

    int current = int(gl_GlobalInvocationID.x) + batchStart;

    if (current>=batchEnd) {
        return;
    }

    Vertex dst;
    clear(dst);

    int offset = _offsets[current],
        size   = _sizes[current];

    for (int i=0; i<size; ++i) {
        addWithWeight(dst, readVertex( _indices[offset+i] ), _weights[offset+i]);
    }

    writeVertex(current, dst);
}

//------------------------------------------------------------------------------
