#version 300 es
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;
out vec2 fragUV;
out vec3 fragWorldNormal;
out highp vec4 fragShadowCoord;
uniform mat4 uModel;
uniform highp mat4 uLightVP;
uniform mat4 uBoneMatrices[48];
layout(std140) uniform FrameData {
    mat4 uViewProjection;
    vec4 uLightDirAmbient;
};
void main() {
    fragUV = inUV;
    mat4 skin = inBoneWeights.x * uBoneMatrices[inBoneIndices.x]
              + inBoneWeights.y * uBoneMatrices[inBoneIndices.y]
              + inBoneWeights.z * uBoneMatrices[inBoneIndices.z]
              + inBoneWeights.w * uBoneMatrices[inBoneIndices.w];
    vec4 skinnedPos = skin * vec4(inPosition, 1.0);
    fragWorldNormal = mat3(uModel * skin) * inNormal;
    fragShadowCoord = uLightVP * uModel * skinnedPos;
    gl_Position = uViewProjection * uModel * skinnedPos;
}
