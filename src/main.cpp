// src/main.cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>

#include "renderer/vulkancontext.h"
#include "renderer/swapchain.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "core/timer.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    const uint32_t initialWidth = 1600;
    const uint32_t initialHeight = 900;

    SDL_Window* window = SDL_CreateWindow(
        "Vulkan PBR Renderer",
        initialWidth, initialHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if(!window) {
        LOG_ERROR("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    try {
        vpbr::VulkanContext ctx;
        ctx.init(window);

        vpbr::Swapchain swapchain;
        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        swapchain.create(ctx, static_cast<uint32_t>(w), static_cast<uint32_t>(h));

        vpbr::Scene scene;
        scene.loadDefaultScene(); // loads Sponza or similar

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
                if(event.type == SDL_EVENT_WINDOW_RESIZED) {
                    SDL_GetWindowSizeInPixels(window, &w, &h);
                    if(w > 0 && h > 0) {
                        renderer.recreateSwapchain(ctx, swapchain,
                                                   static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                    }
                }
                renderer.handleInput(event, timer.getDeltaTime());
            }

            if(!minimized && w > 0 && h > 0) {
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
    }
    catch(const std::exception& e) {
        LOG_ERROR("Fatal error: %s", e.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}