#version 330


uniform int uProcedure;
uniform sampler2D uData;
uniform isampler2D uiData;

in vec2 vPosition;

out layout(location = 0) vec4 outColor;


void main() {
    vec2 npos = (vPosition+1)/2;
    outColor = texture(uData, npos);
    float val = outColor.x;


    // Smoke
    if (uProcedure == 1) {
        if (val < 0) val = 0; if (val > 1) val = 1;
        outColor = vec4(vec3(1), 0.1 * val);
    }

    // Temperature
    if (uProcedure == 2) {
        outColor = vec4(1, 0, 0, val);
    }

    // Fire
    if (uProcedure == 3) {
        if (val < 0) val = 0; if (val > 1) val = 1;
        outColor = vec4(1, val, 0, val > 0 ? 1 : 0);
    }

    // Cells
    if (uProcedure == 4) {
        int type = int(round(texture(uData, npos).r));
        if (type == 1) outColor = vec4(0, 1, 0, 1);
        else outColor = vec4(0);
    }
}
