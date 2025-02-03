#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfx.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <map>
#include <set>
#include <regex>
#include <algorithm>
#include <unordered_set>
#include <fstream>

using namespace std;

// Screen dimensions
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 640;
const int ERROR_PANEL_HEIGHT = 100;  // Height of the error panel at the bottom


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
        scrollOffset = min(scrollOffset, contentHeight - SCREEN_HEIGHT);
    } else {
        scrollOffset = 0; // No scrolling needed if content fits
    }
}

// Define colors for Light Mode and Dark Mode
SDL_Color lightBackgroundColor = {220, 220, 220, 255};  // White
SDL_Color lightTextColor       = {0, 0, 0, 255};        // Black
SDL_Color darkBackgroundColor  = {20, 20, 20, 255};        // Black
SDL_Color darkTextColor        = {255, 255, 255, 255};  // White


//----------------------
//Syntax Highlighting
//----------------------

// Light Mode colors
map<string, SDL_Color> lightModeColors = {
        {"keyword", {0, 51, 102, 255}},      // Dark Blue
        {"datatype", {0, 128, 128, 255}},   // Teal
        {"function", {255, 140, 0, 255}},   // Dark Orange
        {"variable", {139, 0, 0, 255}},     // Dark Red
        {"string", {0, 100, 0, 255}},       // Dark Green
        {"char", {128, 0, 128, 255}},       // Purple
        {"number", {128, 128, 128, 255}},   // Gray
        {"comment", {0, 139, 139, 255}},    // Turquoise Blue
        {"preprocessor", {128, 0, 0, 255}}, // Maroon
        {"operator", {184, 134, 11, 255}},  // Dark Goldenrod
        {"bracket", {184, 134, 11, 255}},   // Dark Goldenrod
        {"normal", {0, 0, 0, 255}}
};

// Dark Mode colors
map<string, SDL_Color> darkModeColors = {
        {"keyword", {198, 120, 221, 255}},  // Purple
        {"datatype", {224, 108, 117, 255}}, // Red
        {"function", {97, 175, 254, 255}},  // Light Blue
        {"variable", {229, 192, 123, 255}}, // Yellow
        {"string", {152, 195, 121, 255}},   // Green
        {"char", {152, 195, 121, 255}},     // Green
        {"number", {209, 154, 102, 255}},   // Orange
        {"comment", {92, 99, 112, 255}},    // Gray
        {"preprocessor", {86, 182, 194, 255}}, // Cyan
        {"operator", {213, 94, 0, 255}},    // Dark Orange
        {"bracket", {171, 178, 191, 255}},   // Light Gray
        {"normal", {255, 255, 255, 255}}
};

bool isDarkMode = false;  // Default mode: Light Mode
map<string, SDL_Color>& getCurrentColors() {
    if (isDarkMode) {
        return darkModeColors;
    } else {
        return lightModeColors;
    }
}

unordered_set <string> operators ={"+","-","*","/","%","="};

SDL_Color applyHighlightColor(const string& word, bool &comment) {
    regex keywordRegex("\\b(if|else|while|for|switch|case|return|class|struct|namespace|using)\\b");
    if (regex_match(word, keywordRegex)) {
        return getCurrentColors()["keyword"];
    }

    regex datatypeRegex("\\b(int|float|double|char|bool|void|string)\\b");
    if (regex_match(word, datatypeRegex)) {
        return getCurrentColors()["datatype"];
    }

    regex functionRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
    if (regex_match(word, functionRegex)) {
        return getCurrentColors()["function"];
    }

    regex stringRegex("\"([^\"]*)\"");  // Match strings
    if (regex_match(word, stringRegex)) {
        return getCurrentColors()["string"];
    }

    regex numberRegex("\\b\\d+(\\.\\d+)?\\b");
    if (regex_match(word, numberRegex)) {
        return getCurrentColors()["number"];
    }

    regex commentRegex("//.*");  // Match comments
    if (regex_match(word, commentRegex)) {
        comment = true;
        return getCurrentColors()["comment"];
    }

    if(operators.find(word) != operators.end())
    {
        return getCurrentColors()["operator"];
    }

    regex bracketRegex("[\\[\\](){}]");
    if (regex_match(word, bracketRegex)) {
        return getCurrentColors()["bracket"];
    }

    return getCurrentColors()["normal"];
}

int getSpaceWidth(TTF_Font* codefont) {
    int spaceWidth;
    TTF_SizeText(codefont, " ", &spaceWidth, nullptr);
    return spaceWidth;
}


//----------------------------
//Poshtibani az ketabkhaneha
//----------------------------

map<string, vector<string>> libraryFunctions = {
        {"<iostream>", {"cout", "cin", "endl", "getline"}},
        {"<cmath>", {"sqrt", "pow", "sin", "cos", "tan", "abs", "log", "exp", "log10", "floor", "ceil"}},
        {"<vector>", {"vector", "push_back", "pop_back", "size"}},
        {"<algorithm>", {"sort", "max_element", "min_element", "reverse"}},
        {"<map>", {"map", "insert", "erase", "find", "size"}},
        {"<set>", {"set", "insert", "erase", "find", "size"}},
        {"<fstream>", {"ofstream", "ifstream", "open", "close"}},
};

vector <string> extractIncludedLibraries(const string &code) {
    vector <string> includedLibraries;
    regex includeRegex("#include\\s*<([^>]+)>");
    smatch match;
    string remainingCode = code;
    while (regex_search(remainingCode, match, includeRegex)) {
        includedLibraries.push_back("<" + match[1].str() + ">"); // Add the library in angle brackets
        remainingCode = match.suffix().str();
    }
    return includedLibraries;
}


vector <string> getAvailableFunctions(const vector<string> &includedLibraries) {
    vector <string> availableFunctions;
    for (const auto &lib: includedLibraries)
    {
        if (libraryFunctions.find(lib) != libraryFunctions.end()) {
            availableFunctions.insert(availableFunctions.end(),
                                      libraryFunctions[lib].begin(),
                                      libraryFunctions[lib].end());
        }
    }
    return availableFunctions;
}

void updateFunctionList(const string& code, vector<string>& currentIncludes, vector<string>& availableFunctions) {
    vector<string> newIncludes = extractIncludedLibraries(code);  // Extract updated include libraries

    // Find newly added includes
    for (const auto& lib : newIncludes) {
        if (find(currentIncludes.begin(), currentIncludes.end(), lib) == currentIncludes.end()) {
            // If the library was not previously included, add its functions
            if (libraryFunctions.find(lib) != libraryFunctions.end()) {
                availableFunctions.insert(availableFunctions.end(),
                                          libraryFunctions[lib].begin(),
                                          libraryFunctions[lib].end());
            }
        }
    }

    // Find removed includes
    for (const auto& oldLib : currentIncludes) {
        if (find(newIncludes.begin(), newIncludes.end(), oldLib) == newIncludes.end()) {
            // If the library has been removed, delete its functions from the available list
            if (libraryFunctions.find(oldLib) != libraryFunctions.end()) {
                for (const auto& func : libraryFunctions[oldLib]) {
                    auto it = find(availableFunctions.begin(), availableFunctions.end(), func);
                    if (it != availableFunctions.end()) {
                        availableFunctions.erase(it);  // Remove function from the list
                    }
                }
            }
        }
    }

    // Update the current list of includes
    currentIncludes = newIncludes;
}

bool isLibraryIncluded(const string& functionName, const vector<string>& currentIncludes) {
    for (const auto& lib : libraryFunctions) {
        if (find(lib.second.begin(), lib.second.end(), functionName) != lib.second.end()) {
            // Check if the corresponding library is included in currentIncludes
            if (find(currentIncludes.begin(), currentIncludes.end(), lib.first) != currentIncludes.end()) {
                return true; // The corresponding library is included
            } else {
                return false; // The corresponding library is not included
            }
        }
    }
    return true; // If the function is not found, assume no missing library
}



vector<string> extractUsedFunctions(const string& code) {
    vector<string> usedFunctions;
    regex functionRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\("); // Match function names
    smatch match;
    string remainingCode = code;

    while (regex_search(remainingCode, match, functionRegex)) {
        usedFunctions.push_back(match[1].str()); // Extract function name
        remainingCode = match.suffix().str();    // Move to the next match
    }

    return usedFunctions;
}

vector<string> findUndefinedFunctions(const vector<string>& usedFunctions, const vector<string>& availableFunctions) {
    vector<string> undefinedFunctions;

    for (const auto& func : usedFunctions) {
        if (find(availableFunctions.begin(), availableFunctions.end(), func) == availableFunctions.end()) {
            undefinedFunctions.push_back(func); // Function is not in available list
        }
    }

    return undefinedFunctions;
}

vector<string> suggestMissingIncludes(const vector<string>& undefinedFunctions) {
    vector<string> missingIncludes;

    for (const auto& func : undefinedFunctions) {
        for (const auto& lib : libraryFunctions) {
            if (find(lib.second.begin(), lib.second.end(), func) != lib.second.end()) {
                missingIncludes.push_back(lib.first); // Suggest including this library
                break;
            }
        }
    }

    return missingIncludes;
}

vector<string> collectedErrors;  // Vector to store the error messages
// Update the checkCodeErrors function to use isLibraryIncluded
void checkCodeErrors(const string& code, const vector<string>& availableFunctions, vector<string>& currentIncludes) {
    vector<string> usedFunctions = extractUsedFunctions(code);
    vector<string> undefinedFunctions = findUndefinedFunctions(usedFunctions, availableFunctions);
    vector<string> missingIncludes = suggestMissingIncludes(undefinedFunctions);

    collectedErrors.clear(); // Clear previous errors

    // Check if required libraries are included for each used function
    for (const auto& func : usedFunctions) {
        if (!isLibraryIncluded(func, currentIncludes)) {
            collectedErrors.push_back("Error: Missing include for function '" + func + "'! You need to include the corresponding library.");
        }
    }

    // Add missing includes suggestions
    for (const auto& inc : missingIncludes) {
        collectedErrors.push_back("Hint: You may need to add `#include " + inc + "` to use these functions.");
    }
}


//------------------
//---Save Project---
//------------------

struct ProjectInfo {
    string name;
    string filePath;
    SDL_Rect rect;
};

vector <ProjectInfo> savedProjects;
bool saveProject(const string &projectName, const vector<string> &lines) {
    string filePath = projectName + ".cpp";

    ofstream outputFile(filePath, ios::trunc);
    if (!outputFile) return false;

    for (const string& line : lines)
        outputFile << line << "\n";

    outputFile.close();

    ifstream inFile("projects.txt");
    set<string> existingProjects;
    string line;
    while(getline(inFile, line)) {
        existingProjects.insert(line);
    }
    inFile.close();

    if(existingProjects.find(projectName) == existingProjects.end()) {
        ofstream outFile("projects.txt", ios::app);
        outFile << projectName << "\n";
        outFile.close();
    }

    return true;
}

bool showSaveDialog(string& projectName, Uint8 textCol) {
    const int WINDOW_WIDTH = 300;
    const int WINDOW_HEIGHT = 150;

    SDL_Window* window = SDL_CreateWindow("Save Project", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "Save Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        return false;
    }

    TTF_Font* font = TTF_OpenFont("C:\\Windows\\Fonts\\Calibri.ttf", 20); // Use a fallback font
    if (!font) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }
    TTF_Font* font2 = TTF_OpenFont("C:\\Windows\\Fonts\\Calibri.ttf", 18); // Use a fallback font

    bool dialogActive = true;
    SDL_Event e;
    projectName.clear();
    string inputText;

    // UI element coordinates (relative to window size)
    SDL_Rect dialogBox = {10, 10, WINDOW_WIDTH - 20, WINDOW_HEIGHT - 20};
    SDL_Rect inputBox = {20, 50, WINDOW_WIDTH - 40, 30};
    SDL_Rect cancelButton = {50, 90, 80, 30};
    SDL_Rect saveButton = {170, 90, 80, 30};
    SDL_Color textColor = {textCol, textCol, textCol, 255};

    // Render "Enter Project Name:" text
    SDL_Surface* messageSurface = TTF_RenderText_Blended(font, "Enter Project Name:", textColor);
    if (!messageSurface) {
        cerr << "Failed to create message surface! TTF_Error: " << TTF_GetError() << endl;
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);
    SDL_Rect messageRect = {20, 20, messageSurface->w, messageSurface->h};
    SDL_FreeSurface(messageSurface);

    SDL_Color buttonColor = {255, 255, 255, 255};

    SDL_Surface* saveSurface = TTF_RenderText_Blended(font2, "Save", buttonColor);
    SDL_Texture* saveTexture = SDL_CreateTextureFromSurface(renderer, saveSurface);
    SDL_Rect saveRect = {193, 96, saveSurface->w, saveSurface->h};
    SDL_FreeSurface(saveSurface);

    SDL_Surface* cancelSurface = TTF_RenderText_Blended(font2, "Cancel", buttonColor);
    SDL_Texture* cancelTexture = SDL_CreateTextureFromSurface(renderer, cancelSurface);
    SDL_Rect cancelRect = {65, 96, cancelSurface->w, cancelSurface->h};
    SDL_FreeSurface(cancelSurface);

    while (dialogActive) {
        // Clear background
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderClear(renderer);

        // Draw dialog box
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_RenderFillRect(renderer, &dialogBox);

        // Draw input box
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &inputBox);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &inputBox);

        // Draw buttons
        SDL_SetRenderDrawColor(renderer, 0, 128, 0, 255);
        SDL_RenderFillRect(renderer, &saveButton);
        SDL_RenderCopy(renderer, saveTexture, nullptr, &saveRect);

        SDL_SetRenderDrawColor(renderer, 128, 0, 0, 255);
        SDL_RenderFillRect(renderer, &cancelButton);
        SDL_RenderCopy(renderer, cancelTexture, nullptr, &cancelRect);

        // Render text
        SDL_RenderCopy(renderer, messageTexture, nullptr, &messageRect);

        // Render input text
        if (!inputText.empty()) {
            SDL_Surface* inputSurface = TTF_RenderText_Blended(font, inputText.c_str(), textColor);
            SDL_Texture* inputTexture = SDL_CreateTextureFromSurface(renderer, inputSurface);
            SDL_Rect inputRect = {inputBox.x + 5, inputBox.y + 5, inputSurface->w, inputSurface->h};
            SDL_RenderCopy(renderer, inputTexture, nullptr, &inputRect);
            SDL_FreeSurface(inputSurface);
            SDL_DestroyTexture(inputTexture);
        }

        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                dialogActive = false;
                projectName.clear();
                return false;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RETURN) {
                    projectName = inputText;
                    dialogActive = false;
                } else if (e.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
                    inputText.pop_back();
                }
            } else if (e.type == SDL_TEXTINPUT) {
                inputText += e.text.text;
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;

                if (mouseX >= saveButton.x && mouseX <= saveButton.x + saveButton.w &&
                    mouseY >= saveButton.y && mouseY <= saveButton.y + saveButton.h) {
                    projectName = inputText;
                    dialogActive = false;
                } else if (mouseX >= cancelButton.x && mouseX <= cancelButton.x + cancelButton.w &&
                           mouseY >= cancelButton.y && mouseY <= cancelButton.y + cancelButton.h) {
                    dialogActive = false;
                    projectName.clear();
                }
            }
        }

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    SDL_DestroyTexture(messageTexture);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return !projectName.empty();
}

void loadProjectsList()
{
    ifstream file("projects.txt");
    string line;
    savedProjects.clear();
    while(getline(file, line))
    {
        ProjectInfo project;
        project.name = line;
        project.filePath = line + ".cpp";
        project.rect = {0, 0, 0, 0};
        savedProjects.push_back(project);
    }
    file.close();
}


int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return -1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << endl;
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
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // Load font
    TTF_Font *font = TTF_OpenFont(R"(C:\Windows\Fonts\Calibri.ttf)", 18); // adjust path as needed
    TTF_Font *codefont = TTF_OpenFont(R"(C:\Windows\Fonts\Consola.ttf)", 16);
    if (!font) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    if (!codefont) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    vector<string> currentIncludes;  // Stores currently included libraries
    vector<string> availableFunctions;  // Stores available functions from included libraries


    // Basic editor state
    SDL_Color textColor = {0, 0, 0, 255};     // default black text
    vector<string> lines = {""};    // at least one empty line
    int currentLine = 0;
    int cursorPos = 1;    // note: your code starts it at 1
    int scrollOffset = 0;
    const int LINE_HEIGHT = TTF_FontHeight(font);

    // Menu / UI strings
    string viewText       = "View";
    string lightModeText  = "Light Mode";
    string darkModeText   = "Dark Mode";
    string newprojectText = "New Project";
    string saveprojectText = "Save Project";
    string exitText = "Exit";
    string undoText = "Undo";
    string redoText = "Redo";

    bool ViewMenuVisible  = false;
    bool FileMenuVisible  = false;
    bool EditMenuVisible  = false;

    // Timer for cursor blinking
    Uint32 lastCursorToggle = SDL_GetTicks();
    bool cursorVisible      = true;
    const Uint32 CURSOR_BLINK_INTERVAL = 500; // ms
    bool quit = false;
    SDL_Event e;
    loadProjectsList();

    while (!quit) {
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // Render text
        SDL_Color textColor;

        // Capture the current code from the editor as a single string
        string currentCode;
        for (const auto& line : lines) {
            currentCode += line + "\n";
        }

        // Update included libraries and available functions
        updateFunctionList(currentCode, currentIncludes, availableFunctions);

        // Extract functions used in the code
        vector<string> usedFunctions = extractUsedFunctions(currentCode);

        // Find functions that are used but not defined
        vector<string> undefinedFunctions = findUndefinedFunctions(usedFunctions, availableFunctions);

        // Suggest missing #include directives
        vector<string> missingIncludes = suggestMissingIncludes(undefinedFunctions);


        Uint32 currentTime = SDL_GetTicks();
        if (currentTime > lastCursorToggle + CURSOR_BLINK_INTERVAL) {
            cursorVisible = !cursorVisible;
            lastCursorToggle = currentTime;
        }

        //Saving Project
        bool saveProjectButtonPressed = false;
        string projectNameInput = "";

        // Event loop
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
                    // Ensure cursorPos is within the valid range
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
                }
                else if (e.key.keysym.sym == SDLK_TAB) {
                    // Add spaces for tab
                    lines[currentLine].insert(cursorPos, "    ");
                    cursorPos += 4;
                } else if (e.key.keysym.sym == SDLK_LEFT) {
                    // Move cursor left
                    if (cursorPos > 1) {
                        cursorPos--;
                    } else if (currentLine > 0) {
                        currentLine--;
                        cursorPos = lines[currentLine].size();
                    }
                } else if (e.key.keysym.sym == SDLK_RIGHT) {
                    // Move cursor right
                    if (cursorPos < lines[currentLine].size()) {
                        cursorPos++;
                    } else if (currentLine < lines.size() - 1) {
                        currentLine++;
                        cursorPos = 0;
                    }
                } else if (e.key.keysym.sym == SDLK_UP) {
                    // Move cursor up
                    if (currentLine > 0) {
                        currentLine--;
                        cursorPos = std::min(cursorPos, (int)lines[currentLine].size());
                        ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                    }
                } else if (e.key.keysym.sym == SDLK_DOWN) {
                    if (currentLine < lines.size() - 1) {
                        currentLine++;
                        cursorPos = std::min(cursorPos, (int)lines[currentLine].size());
                        ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                if (e.text.text) {
                    if (e.text.text[0] == ' ')
                        lines[currentLine].insert(cursorPos, " ");
                    else
                        lines[currentLine].insert(cursorPos, e.text.text);
                    cursorPos += strlen(e.text.text);
                    ensureLastLineVisible(currentLine, scrollOffset, SCREEN_HEIGHT, LINE_HEIGHT, lines.size());
                }
            }  else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;

                if (!FileMenuVisible)
                {
                    for (auto& project : savedProjects) {
                        if (mouseX >= project.rect.x &&
                            mouseX <= project.rect.x + project.rect.w &&
                            mouseY >= project.rect.y &&
                            mouseY <= project.rect.y + project.rect.h) {

                            ifstream file(project.filePath);
                            if (file.is_open()) {
                                lines.clear();
                                string line;
                                while (getline(file, line)) {
                                    lines.push_back(line);
                                }
                                file.close();
                                currentLine = 0;
                                cursorPos = 0;
                            } else {
                                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                                         ("Failed to open project: " + project.name).c_str(), window);
                            }

                        }
                    }
                }

                // Click on "View" button region?
                if (mouseX >= 110 && mouseX <= 110 + 50 && mouseY < 40) {
                    ViewMenuVisible = !ViewMenuVisible;  // Toggle the visibility of the menu
                }

                //file button
                if (mouseX >= 10 && mouseX <= 50 && mouseY < 40) {
                    FileMenuVisible = !FileMenuVisible;  // Toggle the visibility of the menu
                }

                if (FileMenuVisible)
                {
                    if (mouseX >= 20 && mouseX <= 200)
                    {
                        if (mouseY <= 120 && mouseY >= 80)
                        {
                            saveProjectButtonPressed = true;
                            projectNameInput = "";
                        }
                    }
                }

                // If the View menu is open, check clicks on Light/Dark
                if (ViewMenuVisible) {
                    if (mouseX > 100 + 50 && mouseX < 250 + 50) {
                        if (mouseY > 40 && mouseY < 80) {
                            isDarkMode = false;  // Light Mode
                            ViewMenuVisible = false;
                        } else if (mouseY > 80 && mouseY < 120) {
                            isDarkMode = true;   // Dark Mode
                            ViewMenuVisible = false;
                        }
                    }
                }
            }
        }

        Uint8 navarcolor, textCol;
        // Set the background color based on the current mode
        if (isDarkMode) {
            SDL_SetRenderDrawColor(renderer, darkBackgroundColor.r, darkBackgroundColor.g, darkBackgroundColor.b, 255);
            textColor = darkTextColor;
            navarcolor = 50;
            textCol = 255;
        } else {
            SDL_SetRenderDrawColor(renderer, lightBackgroundColor.r, lightBackgroundColor.g, lightBackgroundColor.b, 255);
            textColor = lightTextColor;
            navarcolor = 180;
            textCol = 0;
        }
        SDL_RenderClear(renderer);

        //Navare Bala
        for (int i = 0; i < 35; i++)
            lineRGBA(renderer, 0, i, SCREEN_WIDTH, i, navarcolor, navarcolor, navarcolor, 255);


        int y = -scrollOffset + 50; // Start rendering based on the scroll offset

        for (size_t i = 0; i < lines.size(); ++i) {
            bool comment = false;
            if (y + LINE_HEIGHT > 0 && y < SCREEN_HEIGHT) { // Render only visible lines
                if (lines[i].empty()) {
                    lines[i] = " "; // Show cursor on the current line
                }

                // Split line into words
                vector<string> words;
                stringstream ss(lines[i]);
                string word;
                while (ss >> word) {
                    words.push_back(word);
                }

                int currentX = 250;  // Starting position for text rendering

                // Render each word with its corresponding color
                for (const string& word : words) {
                    SDL_Color syntaxColor = applyHighlightColor(word, comment); // Get color based on word type

                    if (comment) syntaxColor = getCurrentColors()["comment"];

                    // Render the word
                    SDL_Surface* textSurface = TTF_RenderText_Blended(codefont, word.c_str(), syntaxColor);
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                    int textWidth = textSurface->w;
                    int textHeight = textSurface->h;
                    SDL_Rect renderQuad = {currentX, y, textWidth, textHeight};

                    SDL_FreeSurface(textSurface);

                    SDL_RenderCopy(renderer, textTexture, nullptr, &renderQuad);
                    SDL_DestroyTexture(textTexture);

                    currentX += textWidth + getSpaceWidth(codefont);  // Move the x-coordinate for the next word
                }

                // Render the cursor if this is the current line
                if (i == currentLine) {
                    int cursorX = 0;
                    if (cursorPos > 0) {
                        TTF_SizeText(codefont, lines[i].substr(0, cursorPos).c_str(), &cursorX, nullptr);
                    }
                    if (i == 0){
                        cursorX += 240;
                    }
                    else
                        cursorX += 250;

                    if (!isDarkMode)
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    else
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderDrawLine(renderer, cursorX, y, cursorX, y + LINE_HEIGHT);
                }
            }
            y += LINE_HEIGHT; // Move to the next line
        }

        //--------------------
        //---Projects Panel---
        //--------------------

        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);  // Red color for the error panel
        SDL_Rect ProjectPanel = {0, 35, 200, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &ProjectPanel);

        int startY = 50;
        int itemHeight = 30;

        for (size_t i = 0; i < savedProjects.size(); i++)
        {
            SDL_Surface* surface = TTF_RenderText_Blended(font, savedProjects[i].name.c_str(), {255, 255, 255, 255});
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

            SDL_Rect destRect = {
                    20,
                    startY + (itemHeight * int(i)),
                    surface->w,
                    surface->h
            };

            savedProjects[i].rect = {
                    15,
                    startY + (itemHeight * int(i)) - 5,
                    surface->w + 10,
                    surface->h + 10
            };

            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            SDL_RenderFillRect(renderer, &savedProjects[i].rect);

            SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
            SDL_RenderDrawRect(renderer, &savedProjects[i].rect);

            SDL_RenderCopy(renderer, texture, NULL, &destRect);

            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }



        //-----------------
        //---Error Panel---
        //-----------------
        checkCodeErrors(currentCode, availableFunctions, currentIncludes);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);  // Red color for the error panel
        SDL_Rect errorPanel = {200, SCREEN_HEIGHT - ERROR_PANEL_HEIGHT, SCREEN_WIDTH, ERROR_PANEL_HEIGHT};
        SDL_RenderFillRect(renderer, &errorPanel);
        int errorY = SCREEN_HEIGHT - ERROR_PANEL_HEIGHT + 10;
        SDL_Color errorColor = {150, 0, 0, 255};
        for (const string &errorMessage: collectedErrors) {
            // Create a surface and texture for the error message
            SDL_Surface* errorSurface = TTF_RenderText_Blended(font, errorMessage.c_str(), errorColor);
            SDL_Texture* errorTexture = SDL_CreateTextureFromSurface(renderer, errorSurface);
            int textWidth = errorSurface->w;
            int textHeight = errorSurface->h;
            SDL_Rect errorRect = {10, errorY, textWidth, textHeight};  // Position the error text

            // Render the error text on the panel
            SDL_RenderCopy(renderer, errorTexture, nullptr, &errorRect);

            // Move down for the next error
            errorY += textHeight + 5;

            // Clean up
            SDL_FreeSurface(errorSurface);
            SDL_DestroyTexture(errorTexture);
        }


        // --- Render top menu items ---
        // "File" (just as an example)
        {
            SDL_Surface *fileSurface = TTF_RenderText_Blended(font, "File", textColor);
            SDL_Texture *fileTexture = SDL_CreateTextureFromSurface(renderer, fileSurface);
            SDL_Rect fileRect = {10, 10, fileSurface->w, fileSurface->h};
            SDL_RenderCopy(renderer, fileTexture, nullptr, &fileRect);
            SDL_FreeSurface(fileSurface);
            SDL_DestroyTexture(fileTexture);
        }

        if (saveProjectButtonPressed)
        {
            FileMenuVisible = false;
            string projectName;
            if (showSaveDialog(projectName, textCol)) {
                if (saveProject(projectName, lines)) {
                        ProjectInfo newProject;
                        newProject.name = projectName;
                        newProject.filePath = projectName + ".cpp";
                        newProject.rect = {0, 0, 0, 0};
                        savedProjects.push_back(newProject);
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                             "Success",
                                             "Project saved successfully",
                                             window);
                    loadProjectsList();
                } else {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Warning", "Error: Project with this name already exists.", window);
                }
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Info", "Save operation was canceled.", window);
            }
            saveProjectButtonPressed = false;
        }

        // "Edit" (just as an example)
        {
            SDL_Surface *editSurface = TTF_RenderText_Blended(font, "Edit", textColor);
            SDL_Texture *editTexture = SDL_CreateTextureFromSurface(renderer, editSurface);
            SDL_Rect editRect = {60, 10, editSurface->w, editSurface->h};
            SDL_RenderCopy(renderer, editTexture, nullptr, &editRect);
            SDL_FreeSurface(editSurface);
            SDL_DestroyTexture(editTexture);
        }

        // "View"
        {
            SDL_Surface *viewSurface = TTF_RenderText_Blended(font, viewText.c_str(), textColor);
            SDL_Texture *viewTexture = SDL_CreateTextureFromSurface(renderer, viewSurface);
            SDL_Rect viewRect = {60 + 50, 10, viewSurface->w, viewSurface->h};
            SDL_RenderCopy(renderer, viewTexture, nullptr, &viewRect);
            SDL_FreeSurface(viewSurface);
            SDL_DestroyTexture(viewTexture);
        }

        // "debug and compile" (just as an example)
        {
            SDL_Surface *fileSurface = TTF_RenderText_Blended(font, "Debug & Compile", textColor);
            SDL_Texture *fileTexture = SDL_CreateTextureFromSurface(renderer, fileSurface);
            SDL_Rect fileRect = {160, 10, fileSurface->w, fileSurface->h};
            SDL_RenderCopy(renderer, fileTexture, nullptr, &fileRect);
            SDL_FreeSurface(fileSurface);
            SDL_DestroyTexture(fileTexture);
        }

        // "run" (just as an example)
        {
            SDL_Surface *fileSurface = TTF_RenderText_Blended(font, "Run", textColor);
            SDL_Texture *fileTexture = SDL_CreateTextureFromSurface(renderer, fileSurface);
            SDL_Rect fileRect = {300, 10, fileSurface->w, fileSurface->h};
            SDL_RenderCopy(renderer, fileTexture, nullptr, &fileRect);
            SDL_FreeSurface(fileSurface);
            SDL_DestroyTexture(fileTexture);
        }

        // If the menu is visible, render the menu options for View
        if (ViewMenuVisible) {
            SDL_SetRenderDrawColor(renderer, navarcolor, navarcolor, navarcolor, 255);  // Light gray color
            SDL_Rect grayBackgroundRect = {140, 35, 120, 80}; // Position it below the top menu
            SDL_RenderFillRect(renderer, &grayBackgroundRect);

            SDL_Surface *lightModeSurface = TTF_RenderText_Blended(font, lightModeText.c_str(), textColor);
            SDL_Texture *lightModeTexture = SDL_CreateTextureFromSurface(renderer, lightModeSurface);
            SDL_Rect lightModeRect = {150, 40, lightModeSurface->w, lightModeSurface->h};
            SDL_RenderCopy(renderer, lightModeTexture, nullptr, &lightModeRect);
            SDL_FreeSurface(lightModeSurface);
            SDL_DestroyTexture(lightModeTexture);

            SDL_Surface *darkModeSurface = TTF_RenderText_Blended(font, darkModeText.c_str(), textColor);
            SDL_Texture *darkModeTexture = SDL_CreateTextureFromSurface(renderer, darkModeSurface);
            SDL_Rect darkModeRect = {150, 80, darkModeSurface->w, darkModeSurface->h};
            SDL_RenderCopy(renderer, darkModeTexture, nullptr, &darkModeRect);
            SDL_FreeSurface(darkModeSurface);
            SDL_DestroyTexture(darkModeTexture);

        }

        //file menu
        if (FileMenuVisible) {
            SDL_SetRenderDrawColor(renderer, navarcolor, navarcolor, navarcolor, 255);  // Light gray color
            SDL_Rect grayBackgroundRect = {10, 35, 120, 120}; // Position it below the top menu
            SDL_RenderFillRect(renderer, &grayBackgroundRect);

            SDL_Surface *newprojectSurface= TTF_RenderText_Blended(font, newprojectText.c_str(), textColor);
            SDL_Texture *newprojectTexture = SDL_CreateTextureFromSurface(renderer, newprojectSurface);
            SDL_Rect newprojectRect = {20, 40, newprojectSurface->w, newprojectSurface->h};
            SDL_RenderCopy(renderer, newprojectTexture, nullptr, &newprojectRect);
            SDL_FreeSurface(newprojectSurface);
            SDL_DestroyTexture(newprojectTexture);

            SDL_Surface *saveprojectSurface = TTF_RenderText_Blended(font, saveprojectText.c_str(), textColor);
            SDL_Texture *saveprojectTexture = SDL_CreateTextureFromSurface(renderer, saveprojectSurface);
            SDL_Rect saveprojectRect = {20, 80, saveprojectSurface->w, saveprojectSurface->h};
            SDL_RenderCopy(renderer, saveprojectTexture, nullptr, &saveprojectRect);
            SDL_FreeSurface(saveprojectSurface);
            SDL_DestroyTexture(saveprojectTexture);

            SDL_Surface *exitSurface = TTF_RenderText_Blended(font, exitText.c_str(), textColor);
            SDL_Texture *exitTexture = SDL_CreateTextureFromSurface(renderer, exitSurface);
            SDL_Rect exitRect = {20, 120, exitSurface->w, exitSurface->h};
            SDL_RenderCopy(renderer, exitTexture, nullptr, &exitRect);
            SDL_FreeSurface(exitSurface);
            SDL_DestroyTexture(exitTexture);

        }



        // Present the updated rendering on the screen
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    TTF_CloseFont(font);
    TTF_CloseFont(codefont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
