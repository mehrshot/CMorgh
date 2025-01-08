#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <map>
using namespace std;
// Screen dimensions
const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
// Ensure the current line is visible when adding new lines or moving the cursor
void ensureLastLineVisible(int currentLine, int &scrollOffset, int SCREEN_HEIGHT, int LINE_HEIGHT, int totalLines) {
    int cursorY = currentLine * LINE_HEIGHT - scrollOffset;
    if (cursorY < 0) {
        // Scroll up
        scrollOffset = currentLine * LINE_HEIGHT;
    } else if (cursorY + LINE_HEIGHT > SCREEN_HEIGHT) {
        // Scroll down
        scrollOffset = (currentLine + 1) * LINE_HEIGHT - SCREEN_HEIGHT;
    }

    // Ensure last line is always visible
    int contentHeight = totalLines * LINE_HEIGHT;
    if (contentHeight > SCREEN_HEIGHT) {
        scrollOffset = std::min(scrollOffset, contentHeight - SCREEN_HEIGHT);
    } else {
        scrollOffset = 0; // No scrolling needed if content fits
    }
}

// Define colors for Light Mode and Dark Mode
SDL_Color lightBackgroundColor = {255, 255, 255, 255};  // White for Light Mode
SDL_Color lightTextColor = {0, 0, 0, 255};  // Black for Light Mode
SDL_Color darkBackgroundColor = {0, 0, 0, 255};  // Black for Dark Mode
SDL_Color darkTextColor = {255, 255, 255, 255};  // White for Dark Mode

bool isDarkMode = false;  // Default mode: Light Mode
/// text color
map < string,SDL_Color > keywords ={
        {"while",{0,51,102}},
{"int",{0,128,128}}};
int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("SDL Text Editor",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH,
                                          SCREEN_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font
    TTF_Font *font = TTF_OpenFont(R"(C:\Windows\Fonts\Calibri.ttf)", 18); // Replace with the path to your .ttf font
    if (!font) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_Color textColor = {0, 0, 0, 255};  // Default text color (black)
    std::vector<std::string> lines = {""}; // Holds multiple lines of text
    int currentLine = 0; // Track the current line being edited
    int cursorPos = 1; // Track the cursor position within the current line
    int scrollOffset = 0; // Keeps track of scrolling
    const int LINE_HEIGHT = TTF_FontHeight(font); // Height of each line

    // Button text for "View" and menu options
    std::string viewText = "View";
    std::string lightModeText = "Light Mode";
    std::string darkModeText = "Dark Mode";
    // Button text for "Edit" and menu options
    std::string editText = "Edit";
    std::string undoText = "Undo";
    std::string redoText = "Redo";
    // Button text for "File" and menu options
    std::string FileText = "File";
    std::string NewText = "New";
    std::string SaveText = "Save";
    string exitText = "Exit";

    // Timer for cursor blinking
    Uint32 lastCursorToggle = SDL_GetTicks();
    bool cursorVisible = true;
    const Uint32 CURSOR_BLINK_INTERVAL = 500; // 500 ms for blinking
    bool quit = false;
    SDL_Event e;

    // Menu visibility flag
    bool ViewMenuVisible = false;

    while (!quit) {
        // Handle cursor blinking
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime > lastCursorToggle + CURSOR_BLINK_INTERVAL) {
            cursorVisible = !cursorVisible;
            lastCursorToggle = currentTime;
        }

        // Event handling loop
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEWHEEL) {
                // Handle scroll
                if (e.wheel.y > 0) { // Scroll up
                    scrollOffset = std::max(0, scrollOffset - LINE_HEIGHT);
                } else if (e.wheel.y < 0) { // Scroll down
                    scrollOffset += LINE_HEIGHT;
                }
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    if (cursorPos > 1 && cursorPos <= lines[currentLine].size()) {
                        // Remove character before cursor
                        lines[currentLine].erase(cursorPos - 1, 1);
                        cursorPos--;
                    } else if (currentLine > 0) {
                        // Merge with previous line
                        cursorPos = lines[currentLine - 1].size();
                        lines[currentLine - 1] += lines[currentLine].substr(1, lines[currentLine].size());
                        lines.erase(lines.begin() + currentLine);
                        currentLine--;
                    }

                    // Ensure there's always at least one line
                    if (lines.empty()) {
                        lines.push_back("");
                        currentLine = 0;
                        cursorPos = 0;
                    }
                } else if (e.key.keysym.sym == SDLK_RETURN) {
                    if (cursorPos <= lines[currentLine].size()) {
                        std::string remainder = lines[currentLine].substr(cursorPos);
                        lines[currentLine] = lines[currentLine].substr(0, cursorPos);
                        lines.insert(lines.begin() + currentLine + 1, remainder);
                        currentLine++;
                        cursorPos = 0;
                        ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                    }
                } else if (e.key.keysym.sym == SDLK_TAB) {
                    lines[currentLine].insert(cursorPos, "    ");
                    cursorPos += 4;
                } else if (e.key.keysym.sym == SDLK_LEFT) {
                    if (cursorPos > 1) {
                        cursorPos--;
                    } else if (currentLine > 0) {
                        currentLine--;
                        cursorPos = lines[currentLine].size();
                    }
                } else if (e.key.keysym.sym == SDLK_RIGHT) {
                    if (cursorPos < lines[currentLine].size()) {
                        cursorPos++;
                    } else if (currentLine < lines.size() - 1) {
                        currentLine++;
                        cursorPos = 0;
                    }
                } else if (e.key.keysym.sym == SDLK_UP) {
                    if (currentLine > 0) {
                        currentLine--;
                        cursorPos = std::min(cursorPos, (int) lines[currentLine].size());
                        ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                    }
                } else if (e.key.keysym.sym == SDLK_DOWN) {
                    if (currentLine < lines.size() - 1) {
                        currentLine++;
                        cursorPos = std::min(cursorPos, (int) lines[currentLine].size());
                        ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                if (e.text.text) {
                    lines[currentLine].insert(cursorPos, e.text.text);
                    cursorPos += strlen(e.text.text);
                    ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;

                if (mouseX < 100 && mouseY < 40) {
                    ViewMenuVisible = !ViewMenuVisible;  // Toggle the visibility of the menu
                }

                if (ViewMenuVisible) {
                    if (mouseX > 100 && mouseX < 250) {
                        if (mouseY > 40 && mouseY < 80) {
                            isDarkMode = false;  // Select Light Mode
                            ViewMenuVisible = false;
                        } else if (mouseY > 80 && mouseY < 120) {
                            isDarkMode = true;  // Select Dark Mode
                            ViewMenuVisible = false;
                        }
                    }
                }
            }
        }

        // Set the background color based on the current mode
        if (isDarkMode) {
            SDL_SetRenderDrawColor(renderer, darkBackgroundColor.r, darkBackgroundColor.g, darkBackgroundColor.b, 255);
            textColor = darkTextColor;
        } else {
            SDL_SetRenderDrawColor(renderer, lightBackgroundColor.r, lightBackgroundColor.g, lightBackgroundColor.b, 255);
            textColor = lightTextColor;

        }

        SDL_RenderClear(renderer);  // Clear the screen

        // Render the "View" button
        SDL_Surface *viewSurface = TTF_RenderText_Blended(font, viewText.c_str(), textColor);
        SDL_Texture *viewTexture = SDL_CreateTextureFromSurface(renderer, viewSurface);
        SDL_Rect viewRect = {60, 10, viewSurface->w, viewSurface->h};
        SDL_RenderCopy(renderer, viewTexture, nullptr, &viewRect);
        SDL_FreeSurface(viewSurface);
        SDL_DestroyTexture(viewTexture);
        // Render the "File" button
        SDL_Surface *fileSurface = TTF_RenderText_Blended(font, FileText.c_str(), textColor);
        SDL_Texture *fileTexture = SDL_CreateTextureFromSurface(renderer, fileSurface);
        SDL_Rect fileRect = {10, 10, fileSurface->w, fileSurface->h};
        SDL_RenderCopy(renderer, fileTexture, nullptr, &fileRect);
        SDL_FreeSurface(fileSurface);
        SDL_DestroyTexture(fileTexture);
        // Render the "File" button
        SDL_Surface *editSurface = TTF_RenderText_Blended(font, editText.c_str(), textColor);
        SDL_Texture *editTexture = SDL_CreateTextureFromSurface(renderer, editSurface);
        SDL_Rect editRect = {110, 10, editSurface->w, editSurface->h};
        SDL_RenderCopy(renderer, editTexture, nullptr, &editRect);
        SDL_FreeSurface(editSurface);
        SDL_DestroyTexture(editTexture);
        int y = 50-scrollOffset; // Start rendering based on the scroll offset

        for (size_t i = 0; i < lines.size(); ++i) {
            if (y + LINE_HEIGHT > 0 && y < SCREEN_HEIGHT) { // Render only visible lines
                if (lines[i].empty()) {
                    lines[i] = " "; // Show cursor on the current line
                }
                SDL_Surface *textSurface = TTF_RenderText_Blended(font, lines[i].c_str(), textColor);
                SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                int textWidth = textSurface->w;
                int textHeight = textSurface->h;
                SDL_Rect renderQuad = {10, y, textWidth, textHeight};

                SDL_FreeSurface(textSurface);

                SDL_RenderCopy(renderer, textTexture, nullptr, &renderQuad);
                SDL_DestroyTexture(textTexture);

                // Render cursor if this is the current line
                if (i == currentLine && cursorVisible) {
                    int cursorX = 0;
                    if (cursorPos > 0) {
                        TTF_SizeText(font, lines[i].substr(0, cursorPos).c_str(), &cursorX, nullptr);
                    }
                    cursorX += 10; // Add padding for the left margin
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderDrawLine(renderer, cursorX, y, cursorX, y + LINE_HEIGHT);
                }
            }
            y += LINE_HEIGHT; // Move to the next line

            // If the menu is visible, render the menu options
            if (ViewMenuVisible) {
                // Render Light Mode option
                SDL_Surface *lightModeSurface = TTF_RenderText_Blended(font, lightModeText.c_str(), textColor);
                SDL_Texture *lightModeTexture = SDL_CreateTextureFromSurface(renderer, lightModeSurface);
                SDL_Rect lightModeRect = {100, 40, lightModeSurface->w, lightModeSurface->h};
                SDL_RenderCopy(renderer, lightModeTexture, nullptr, &lightModeRect);
                SDL_FreeSurface(lightModeSurface);
                SDL_DestroyTexture(lightModeTexture);

                // Render Dark Mode option
                SDL_Surface *darkModeSurface = TTF_RenderText_Blended(font, darkModeText.c_str(), textColor);
                SDL_Texture *darkModeTexture = SDL_CreateTextureFromSurface(renderer, darkModeSurface);
                SDL_Rect darkModeRect = {100, 80, darkModeSurface->w, darkModeSurface->h};
                SDL_RenderCopy(renderer, darkModeTexture, nullptr, &darkModeRect);
                SDL_FreeSurface(darkModeSurface);
                SDL_DestroyTexture(darkModeTexture);
            }
        }

        // Present the updated rendering on the screen
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
