#version 320 es
precision highp float;

// you might need to enable the "render-every-frame" plugin option for this to animate properly
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D tex;
uniform sampler2D backgroundTex;
uniform vec2 windowPos;
uniform vec2 windowSize;
uniform vec2 monitorSize;
uniform float time;

const float SPEED = 5.0;
const float MAGNITUDE = 0.002;

void main() {
    float t = sin(time * 0.28);
    vec2 wobbledCoords = v_texcoord;

    wobbledCoords.x += sin(v_texcoord.y * 10.0 * SPEED + (t * t) * SPEED) * MAGNITUDE;

    wobbledCoords.y += sin(v_texcoord.x * 10.0 * SPEED + (t * t) * SPEED) * MAGNITUDE;

    fragColor = texture(tex, wobbledCoords);
}