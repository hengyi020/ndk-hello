// 最小 NDK 程序：NativeActivity + EGL + GLES3，画一个会变色的三角形。
// 没有 Java、没有 Gradle —— 全部用 clang + aapt2 手工打包。
#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ndkhello", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ndkhello", __VA_ARGS__)

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
static int32_t g_w = 0, g_h = 0;
static GLuint g_prog = 0, g_vbo = 0;
static GLint g_ut = -1;
static double g_t0 = 0.0;

static GLuint shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, (GLsizei)sizeof(log), NULL, log);
        LOGE("shader compile failed: %s", log);
    }
    return s;
}

static void init_gl(void) {
    static const char *vs =
        "#version 300 es\n"
        "in vec2 a_pos;\n"
        "void main(){ gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 300 es\n"
        "precision mediump float;\n"
        "uniform float u_t;\n"
        "out vec4 o;\n"
        "void main(){ o = vec4(0.5+0.5*sin(u_t), 0.5+0.5*sin(u_t+2.0), "
        "0.5+0.5*sin(u_t+4.0), 1.0); }\n";

    GLuint v = shader(GL_VERTEX_SHADER, vs);
    GLuint f = shader(GL_FRAGMENT_SHADER, fs);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, v);
    glAttachShader(g_prog, f);
    glLinkProgram(g_prog);
    GLint ok = 0;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(g_prog, (GLsizei)sizeof(log), NULL, log);
        LOGE("link failed: %s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    g_ut = glGetUniformLocation(g_prog, "u_t");

    static const float verts[] = {0.0f, 0.6f, -0.6f, -0.6f, 0.6f, -0.6f};
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLint loc = glGetAttribLocation(g_prog, "a_pos");
    if (loc >= 0) {
        glEnableVertexAttribArray((GLuint)loc);
        glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
    }
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    LOGI("GL up: %s | %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
}

static int init_egl(ANativeWindow *win) {
    if (g_surf != EGL_NO_SURFACE) return 1;

    if (g_dpy == EGL_NO_DISPLAY) {
        g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint maj = 0, min = 0;
        if (!eglInitialize(g_dpy, &maj, &min)) {
            LOGE("eglInitialize failed");
            return 0;
        }
        const EGLint attrs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
            EGL_NONE};
        EGLConfig cfg;
        EGLint n = 0;
        if (!eglChooseConfig(g_dpy, attrs, &cfg, 1, &n) || n < 1) {
            LOGE("no matching EGLConfig");
            return 0;
        }
        const EGLint ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctx_attrs);
        if (g_ctx == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed");
            return 0;
        }
        g_surf = eglCreateWindowSurface(g_dpy, cfg, win, NULL);
        if (g_surf == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed");
            return 0;
        }
        eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx);
        eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w);
        eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
        init_gl();
        LOGI("surface %dx%d", g_w, g_h);
    }
    return 1;
}

static void render_frame(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    float t = (float)(now - g_t0);

    glViewport(0, 0, g_w, g_h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (g_prog) {
        glUseProgram(g_prog);
        if (g_ut >= 0) glUniform1f(g_ut, t);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    eglSwapBuffers(g_dpy, g_surf);
}

static void handle_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) init_egl(app->window);
            break;
        case APP_CMD_TERM_WINDOW:
            if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) {
                eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroySurface(g_dpy, g_surf);
                g_surf = EGL_NO_SURFACE;
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
            if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) {
                eglQuerySurface(g_dpy, g_surf, EGL_WIDTH, &g_w);
                eglQuerySurface(g_dpy, g_surf, EGL_HEIGHT, &g_h);
            }
            break;
        default:
            break;
    }
}

void android_main(struct android_app *app) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    g_t0 = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    app->onAppCmd = handle_cmd;
    LOGI("android_main started");

    for (;;) {
        int events = 0;
        struct android_poll_source *src = NULL;
        int ident;
        while ((ident = ALooper_pollOnce(16, NULL, &events, (void **)&src)) >= 0) {
            if (src != NULL) src->process(app, src);
            if (app->destroyRequested != 0) {
                if (g_dpy != EGL_NO_DISPLAY && g_surf != EGL_NO_SURFACE) {
                    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroySurface(g_dpy, g_surf);
                    g_surf = EGL_NO_SURFACE;
                }
                if (g_dpy != EGL_NO_DISPLAY && g_ctx != EGL_NO_CONTEXT) {
                    eglDestroyContext(g_dpy, g_ctx);
                    g_ctx = EGL_NO_CONTEXT;
                }
                if (g_dpy != EGL_NO_DISPLAY) {
                    eglTerminate(g_dpy);
                    g_dpy = EGL_NO_DISPLAY;
                }
                LOGI("exiting");
                return;
            }
        }
        if (g_surf != EGL_NO_SURFACE) render_frame();
    }
}
