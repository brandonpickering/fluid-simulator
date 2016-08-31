#version 330


uniform int uProcedure;

uniform int uGridsize;
uniform sampler2D uCellType; /*  0 = FLUID, 1 = SOLID, 2 = FREE  */
uniform float uTimestep;
uniform sampler2D uVelocity;

// Apply forces
uniform sampler2D uForces;

// Project
uniform sampler2D uPressure;

// Advect quantity
uniform sampler2D uQuantity;
uniform float uQuantityDepletion;

// Add forces
uniform sampler2D uTemper;


in vec2 vPosition;

out layout(location = 0) vec4 outResult;


vec2 getNPos(vec2 pos) {
    return (pos + vec2(0.5)) / uGridsize;
}

int getType(vec2 pos) {
    return int(round(texture(uCellType, getNPos(pos)).x));
}

vec2 getVel(vec2 pos) {
    float vx = texture(uVelocity, getNPos(pos + vec2(0.5, 0))).x;
    float vy = texture(uVelocity, getNPos(pos + vec2(0, 0.5))).y;
    return vec2(vx, vy);
}

vec2 slag(vec2 pos1, float dt) {
    vec2 vel = getVel(pos1);
    float m = length(vel);
    if (m != 0) vel /= m;

    vec2 pos0 = pos1;
    float t = 0;
    while (t <= m*dt) {
        vec2 pos0_c = pos1 - vel*t;
        if (getType(pos0_c) == 1) break;
        pos0 = pos0_c;
        t += 0.5;
    }
    if (t >= m*dt) pos0 = pos1 - vel*m*dt;

    return pos0;
}

float getCurl(vec2 pos) {
    float dv_dx = (getVel(pos+vec2(1,0)).y - getVel(pos-vec2(1,0)).y)/2;
    float du_dy = (getVel(pos+vec2(0,1)).x - getVel(pos-vec2(0,1)).x)/2;
    return dv_dx - du_dy;
}


void main() {
    outResult = vec4(0);

    float dt = uTimestep;

    vec2 npos = (vPosition+1)/2;
    vec2 cpos = npos * uGridsize - vec2(0.5);
    vec2 expos = cpos - vec2(0.5, 0);
    vec2 eypos = cpos - vec2(0, 0.5);


    // Apply forces
    if (uProcedure == 1) {
        float ndx = 1.0/uGridsize;

        vec2 nexpos = npos - vec2(ndx/2, 0);
        vec2 neypos = npos - vec2(0, ndx/2);

        vec2 evel = texture(uVelocity, npos).xy;
        vec2 force = vec2(texture(uForces, nexpos).x, texture(uForces, neypos).y);
        outResult = vec4(evel + force*dt, 0, 0);
    }


    // Advect velocity
    if (uProcedure == 2) {
        vec2 expos0 = slag(expos, dt);
        vec2 eypos0 = slag(eypos, dt);
        outResult.x = getType(expos0) == 1 ? getVel(expos).x : getVel(expos0).x;
        outResult.y = getType(eypos0) == 1 ? getVel(eypos).y : getVel(eypos0).y;
    }


    // Project
    if (uProcedure == 3) {
        if (getType(cpos) != 0) {
            outResult.x = 0;
            return;
        }

        int n = 0;
        float div = 0;
        if (getType(cpos + vec2(0.5, 0)) != 1) { div += getVel(cpos + vec2(0.5, 0)).x; n += 1; }
        if (getType(cpos - vec2(0.5, 0)) != 1) { div -= getVel(cpos - vec2(0.5, 0)).x; n += 1; }
        if (getType(cpos + vec2(0, 0.5)) != 1) { div += getVel(cpos + vec2(0, 0.5)).y; n += 1; }
        if (getType(cpos - vec2(0, 0.5)) != 1) { div -= getVel(cpos - vec2(0, 0.5)).y; n += 1; }

        float psum = 0;
        if (getType(cpos + vec2(1, 0)) == 0) psum += texture(uPressure, getNPos(cpos + vec2(1, 0))).x;
        if (getType(cpos - vec2(1, 0)) == 0) psum += texture(uPressure, getNPos(cpos - vec2(1, 0))).x;
        if (getType(cpos + vec2(0, 1)) == 0) psum += texture(uPressure, getNPos(cpos + vec2(0, 1))).x;
        if (getType(cpos - vec2(0, 1)) == 0) psum += texture(uPressure, getNPos(cpos - vec2(0, 1))).x;

        outResult.x = (psum - div/dt) / n;
    }

    if (uProcedure == 4) {
        outResult.xy = vec2(0);
        vec2 evel = texture(uVelocity, npos).xy;

        float p00 = texture(uPressure, getNPos(cpos)).x;
        float p10 = texture(uPressure, getNPos(cpos - vec2(1, 0))).x;
        float p01 = texture(uPressure, getNPos(cpos - vec2(0, 1))).x;

        if (getType(cpos - vec2(1, 0)) != 1 && getType(cpos) != 1)
            outResult.x = evel.x - (p00 - p10) * dt;
        if (getType(cpos - vec2(0, 1)) != 1 && getType(cpos) != 1)
            outResult.y = evel.y - (p00 - p01) * dt;
    }


    // Advect quantity
    if (uProcedure == 5) {
        if (getType(cpos) != 0) {
            outResult = vec4(0);
            return;
        }
        outResult = texture(uQuantity, getNPos(slag(cpos, dt))) - vec4(uQuantityDepletion) * dt;
    }


    // Add forces
    if (uProcedure == 6) {
        if (getType(cpos) == 1) {
            outResult.xy = vec2(0);
            return;
        }
        outResult = texture(uForces, npos);

        // Vorticity confinement
        float vort = getCurl(cpos);
        float gvortx = (abs(getCurl(cpos + vec2(1,0))) - abs(getCurl(cpos - vec2(1,0))))/2;
        float gvorty = (abs(getCurl(cpos + vec2(0,1))) - abs(getCurl(cpos - vec2(0,1))))/2;
        vec2 gvort = vec2(gvortx, gvorty);
        if (length(gvort) != 0) gvort = normalize(gvort);
        outResult.xy += 50.0/uGridsize * vort * vec2(gvort.y, -gvort.x);

        // Buoyancy
        outResult.y += 2.0 * uGridsize * texture(uTemper, npos).x;
    }
}
