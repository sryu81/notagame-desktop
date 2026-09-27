#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;
uniform mat4 uLightVP;
uniform mat4 uModel;
uniform mat4 uBoneMatrices[48];
void main() {
    mat4 skin = inBoneWeights.x * uBoneMatrices[inBoneIndices.x]
              + inBoneWeights.y * uBoneMatrices[inBoneIndices.y]
              + inBoneWeights.z * uBoneMatrices[inBoneIndices.z]
              + inBoneWeights.w * uBoneMatrices[inBoneIndices.w];
    gl_Position = uLightVP * uModel * skin * vec4(inPosition, 1.0);
}
