#include <GLFW/glfw3.h>
#include <DFSParks_Color.h>
namespace dfsparks {
  namespace ui {
    
    void setColor(uint32_t color) {
        glColor3f(chan_red(color)/255.f, 
          chan_green(color)/255.f, 
          chan_blue(color)/255.f);
    }

    void renderStrand(uint32_t color) {
        setColor(color);
        glBegin(GL_TRIANGLES);
        glVertex3f(-0.6f, -0.4f, 0.f);
        glVertex3f(0.6f, -0.4f, 0.f);
        glVertex3f(0.f, 0.6f, 0.f);
        glEnd();
    }

    int run() {
        GLFWwindow* window;
        if (!glfwInit())
            return -1;

        window = glfwCreateWindow(640, 480, "DiscoFish Sparks Demo", NULL, NULL);
        if (!window)
        {
            glfwTerminate();
            return -1;
        }

        glfwMakeContextCurrent(window);
        while (!glfwWindowShouldClose(window))
        {
            glClear(GL_COLOR_BUFFER_BIT);

            renderStrand(rgb(255,255,0));

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        glfwTerminate();
        return 0;
    }

  } // namespace ui
} // namespace dfsparks


