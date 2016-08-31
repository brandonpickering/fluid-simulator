
#include <gamelib.hpp>


using namespace gamelib;
using namespace std;


static const int gridsize = 50;

static float zero4[4*gridsize*gridsize] = {0};

static gl::Program program, renderProgram;
static gl::VertexArray varray;
static gl::Framebuffer fbo;


static gl::Texture cellType;
static gl::Texture velocity, velocityTemp;
static gl::Texture pressure, pressureTemp;

static gl::Texture temper, temperTemp;
static gl::Texture smoke, smokeTemp;
static gl::Texture fire, fireTemp;

static gl::Texture forces, forcesTemp;



static float mag(float x, float y) {
    return sqrt(x*x + y*y);
}

static int mod(int a, int b) {
    int res = a % b;
    return res >= 0 ? res : res + b;
}

static int ix(int x, int y, int n=1, int k=0) {
    return (mod(y, gridsize) * gridsize + mod(x, gridsize))*n + k;
}


static void loadBuffer(gl::Texture tex, float *data) {
    tex.bind();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, gridsize, gridsize, 0, GL_RGBA, GL_FLOAT, data);
}

static gl::Texture createTex(GLenum filter, GLenum wrapMode=GL_REPEAT) {
    gl::Texture tex = gl::Texture::create(GL_TEXTURE_2D);
    tex.bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    loadBuffer(tex, zero4);
    return tex;
}


gl::Program loadProgram(string vfilename, string ffilename) {
    string includePattern = "\\s*#include\\s+\"([^\"]*)\"\\s*";
    TextFile vfile = TextFile::load(vfilename, includePattern);
    gl::Shader vshader = gl::Shader::create(GL_VERTEX_SHADER, vfile.getLines());
    vfile.destroy();
    TextFile ffile = TextFile::load(ffilename, includePattern);
    gl::Shader fshader = gl::Shader::create(GL_FRAGMENT_SHADER, ffile.getLines());
    ffile.destroy();
    gl::Program program = gl::Program::create({vshader, fshader});
    vshader.destroy(); fshader.destroy();
    printf("%s", program.getInfoLog().c_str());
    return program;
}


static void initialize() {
    program = loadProgram("shaders/simple.vert", "shaders/fluid.frag");
    renderProgram = loadProgram("shaders/simple.vert", "shaders/render.frag");

    varray = gl::VertexArray::create();
    GLfloat vdata[] = {-1,-1,  1,-1,  -1,1,  1,1};
    gl::Buffer vbuf = gl::Buffer::create(sizeof(vdata), vdata);
    varray.attribPointer(0, vbuf, 2);

    fbo = gl::Framebuffer::create();
    fbo.drawBuffers({GL_COLOR_ATTACHMENT0});
    fbo.readBuffer(GL_COLOR_ATTACHMENT0);


    GLfloat types[gridsize*gridsize*4] = {0};
    for (int x = 0; x < gridsize; x++) {
        types[ix(x, 0, 4, 0)] = 1;
        types[ix(x, gridsize-1, 4, 0)] = 2;
    }
    for (int y = 1; y < gridsize; y++) 
        types[ix(0, y, 4, 0)] = types[ix(gridsize-1, y, 4, 0)] = 2;
    cellType = createTex(GL_NEAREST); loadBuffer(cellType, types);

    velocity = createTex(GL_LINEAR); velocityTemp = createTex(GL_LINEAR);
    pressure = createTex(GL_NEAREST); pressureTemp = createTex(GL_NEAREST);

    temper = createTex(GL_LINEAR); temperTemp = createTex(GL_LINEAR);
    smoke = createTex(GL_LINEAR); smokeTemp = createTex(GL_LINEAR);
    fire = createTex(GL_LINEAR); fireTemp = createTex(GL_LINEAR);

    forces = createTex(GL_LINEAR); forcesTemp = createTex(GL_LINEAR);
}


static void update(float dt) {
    glDisable(GL_BLEND);

    fbo.bindDraw();
    glViewport(0, 0, gridsize, gridsize);
    program.use();
    varray.bind();

    program.uniform1i("uGridsize", gridsize);
    program.uniform1f("uTimestep", dt);

    program.uniform1f("uQuantityDepletion", 0);


    // Add forces
    program.uniform1i("uProcedure", 6);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uForces", 2); forces.bind(GL_TEXTURE2);
    program.uniform1i("uTemper", 3); temper.bind(GL_TEXTURE3);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, forcesTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(forces, forcesTemp);


    // Advect velocity
    program.uniform1i("uProcedure", 2);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, velocityTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(velocity, velocityTemp);


    // Apply forces
    program.uniform1i("uProcedure", 1);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uForces", 2); forces.bind(GL_TEXTURE2);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, velocityTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(velocity, velocityTemp);


    // Project
    program.uniform1i("uProcedure", 3);
    loadBuffer(pressure, zero4);        // Do we get faster convergence if we reuse last frame's pressure?
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uPressure", 2);
    for (int k = 0; k < 50; k++) {
        pressure.bind(GL_TEXTURE2);
        fbo.texture2D(GL_COLOR_ATTACHMENT0, pressureTemp);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        swap(pressure, pressureTemp);
    }

    program.uniform1i("uProcedure", 4);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uPressure", 2); pressure.bind(GL_TEXTURE2);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, velocityTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(velocity, velocityTemp);


    // Advect temperature
    program.uniform1i("uProcedure", 5);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uQuantity", 2); temper.bind(GL_TEXTURE2);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, temperTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(temper, temperTemp);


    // Advect smoke
    program.uniform1i("uProcedure", 5);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uQuantity", 2); smoke.bind(GL_TEXTURE2);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, smokeTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    swap(smoke, smokeTemp);

    // Advect fire
    program.uniform1i("uProcedure", 5);
    program.uniform1i("uCellType", 0); cellType.bind(GL_TEXTURE0);
    program.uniform1i("uVelocity", 1); velocity.bind(GL_TEXTURE1);
    program.uniform1i("uQuantity", 2); fire.bind(GL_TEXTURE2);
    program.uniform1f("uQuantityDepletion", 2);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, fireTemp);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program.uniform1f("uQuantityDepletion", 0);
    swap(fire, fireTemp);
}


static const int windowsize = 512;

static float g_to_w(float coord) {
    return 2 * (coord + 0.5f)/gridsize - 1;
}

static float w_to_g(float coord) {
    return 0.5f * (coord + 1) * gridsize - 0.5f;
}

static bool drawVel = false;
static vector<Vec2f> flames;
static Timer flametimer;

static void setUp(float dt, Input input, Vec2f mousePos, Vec2f mouseVel) {
    if (input.wasKeyPressed("Tab")) drawVel = !drawVel;

    static float force[4*gridsize*gridsize];

    static float type[4*gridsize*gridsize];

    static float temperbuf[4*gridsize*gridsize];
    static float smokebuf[4*gridsize*gridsize];
    static float firebuf[4*gridsize*gridsize];
    fbo.bindRead();
    fbo.texture2D(GL_COLOR_ATTACHMENT0, cellType); glReadPixels(0, 0, gridsize, gridsize, GL_RGBA, GL_FLOAT, type);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, temper); glReadPixels(0, 0, gridsize, gridsize, GL_RGBA, GL_FLOAT, temperbuf);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, smoke); glReadPixels(0, 0, gridsize, gridsize, GL_RGBA, GL_FLOAT, smokebuf);
    fbo.texture2D(GL_COLOR_ATTACHMENT0, fire); glReadPixels(0, 0, gridsize, gridsize, GL_RGBA, GL_FLOAT, firebuf);


    if (flametimer.recur(seconds(0.05f))) {
        float x = 2 * ((float) rand() / RAND_MAX) - 1;
        Vec2f flame = Vec2f(x, -1 + 7.0f/gridsize);
        flames.push_back(flame);
    }

    while (flames.size() > 50) flames.erase(flames.begin());

    for (Vec2f flame : flames) {
        for (int x = 0; x < gridsize; x++) {
            for (int y = 0; y < gridsize; y++) {
                if (type[ix(x, y, 4, 0)] != 0) continue;
                float wx = g_to_w(x); float wy = g_to_w(y);
                float dist = mag(wx - flame.x, wy - flame.y);
                float a = 5 * fmax(1 - 3*dist, 0);
                if (dist < 0.2) temperbuf[ix(x, y, 4, 0)] = a;
                if (dist < 0.2) smokebuf[ix(x, y, 4, 0)] = 1;
                if (dist < 0.2) firebuf[ix(x, y, 4, 0)] = 1;
            }
        }
    }


    
    for (int x = 0; x < gridsize; x++) {
        for (int y = 0; y < gridsize; y++) {
            force[ix(x, y, 4, 0)] = 0;
            force[ix(x, y, 4, 1)] = 0;

            float wx = g_to_w(x); float wy = g_to_w(y);
            float dist = mag(wx - mousePos.x, wy - mousePos.y);
            if (input.isButtonDown("Right")) {
                float a = 60 * fmax(1 - 3*dist, 0);
                force[ix(x, y, 4, 0)] = a * mouseVel.x;
                force[ix(x, y, 4, 1)] = a * mouseVel.y;
            }
        }
    }


    loadBuffer(forces, force);
    loadBuffer(temper, temperbuf);
    loadBuffer(smoke, smokebuf);
    loadBuffer(fire, firebuf);
}


static void render() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    gl::Framebuffer::getDefault().bindDraw();
    gl::Program(0).use();
    gl::VertexArray(0).bind();

    glViewport(0, 0, windowsize, windowsize);
    glClear(GL_COLOR_BUFFER_BIT);



    static GLfloat vel[gridsize*gridsize*4];

    if (drawVel) {
        fbo.bindRead();
        fbo.texture2D(GL_COLOR_ATTACHMENT0, velocity);
        glReadPixels(0, 0, gridsize, gridsize, GL_RGBA, GL_FLOAT, vel);

        gl::Texture::getDefault(GL_TEXTURE_2D).bind();
        glColor3f(0.5f, 0.5f, 1);
        glBegin(GL_LINES);
        for (int x = 0; x < gridsize; x++) {
            for (int y = 0; y < gridsize; y++) {
                float vx = (vel[ix(x, y, 4, 0)] + vel[ix(x+1, y, 4, 0)])/2;
                float vy = (vel[ix(x, y, 4, 1)] + vel[ix(x, y+1, 4, 1)])/2;

                float wx = g_to_w(x); float wy = g_to_w(y);
                float s = 2.0f / gridsize * 0.1f;
                glVertex2f(wx, wy);
                glVertex2f(wx + s*vx, wy + s*vy);
            }
        }
        glEnd();
    }


    renderProgram.use();
    varray.bind();

    renderProgram.uniform1i("uProcedure", 4);
    renderProgram.uniform1i("uData", 0); cellType.bind(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    renderProgram.uniform1i("uProcedure", 1);
    renderProgram.uniform1i("uData", 0); smoke.bind(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // renderProgram.uniform1i("uProcedure", 2);
    // renderProgram.uniform1i("uData", 0); temper.bind(GL_TEXTURE0);
    // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    renderProgram.uniform1i("uProcedure", 3);
    renderProgram.uniform1i("uData", 0); fire.bind(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}



int SDL_main(int argc, char *argv[]) {
    Logger::init();
    Window::init(3, 2, true);
    Benchmark::init();

    Window window("", windowsize, windowsize);
    Input input = window.getInput();

    glEnable(GL_TEXTURE_2D);

    glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
    glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
    glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

    initialize();

    Timer timer;
    Timer veltimer;

    auto getMousePos([&]() {
        Vec2f mousePos = input.getMousePos().to_f();
        mousePos.x = 2 * mousePos.x / window.getWidth() - 1;
        mousePos.y = 2 * mousePos.y / window.getHeight() - 1;
        return mousePos;
    });
    Vec2f mousePos = getMousePos();
    Vec2f mouseVel = {0, 0};

    while (window.isOpen()) {
        while (timer.getElapsed() < seconds(1/60.0f)) {}
        float dt = timer.tick().seconds();

        float velres = 0.05f;
        if (veltimer.recur(seconds(velres))) {
            Vec2f lastMousePos = mousePos;
            mousePos = getMousePos();
            mouseVel = (mousePos - lastMousePos) / velres;
        }

        setUp(dt, input, mousePos, mouseVel);
        update(dt);
        render();

        window.display();
        Window::handleEvents();

        Benchmark::markFrame();
        if (Benchmark::timeSinceReport() >= seconds(1))
            printf("%s\n", Benchmark::report().toString().c_str());
    }
    
    Window::quit();
    return 0;
}