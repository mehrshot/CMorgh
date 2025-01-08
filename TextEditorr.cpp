#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <cstring>

using namespace std;

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

SDL_Color lightBG = {255, 255, 255, 255};
SDL_Color darkBG = {0, 0, 0, 255};
SDL_Color lightTxt = {0, 0, 0, 255};
SDL_Color darkTxt = {255, 255, 255, 255};

bool isDarkMode = false;
int main (int argc[], char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO < 0))
    {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return -1;
    }

    if (TTF_Init() == -1)
    {
        cerr << "TTF could not initialize! TTF_Error: " << SDL_GetError() << endl;
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL Text Editor",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH,
                                          SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font* font = TTF_OpenFont(R"(C:\Windows\Fonts\Times New Roman.ttf)", 24);
    SDL_Color textColor = lightTxt;
    string FileText = "File";
    string ViewText = "View", lightmode = "Light Mode", darkmode = "Dark Mode";

    bool quit = false;
    SDL_Event e;

    bool filemenuvisible = false;
    bool viewmenuvisible = false;

    while (!quit)
    {
        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                int mouseX = e.button.x;
                int mouseY = e.button.y;

                //File

                //View
                if (mouseX < 200 && mouseX > 100 && mouseY < 40)
                    viewmenuvisible = !viewmenuvisible;
                if (viewmenuvisible)
                {
                    if (mouseX > 100 && mouseX < 200)
                    {
                        if (mouseY > 40 && mouseY < 80)
                        {
                            isDarkMode = false;
                            textColor = lightTxt;
                            viewmenuvisible = false;
                        }
                        if (mouseY > 80 && mouseY < 120)
                        {
                            isDarkMode = true;
                            textColor = darkTxt;
                            viewmenuvisible = false;
                        }
                    }
                }
            }
        }

        if (isDarkMode)
        {
            SDL_SetRenderDrawColor(renderer, darkBG.r, darkBG.g, darkBG.b, 255);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, lightBG.r, lightBG.g, lightBG.b, 255);
        }
    }

    return 0;
}