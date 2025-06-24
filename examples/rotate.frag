#version 320 es
precision highp float;

// you might need to enable the "render-every-frame" plugin option for this to animate properly
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D tex;
uniform vec2 windowPos;
uniform vec2 windowSize;
uniform vec2 monitorSize;
uniform float time;

void main() {
    vec2 p = (v_texcoord * monitorSize) - windowPos;
    vec2 R = windowSize;
    p.y = R.y - p.y; 

    if (p.x < -0.0 || p.y < -0.0 || p.x > R.x + 0.0 || p.y > R.y + 0.0) {
        fragColor = texture(tex, p);
        discard;
    }

    vec2 windowCenterPixels = windowPos + (windowSize / 2.0);
    vec2 normalizedWindowCenter = windowCenterPixels / monitorSize;

    float angle = sin(time * 0.28);

    float s = sin(angle);
    float c = cos(angle);

    mat2 rotationMatrix = mat2(c, s, -s, c);

    vec2 pivot = normalizedWindowCenter;

    fragColor = texture(tex, rotationMatrix * (v_texcoord - pivot) + pivot);
}

