#version 300 es
precision mediump float;
in vec3 vColor;
in float vBrightness;
uniform float uIntensity;
out vec4 outColor;
void main() {
    vec2 c = gl_PointCoord - vec2(0.5);
    float d = length(c);
    if (d > 0.5) discard;
    float core = 1.0 - smoothstep(0.0, 0.35, d);
    float halo = 1.0 - smoothstep(0.35, 0.5, d);
    float falloff = core * 0.85 + halo * 0.25;
    float a = vBrightness * uIntensity * falloff;
    if (a < 0.02) discard;
    outColor = vec4(vColor * a, a);
}
