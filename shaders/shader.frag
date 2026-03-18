#version 450

layout (location = 0) in vec2 fragPos;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push{
  mat2 tranform;
  vec2 offset;
  vec3 color;
  vec2 resolution;
} push;

void main() {
    // Position du fragment en NDC
    vec2 fragNDC = (gl_FragCoord.xy / push.resolution) * 2.0 - 1.0;

    // Vecteur depuis le centre de l'objet (push.offset) vers le fragment
    vec2 diff = fragNDC - push.offset;

    // Correction d'aspect en screen space (toujours correcte peu importe la rotation)
    float minDim = min(push.resolution.x, push.resolution.y);
    diff *= push.resolution / minDim;

    float dist = 0.5 - length(diff);
    float circle = smoothstep(0.0, 0.005, dist);

    vec2 uv = fragPos * 0.5 + 0.5;
    outColor = vec4(circle * vec3(uv, 1.0), circle);
}
