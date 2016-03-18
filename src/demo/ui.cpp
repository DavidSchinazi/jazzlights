#include <mutex>
#include <cmath>
#include <GLFW/glfw3.h>
#include <DFSParks_Color.h>
#include "DFSparks.h"

namespace dfsparks {
  namespace ui {
    
    const double TWO_PI = 2 * 3.1415926;
    const double LED_R = 0.05; // in window heights
    const double LED_DIST = 3*LED_R;

    std::mutex mut;
    Strand strand1;
    Strand strand2;

    void render_circle(double cx, double cy, double r, uint32_t color) {
        const int segments = 1000;
        glColor4f(chan_red(color)/255.f, chan_green(color)/255.f, 
            chan_blue(color)/255.f, chan_alpha(color)/255.f);
        glEnable(GL_LINE_SMOOTH);
        glBegin(GL_LINES);
        for(int i = 0; i <= segments; ++i) {
            glVertex2f(cx, cy);
            glVertex2f(cx + (r * cos(i * TWO_PI/segments)), 
                cy + (r * sin(i * TWO_PI/segments)));
        }
        glEnd();
    }

    void render_led(double cx, double cy, uint32_t color) {
        render_circle(cx, cy, LED_R*1.1, rgb(9,9,9));
        render_circle(cx, cy, LED_R, color);
    }

    void render_stripe(const Strand& strand, double cx, double cy) {
        std::lock_guard<std::mutex> lock(mut);
        double len = strand.length()*LED_DIST;
        
        for( int i=0; i < strand.length(); ++i) {
            render_led(cx - len/2 + LED_DIST*i, cy, strand.color(i));
        }
    }

    void render_ring(const Strand& strand, double cx, double cy) {
        std::lock_guard<std::mutex> lock(mut);
        double r = LED_DIST * strand.length() / TWO_PI;

        for( int i=0; i < strand.length(); ++i) {
            render_led(cx + (r * cos(i * TWO_PI/strand.length())), 
                cy + (r * sin(i * TWO_PI/strand.length())), strand.color(i));
        }
    }

    void update(void *data, size_t len) {
        std::lock_guard<std::mutex> lock(mut);
        strand1.update(data, len);
        strand2.update(data, len);
    }

    void on_resize(GLFWwindow* window, int width, int height) {
        double aspect = 1.0*width/height;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-aspect, aspect, -1, 1, -1, 1); 
        glMatrixMode(GL_MODELVIEW);   
        glLoadIdentity();
    }

    int run() {
        const int WIN_W = 600;
        const int WIN_H = 480;
        strand1.begin(16);
        strand2.begin(8);

        GLFWwindow* window;
        if (!glfwInit())
            return -1;

        window = glfwCreateWindow(WIN_W, WIN_H, "DiscoFish Sparks Demo", NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return -1;
        }
        glfwSetWindowSizeCallback(window, on_resize);
        glfwMakeContextCurrent(window);

        on_resize(window, WIN_W, WIN_H);
        
        while (!glfwWindowShouldClose(window))
        {
            glClear(GL_COLOR_BUFFER_BIT);

            render_ring(strand1, 0, 0.3);
            render_stripe(strand2, 0, -0.5);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glfwTerminate();
        return 0;
    }

  } // namespace ui
} // namespace dfsparks


