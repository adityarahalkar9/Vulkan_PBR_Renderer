#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

#include "renderer/vulkancontext.h"
#include "renderer/swapchain.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "core/timer.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Vulkan PBR Renderer",
        1600, 900,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if(!window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    try {
        vpbr::VulkanContext ctx;
        ctx.init(window);

        int w = 1600, h = 900;
        SDL_GetWindowSizeInPixels(window, &w, &h);

        vpbr::Swapchain swapchain;
        swapchain.create(ctx, static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        vpbr::Scene scene;
        scene.loadDefaultScene(ctx);

        vpbr::Renderer renderer;
        renderer.init(ctx, swapchain, scene);

        vpbr::Timer timer;
        timer.start();

        bool running = true;
        bool minimized = false;

        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
                if(event.type == SDL_EVENT_WINDOW_MINIMIZED) {
                    minimized = true;
                }
                if(event.type == SDL_EVENT_WINDOW_RESTORED) {
                    minimized = false;
                }
                if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    running = false;
                }

                renderer.handleInput(event, scene.camera);
            }

            if(!minimized) {
                timer.tick();
                renderer.update(scene, timer.getDeltaTime());
                renderer.drawFrame(ctx, swapchain, scene);
            }
        }

        ctx.waitIdle();

        renderer.cleanup(ctx);
        scene.cleanup(ctx);
        swapchain.cleanup(ctx);
        ctx.cleanup();

        LOG_INFO("Clean exit.");

    }
    catch(const std::exception& e) {
        LOG_ERROR("Fatal: %s", e.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}