#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>
#include <cctype>
#include <map>
#include <set>
#include <stack>
#include <regex>
#include <limits>
#include <algorithm>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>

using namespace std;

// Screen dimensions
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

// Layout of the main window
const int TOP_BAR_HEIGHT = 35;
const int PROJECT_PANEL_WIDTH = 200;
const int ERROR_PANEL_HEIGHT = 150;
const int CODE_TOP = 45;        // y of the first line of code
const int CODE_X = 250;         // x where the code starts (line numbers are before it)
const int CODE_BOTTOM = SCREEN_HEIGHT - ERROR_PANEL_HEIGHT;
const int MENU_WIDTH = 150;
const int MENU_ITEM_HEIGHT = 32;

const int TAB_SIZE = 4;
const int MAX_HISTORY = 300;    // how many undo steps we keep

// fonts (windows only for now)
const char* UI_FONT_PATH = "C:\\Windows\\Fonts\\calibri.ttf";
const char* CODE_FONT_PATH = "C:\\Windows\\Fonts\\consola.ttf";

int LINE_HEIGHT = 20;  // real value is set in main() after loading the font

// Define colors for Light Mode and Dark Mode
SDL_Color lightBackgroundColor = {220, 220, 220, 255};  // Light gray
SDL_Color lightTextColor       = {0, 0, 0, 255};        // Black
SDL_Color darkBackgroundColor  = {20, 20, 20, 255};     // Almost black
SDL_Color darkTextColor        = {255, 255, 255, 255};  // White


//----------------------
// Editor State
//----------------------

struct EditorState {
    vector<string> lines = {""};   // at least one empty line
    int currentLine = 0;
    int cursorPos = 0;
    int scrollOffset = 0;   // vertical scroll of the code (pixels)
    int scrollX = 0;        // horizontal scroll (pixels)

    bool selectionActive = false;
    int selectionStartLine = 0;
    int selectionStartPos = 0;

    string projectName;     // empty = not saved yet
    string projectPath;
};

// Ensure the current line is visible when adding new lines or moving the cursor
void ensureCursorVisible(EditorState &ed) {
    int viewHeight = CODE_BOTTOM - CODE_TOP;
    int cursorY = ed.currentLine * LINE_HEIGHT - ed.scrollOffset;
    if (cursorY < 0) {
        ed.scrollOffset = ed.currentLine * LINE_HEIGHT;
    } else if (cursorY + LINE_HEIGHT > viewHeight) {
        ed.scrollOffset = (ed.currentLine + 1) * LINE_HEIGHT - viewHeight;
    }

    int contentHeight = ed.lines.size() * LINE_HEIGHT;
    ed.scrollOffset = min(ed.scrollOffset, max(0, contentHeight - viewHeight));
    ed.scrollOffset = max(0, ed.scrollOffset);
}

void clampCursor(EditorState &ed) {
    if (ed.lines.empty()) ed.lines.push_back("");
    ed.currentLine = max(0, min(ed.currentLine, (int)ed.lines.size() - 1));
    ed.cursorPos = max(0, min(ed.cursorPos, (int)ed.lines[ed.currentLine].size()));
}

string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

bool isWordChar(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

// the whole code as one string (used for saving, compiling and to see if something changed)
string getCode(const vector<string> &lines) {
    string code;
    for (const auto &line : lines) {
        code += line + "\n";
    }
    return code;
}


//----------------------
//Syntax Highlighting
//----------------------

// Light Mode colors
map<string, SDL_Color> lightModeColors = {
        {"keyword", {0, 51, 102, 255}},      // Dark Blue
        {"datatype", {0, 128, 128, 255}},   // Teal
        {"function", {200, 100, 0, 255}},   // Dark Orange
        {"variable", {139, 0, 0, 255}},     // Dark Red
        {"string", {0, 100, 0, 255}},       // Dark Green
        {"char", {128, 0, 128, 255}},       // Purple
        {"number", {110, 110, 110, 255}},   // Gray
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

set<std::string> cppKeywords = {
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "return",
        "class", "struct", "public", "private", "protected", "static", "const", "virtual",
        "friend", "namespace", "using", "template", "typename", "try", "catch",
        "throw", "true", "false", "nullptr", "new", "delete", "this", "sizeof",
        "typedef", "enum", "union", "extern", "inline", "volatile", "explicit",
        "mutable", "operator", "asm", "goto", "default", "register", "consteval",
        "constinit", "constexpr", "concept", "requires", "co_await", "co_yield", "co_return",
        "import", "module", "export", "alignas", "alignof", "noexcept", "static_assert",
        "typeid", "const_cast", "dynamic_cast", "reinterpret_cast", "static_cast", "decltype",
        "override", "final"
};
bool isCppKeyword(const std::string& word) {
    return cppKeywords.count(word) > 0;
}

set<string> dataTypes = {
        "int", "float", "double", "char", "bool", "void", "string", "long", "short",
        "unsigned", "signed", "auto", "size_t", "vector", "map", "set", "pair"
};

struct Token {
    string text;
    string type;   // key of the color map
    int column;    // where the token starts in the line
};

// Splits one line into colored pieces. Spaces are skipped (we only need the column of each token)
// inBlockComment is shared between lines because /* */ can be on more than one line
vector<Token> tokenizeLine(const string &line, bool &inBlockComment, const unordered_set<string> &declaredVariables) {
    vector<Token> tokens;
    int n = line.size();
    int i = 0;
    const char* operatorChars = "+-*/%=<>!&|^~?";

    while (i < n) {
        int start = i;
        string type = "normal";

        if (inBlockComment) {
            size_t end = line.find("*/", i);
            if (end == string::npos) {
                i = n;
            } else {
                i = end + 2;
                inBlockComment = false;
            }
            type = "comment";
        } else if (line[i] == ' ') {
            i++;
            continue;
        } else if (line.compare(i, 2, "//") == 0) {
            i = n;
            type = "comment";
        } else if (line.compare(i, 2, "/*") == 0) {
            size_t end = line.find("*/", i + 2);
            if (end == string::npos) {
                i = n;
                inBlockComment = true;
            } else {
                i = end + 2;
            }
            type = "comment";
        } else if (line[i] == '#' && line.find_first_not_of(' ') == (size_t)i) {
            i = n;
            type = "preprocessor";
        } else if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            i++;
            while (i < n && line[i] != quote) {
                if (line[i] == '\\') i++;   // skip escaped chars like \"
                i++;
            }
            i = min(i + 1, n);
            type = (quote == '"') ? "string" : "char";
        } else if (isdigit((unsigned char)line[i])) {
            while (i < n && (isWordChar(line[i]) || line[i] == '.')) i++;
            type = "number";
        } else if (isWordChar(line[i])) {
            while (i < n && isWordChar(line[i])) i++;
            string word = line.substr(start, i - start);

            // look ahead, if a '(' comes after the word then it's a function
            int next = i;
            while (next < n && line[next] == ' ') next++;

            if (dataTypes.count(word)) type = "datatype";
            else if (isCppKeyword(word)) type = "keyword";
            else if (next < n && line[next] == '(') type = "function";
            else if (declaredVariables.count(word)) type = "variable";
        } else if (strchr("()[]{}", line[i])) {
            i++;
            type = "bracket";
        } else if (strchr(operatorChars, line[i])) {
            i++;
            while (i < n && strchr(operatorChars, line[i]) &&
                   line.compare(i, 2, "//") != 0 && line.compare(i, 2, "/*") != 0) {
                i++;
            }
            type = "operator";
        } else {
            i++;
        }

        tokens.push_back({line.substr(start, i - start), type, start});
    }
    return tokens;
}

int getSpaceWidth(TTF_Font* codefont) {
    int spaceWidth;
    TTF_SizeText(codefont, " ", &spaceWidth, nullptr);
    return spaceWidth;
}

// draws text and returns its width (so the caller can put something after it)
int renderText(SDL_Renderer* renderer, TTF_Font* font, const string &text, SDL_Color color, int x, int y) {
    if (text.empty()) return 0;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) return 0;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &rect);

    int width = surface->w;
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    return width;
}

void renderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const string &text, SDL_Color color, SDL_Rect box) {
    int w = 0, h = 0;
    TTF_SizeText(font, text.c_str(), &w, &h);
    renderText(renderer, font, text, color, box.x + (box.w - w) / 2, box.y + (box.h - h) / 2);
}

bool isInside(int x, int y, const SDL_Rect &rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}


//----------------------------
// Library Support
//----------------------------

// only the names that really need the include (member functions like push_back are not here)
map<string, vector<string>> libraryFunctions = {
        {"<iostream>", {"cout", "cin", "cerr", "endl"}},
        {"<cmath>", {"sqrt", "pow", "sin", "cos", "tan", "log", "exp", "log10", "floor", "ceil", "fabs"}},
        {"<vector>", {"vector"}},
        {"<algorithm>", {"sort", "max_element", "min_element", "reverse", "find_if", "count_if"}},
        {"<map>", {"map"}},
        {"<set>", {"set"}},
        {"<fstream>", {"ofstream", "ifstream", "fstream"}},
        {"<cstring>", {"strlen", "strcpy", "strcat", "strcmp"}},
};

vector<string> currentIncludes;  // updated every time the code is analyzed

vector <string> extractIncludedLibraries(const string &code) {
    vector <string> includedLibraries;
    regex includeRegex("#include\\s*<([^>]+)>");
    smatch match;
    string remainingCode = code;
    while (regex_search(remainingCode, match, includeRegex)) {
        includedLibraries.push_back("<" + match[1].str() + ">");
        remainingCode = match.suffix().str();
    }
    return includedLibraries;
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
const string PROJECTS_FILE = "projects.txt";

void loadProjectsList() {
    ifstream file(PROJECTS_FILE);
    string line;
    savedProjects.clear();
    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        istringstream iss(line);
        string name, path;
        if (getline(iss, name, ',') && getline(iss, path)) {
            ProjectInfo project;
            project.name = name;
            project.filePath = path;
            project.rect = {0, 0, 0, 0};
            savedProjects.push_back(project);
        }
    }
    file.close();
}

void saveProjectsList() {
    ofstream file(PROJECTS_FILE, ios::trunc);
    for (const auto &project : savedProjects) {
        file << project.name << "," << project.filePath << "\n";
    }
}

int findProject(const string &name) {
    for (size_t i = 0; i < savedProjects.size(); i++) {
        if (savedProjects[i].name == name) return i;
    }
    return -1;
}

// adds the project to projects.txt (or updates the path if the name is already there)
void addProjectToList(const string &name, const string &path) {
    int index = findProject(name);
    if (index != -1) {
        savedProjects[index].filePath = path;
    } else {
        ProjectInfo project;
        project.name = name;
        project.filePath = path;
        project.rect = {0, 0, 0, 0};
        savedProjects.push_back(project);
    }
    saveProjectsList();
}

bool isValidProjectName(const string &name) {
    if (name.empty()) return false;
    // these can't be in a windows file name (and the comma breaks projects.txt)
    return name.find_first_of("\\/:*?\"<>|,") == string::npos;
}

bool writeLinesToFile(const string &filePath, const vector<string> &lines) {
    ofstream outputFile(filePath, ios::trunc);
    if (!outputFile) return false;

    for (const string& line : lines)
        outputFile << line << "\n";

    outputFile.close();
    return true;
}

bool readFileToLines(const string &filePath, vector<string> &lines) {
    ifstream file(filePath);
    if (!file.is_open()) return false;

    lines.clear();
    string line;
    while (getline(file, line)) {
        string cleanLine;
        for (char c : line) {
            if (c == '\r') continue;
            if (c == '\t') cleanLine += string(TAB_SIZE, ' ');
            else if (c >= 32 && c <= 126) cleanLine += c;
            else cleanLine += '?';  // the font only supports ascii for now
        }
        lines.push_back(cleanLine);
    }
    if (lines.empty()) lines.push_back("");
    return true;
}

bool saveProject(const string &projectName, const vector<string> &lines) {
    string filePath = projectName + ".cpp";
    if (!writeLinesToFile(filePath, lines)) return false;
    addProjectToList(projectName, filePath);
    return true;
}

bool saveAsProject(const string &projectName, const vector<string> &lines, string path, string &filePath) {
    if (path.empty()) {
        filePath = projectName + ".cpp";  // current folder
    } else {
        if (path.back() != '\\' && path.back() != '/') path += "\\";
        filePath = path + projectName + ".cpp";
    }

    if (!writeLinesToFile(filePath, lines)) return false;
    addProjectToList(projectName, filePath);
    return true;
}

// Small popup window with some text boxes (used for Save, Save As and Go to Line)
bool showInputDialog(const string &title, const vector<string> &labels, vector<string> &values,
                     const string &okText, TTF_Font* font, bool numbersOnly = false) {
    const int WINDOW_WIDTH = 420;
    const int ROW_HEIGHT = 65;
    int buttonsY = 20 + labels.size() * ROW_HEIGHT;
    int windowHeight = buttonsY + 45;

    values.resize(labels.size());

    SDL_Window* window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, windowHeight, SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "Dialog window could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        SDL_DestroyWindow(window);
        return false;
    }

    // UI element coordinates (relative to window size)
    vector<SDL_Rect> boxes;
    for (size_t i = 0; i < labels.size(); i++) {
        boxes.push_back({20, 40 + (int)i * ROW_HEIGHT, WINDOW_WIDTH - 40, 30});
    }
    SDL_Rect cancelButton = {WINDOW_WIDTH - 210, buttonsY, 90, 30};
    SDL_Rect okButton = {WINDOW_WIDTH - 110, buttonsY, 90, 30};

    SDL_Color black = {0, 0, 0, 255};
    SDL_Color white = {255, 255, 255, 255};
    Uint32 dialogId = SDL_GetWindowID(window);

    int activeBox = 0;
    bool dialogActive = true;
    bool confirmed = false;
    SDL_Event e;

    while (dialogActive) {
        // Handle events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                dialogActive = false;
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (e.window.windowID == dialogId) dialogActive = false;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;
                if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                    confirmed = true;
                    dialogActive = false;
                } else if (key == SDLK_ESCAPE) {
                    dialogActive = false;
                } else if (key == SDLK_TAB) {
                    activeBox = (activeBox + 1) % values.size();
                } else if (key == SDLK_BACKSPACE && !values[activeBox].empty()) {
                    values[activeBox].pop_back();
                } else if (key == SDLK_v && (SDL_GetModState() & KMOD_CTRL)) {
                    char* clipboardText = SDL_GetClipboardText();
                    if (clipboardText) {
                        for (char* p = clipboardText; *p; p++) {
                            if (*p >= 32 && *p <= 126 && (!numbersOnly || isdigit((unsigned char)*p)))
                                values[activeBox] += *p;
                        }
                        SDL_free(clipboardText);
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                for (char* p = e.text.text; *p; p++) {
                    if (*p < 32 || *p > 126) continue;
                    if (numbersOnly && !isdigit((unsigned char)*p)) continue;
                    if (values[activeBox].size() < 200) values[activeBox] += *p;
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.windowID == dialogId) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;

                for (size_t i = 0; i < boxes.size(); i++) {
                    if (isInside(mouseX, mouseY, boxes[i])) activeBox = i;
                }
                if (isInside(mouseX, mouseY, okButton)) {
                    confirmed = true;
                    dialogActive = false;
                } else if (isInside(mouseX, mouseY, cancelButton)) {
                    dialogActive = false;
                }
            }
        }

        // Clear background
        SDL_SetRenderDrawColor(renderer, 225, 225, 225, 255);
        SDL_RenderClear(renderer);

        bool blink = (SDL_GetTicks() / 500) % 2 == 0;
        for (size_t i = 0; i < boxes.size(); i++) {
            renderText(renderer, font, labels[i], black, boxes[i].x, boxes[i].y - 25);

            // Draw input box (blue border for the one we are typing in)
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &boxes[i]);
            if ((int)i == activeBox)
                SDL_SetRenderDrawColor(renderer, 0, 120, 215, 255);
            else
                SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
            SDL_RenderDrawRect(renderer, &boxes[i]);

            // if the text is longer than the box only show the end of it
            string shownText = values[i];
            if ((int)i == activeBox && blink) shownText += "|";
            int textWidth = 0;
            TTF_SizeText(font, shownText.c_str(), &textWidth, nullptr);
            while (!shownText.empty() && textWidth > boxes[i].w - 10) {
                shownText.erase(0, 1);
                TTF_SizeText(font, shownText.c_str(), &textWidth, nullptr);
            }
            renderText(renderer, font, shownText, black, boxes[i].x + 5, boxes[i].y + 4);
        }

        // Draw buttons
        SDL_SetRenderDrawColor(renderer, 160, 40, 40, 255);
        SDL_RenderFillRect(renderer, &cancelButton);
        renderTextCentered(renderer, font, "Cancel", white, cancelButton);

        SDL_SetRenderDrawColor(renderer, 0, 128, 0, 255);
        SDL_RenderFillRect(renderer, &okButton);
        renderTextCentered(renderer, font, okText, white, okButton);

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return confirmed;
}

bool askYesNo(SDL_Window* window, const string &title, const string &message) {
    const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"},
    };
    SDL_MessageBoxData data = {SDL_MESSAGEBOX_WARNING, window, title.c_str(), message.c_str(), 2, buttons, nullptr};

    int buttonId = 0;
    if (SDL_ShowMessageBox(&data, &buttonId) < 0) return false;
    return buttonId == 1;
}


//--------------
//-----Undo-----
//--------------
vector<vector<string>> historyStack;  // Stores past states of the text
vector<vector<string>> redoStack;     // Stores undone states for redo

// call this BEFORE changing the text
void saveState(const EditorState &ed) {
    redoStack.clear(); // new change -> the old redo list is not valid anymore

    // no need to save the same thing twice
    if (!historyStack.empty() && historyStack.back() == ed.lines) return;

    historyStack.push_back(ed.lines);
    if ((int)historyStack.size() > MAX_HISTORY)
        historyStack.erase(historyStack.begin());
}

void undo(EditorState &ed) {
    if (historyStack.empty()) return;

    redoStack.push_back(ed.lines);
    ed.lines = historyStack.back();
    historyStack.pop_back();

    // Keep cursor in valid range
    ed.selectionActive = false;
    clampCursor(ed);
    ensureCursorVisible(ed);
}

void redo(EditorState &ed) {
    if (redoStack.empty()) return;

    historyStack.push_back(ed.lines);
    ed.lines = redoStack.back();
    redoStack.pop_back();

    ed.selectionActive = false;
    clampCursor(ed);
    ensureCursorVisible(ed);
}


//----------------
//-----Selection--
//----------------
template <typename T>
void myswap(T& a, T& b) {
    T temp = a;
    a = b;
    b = temp;
}

// true only if something is really selected (not just a click)
bool hasSelection(const EditorState &ed) {
    return ed.selectionActive &&
           (ed.selectionStartLine != ed.currentLine || ed.selectionStartPos != ed.cursorPos);
}

// gives the selection in the right order (start is always before end)
void getSelectionRange(const EditorState &ed, int &startLine, int &startPos, int &endLine, int &endPos) {
    startLine = ed.selectionStartLine;
    startPos = ed.selectionStartPos;
    endLine = ed.currentLine;
    endPos = ed.cursorPos;

    if (startLine > endLine || (startLine == endLine && startPos > endPos)) {
        myswap(startLine, endLine);
        myswap(startPos, endPos);
    }

    // just to be safe
    startLine = max(0, min(startLine, (int)ed.lines.size() - 1));
    endLine = max(0, min(endLine, (int)ed.lines.size() - 1));
    startPos = max(0, min(startPos, (int)ed.lines[startLine].size()));
    endPos = max(0, min(endPos, (int)ed.lines[endLine].size()));
}

string getSelectedText(const EditorState &ed) {
    if (!hasSelection(ed)) return "";

    int startLine, startPos, endLine, endPos;
    getSelectionRange(ed, startLine, startPos, endLine, endPos);

    string selectedText;
    for (int i = startLine; i <= endLine; ++i) {
        int lineStart = (i == startLine) ? startPos : 0;
        int lineEnd = (i == endLine) ? endPos : ed.lines[i].length();
        selectedText += ed.lines[i].substr(lineStart, lineEnd - lineStart);
        if (i < endLine) {
            selectedText += "\n";
        }
    }
    return selectedText;
}

void deleteSelection(EditorState &ed) {
    if (!hasSelection(ed)) {
        ed.selectionActive = false;
        return;
    }

    int startLine, startPos, endLine, endPos;
    getSelectionRange(ed, startLine, startPos, endLine, endPos);

    // works for one line and for multiple lines
    ed.lines[startLine] = ed.lines[startLine].substr(0, startPos) + ed.lines[endLine].substr(endPos);
    ed.lines.erase(ed.lines.begin() + startLine + 1, ed.lines.begin() + endLine + 1);

    ed.currentLine = startLine;
    ed.cursorPos = startPos;
    ed.selectionActive = false;
}


//----------------
//----Editing-----
//----------------

// inserts text at the cursor, the text can have more than one line (like when pasting)
void insertText(EditorState &ed, const string &text) {
    string cleanText;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\t') cleanText += string(TAB_SIZE, ' ');
        else if (c == '\n' || (c >= 32 && c <= 126)) cleanText += c;
    }

    string afterCursor = ed.lines[ed.currentLine].substr(ed.cursorPos);
    ed.lines[ed.currentLine].erase(ed.cursorPos);

    size_t pos;
    while ((pos = cleanText.find('\n')) != string::npos) {
        ed.lines[ed.currentLine] += cleanText.substr(0, pos);
        ed.lines.insert(ed.lines.begin() + ed.currentLine + 1, "");
        ed.currentLine++;
        cleanText.erase(0, pos + 1);
    }

    ed.lines[ed.currentLine] += cleanText;
    ed.cursorPos = ed.lines[ed.currentLine].size();
    ed.lines[ed.currentLine] += afterCursor;
    ensureCursorVisible(ed);
}

char getClosingChar(char c) {
    switch (c) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '"': return '"';
        case '\'': return '\'';
    }
    return 0;
}

// called for every SDL_TEXTINPUT
void typeText(EditorState &ed, const string &text) {
    deleteSelection(ed);

    if (text.size() != 1) {
        insertText(ed, text);
        return;
    }

    char c = text[0];
    string &line = ed.lines[ed.currentLine];
    char next = ed.cursorPos < (int)line.size() ? line[ed.cursorPos] : 0;
    char prev = ed.cursorPos > 0 ? line[ed.cursorPos - 1] : 0;

    // typing a closing bracket when it's already there -> just jump over it
    if ((c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') && next == c) {
        ed.cursorPos++;
        return;
    }

    // auto close brackets and quotes (quotes only if we are not in the middle of a word, like don't)
    bool isQuote = (c == '"' || c == '\'');
    if (getClosingChar(c) && !(isQuote && isWordChar(prev))) {
        line.insert(ed.cursorPos, string(1, c) + getClosingChar(c));
        ed.cursorPos++;
        return;
    }

    // typing } on an empty line -> remove one level of indentation
    if (c == '}' && ed.cursorPos >= TAB_SIZE && trim(line.substr(0, ed.cursorPos)).empty()) {
        line.erase(ed.cursorPos - TAB_SIZE, TAB_SIZE);
        ed.cursorPos -= TAB_SIZE;
    }

    insertText(ed, text);
}

// Enter key, keeps the indentation of the current line
void newLine(EditorState &ed) {
    deleteSelection(ed);

    string line = ed.lines[ed.currentLine];
    string before = line.substr(0, ed.cursorPos);
    string after = line.substr(ed.cursorPos);

    int indent = 0;
    while (indent < (int)before.size() && before[indent] == ' ') indent++;

    string trimmedBefore = trim(before);
    bool openBrace = !trimmedBefore.empty() && trimmedBefore.back() == '{';
    int newIndent = indent + (openBrace ? TAB_SIZE : 0);

    size_t firstChar = after.find_first_not_of(' ');
    after = (firstChar == string::npos) ? "" : after.substr(firstChar);

    ed.lines[ed.currentLine] = before;
    if (openBrace && !after.empty() && after[0] == '}') {
        // cursor was between { and } -> put the } on its own line
        ed.lines.insert(ed.lines.begin() + ed.currentLine + 1, string(newIndent, ' '));
        ed.lines.insert(ed.lines.begin() + ed.currentLine + 2, string(indent, ' ') + after);
    } else {
        ed.lines.insert(ed.lines.begin() + ed.currentLine + 1, string(newIndent, ' ') + after);
    }

    ed.currentLine++;
    ed.cursorPos = newIndent;
    ensureCursorVisible(ed);
}

void backspace(EditorState &ed) {
    if (hasSelection(ed)) {
        deleteSelection(ed);
        return;
    }
    ed.selectionActive = false;

    string &line = ed.lines[ed.currentLine];
    if (ed.cursorPos > 0) {
        char prev = line[ed.cursorPos - 1];
        char next = ed.cursorPos < (int)line.size() ? line[ed.cursorPos] : 0;

        // cursor is between () or "" -> delete both
        if (getClosingChar(prev) && getClosingChar(prev) == next)
            line.erase(ed.cursorPos - 1, 2);
        else
            line.erase(ed.cursorPos - 1, 1);
        ed.cursorPos--;
    } else if (ed.currentLine > 0) {
        // join with the line above
        ed.cursorPos = ed.lines[ed.currentLine - 1].size();
        ed.lines[ed.currentLine - 1] += ed.lines[ed.currentLine];
        ed.lines.erase(ed.lines.begin() + ed.currentLine);
        ed.currentLine--;
    }
    ensureCursorVisible(ed);
}

void deleteKey(EditorState &ed) {
    if (hasSelection(ed)) {
        deleteSelection(ed);
        return;
    }
    ed.selectionActive = false;

    string &line = ed.lines[ed.currentLine];
    if (ed.cursorPos < (int)line.size()) {
        line.erase(ed.cursorPos, 1);
    } else if (ed.currentLine < (int)ed.lines.size() - 1) {
        line += ed.lines[ed.currentLine + 1];
        ed.lines.erase(ed.lines.begin() + ed.currentLine + 1);
    }
}

void copySelection(const EditorState &ed) {
    if (hasSelection(ed)) {
        SDL_SetClipboardText(getSelectedText(ed).c_str());
    }
}

void cutSelection(EditorState &ed) {
    if (!hasSelection(ed)) return;
    saveState(ed);
    SDL_SetClipboardText(getSelectedText(ed).c_str());
    deleteSelection(ed);
    ensureCursorVisible(ed);
}

void pasteClipboard(EditorState &ed) {
    char* clipboardText = SDL_GetClipboardText();
    if (clipboardText == nullptr) return;

    string pastedText = clipboardText;
    SDL_free(clipboardText);
    if (pastedText.empty()) return;

    saveState(ed);
    deleteSelection(ed);
    insertText(ed, pastedText);
}

void selectAll(EditorState &ed) {
    ed.selectionActive = true;
    ed.selectionStartLine = 0;
    ed.selectionStartPos = 0;
    ed.currentLine = ed.lines.size() - 1;
    ed.cursorPos = ed.lines[ed.currentLine].size();
}

bool isMovementKey(SDL_Keycode key) {
    return key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN ||
           key == SDLK_HOME || key == SDLK_END || key == SDLK_PAGEUP || key == SDLK_PAGEDOWN;
}

void moveCursor(EditorState &ed, SDL_Keycode key, bool shift) {
    if (shift) {
        if (!ed.selectionActive) { // Start selection only if it's not already active
            ed.selectionStartLine = ed.currentLine;
            ed.selectionStartPos = ed.cursorPos;
            ed.selectionActive = true;
        }
    } else {
        ed.selectionActive = false;
    }

    int &line = ed.currentLine;
    int &pos = ed.cursorPos;
    int lastLine = ed.lines.size() - 1;
    int visibleLines = (CODE_BOTTOM - CODE_TOP) / LINE_HEIGHT;

    switch (key) {
        case SDLK_LEFT:
            if (pos > 0) {
                pos--;
            } else if (line > 0) {
                line--;
                pos = ed.lines[line].size();
            }
            break;
        case SDLK_RIGHT:
            if (pos < (int)ed.lines[line].size()) {
                pos++;
            } else if (line < lastLine) {
                line++;
                pos = 0;
            }
            break;
        case SDLK_UP:
            if (line > 0) line--;
            break;
        case SDLK_DOWN:
            if (line < lastLine) line++;
            break;
        case SDLK_HOME: {
            // first press -> first char of the line, second press -> column 0
            size_t firstChar = ed.lines[line].find_first_not_of(' ');
            int target = (firstChar == string::npos) ? 0 : firstChar;
            pos = (pos == target) ? 0 : target;
            break;
        }
        case SDLK_END:
            pos = ed.lines[line].size();
            break;
        case SDLK_PAGEUP:
            line = max(0, line - visibleLines);
            break;
        case SDLK_PAGEDOWN:
            line = min(lastLine, line + visibleLines);
            break;
    }

    pos = min(pos, (int)ed.lines[line].size());
    ensureCursorVisible(ed);
}

// converts a mouse position in the code area to line / column
void mouseToTextPosition(const EditorState &ed, int mouseX, int mouseY, int charWidth, int &line, int &column) {
    int relativeY = mouseY - CODE_TOP + ed.scrollOffset;
    line = (relativeY < 0) ? 0 : relativeY / LINE_HEIGHT;
    line = max(0, min(line, (int)ed.lines.size() - 1));

    column = (mouseX - CODE_X + ed.scrollX + charWidth / 2) / charWidth;
    column = max(0, min(column, (int)ed.lines[line].size()));
}


//---------------
//---Debugging---
//---------------

// Returns a copy of the line where comments and the inside of strings are replaced with spaces.
// This way the checkers don't get confused by something like "(" or // ;
string removeStringsAndComments(const string &line, bool &inBlockComment) {
    string result = line;
    int n = line.size();
    int i = 0;

    while (i < n) {
        if (inBlockComment) {
            if (line.compare(i, 2, "*/") == 0) {
                result[i] = result[i + 1] = ' ';
                inBlockComment = false;
                i += 2;
            } else {
                result[i] = ' ';
                i++;
            }
        } else if (line.compare(i, 2, "//") == 0) {
            for (int j = i; j < n; j++) result[j] = ' ';
            break;
        } else if (line.compare(i, 2, "/*") == 0) {
            result[i] = result[i + 1] = ' ';
            inBlockComment = true;
            i += 2;
        } else if (line[i] == '"' || line[i] == '\'') {
            // keep the quotes, remove what is inside
            char quote = line[i];
            i++;
            while (i < n && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < n) {
                    result[i] = ' ';
                    i++;
                }
                result[i] = ' ';
                i++;
            }
            i++;
        } else {
            i++;
        }
    }
    return result;
}

vector<string> getCleanLines(const vector<string> &lines) {
    vector<string> cleanLines;
    bool inBlockComment = false;
    for (const string &line : lines) {
        cleanLines.push_back(removeStringsAndComments(line, inBlockComment));
    }
    return cleanLines;
}

void updateDeclaredVariables(const vector<string> &cleanLines, unordered_set<string> &declaredVariables) {
    // user defined types first (struct Point {...} -> Point)
    regex typeNameRegex(R"(\b(?:struct|class|enum)\s+([a-zA-Z_]\w*))");
    unordered_set<string> userTypes;
    for (const string &line : cleanLines) {
        for (sregex_iterator it(line.cbegin(), line.cend(), typeNameRegex), end; it != end; ++it) {
            userTypes.insert((*it)[1].str());
            declaredVariables.insert((*it)[1].str());
        }
    }

    regex variableDeclarationRegex(
            R"((?:\b(?:int|float|double|char|bool|void|string|long|short|unsigned|signed|auto|size_t|ifstream|ofstream|fstream|stringstream)\b|\b(?:vector|map|set|pair|stack|queue)\s*<[^;()]*>)[\s\*&]+([a-zA-Z_]\w*))");
    regex userVariableRegex(R"(\b([a-zA-Z_]\w*)[\s\*&]+([a-zA-Z_]\w*)\s*[;=,\[\(\)\{])");
    regex commaRegex(R"(,\s*[\*&]?\s*([a-zA-Z_]\w*))");

    for (const string &line : cleanLines) {
        for (sregex_iterator it(line.cbegin(), line.cend(), variableDeclarationRegex), end; it != end; ++it) {
            declaredVariables.insert((*it)[1].str()); // Insert only the variable name

            // int a = 1, b, c; -> also take b and c
            size_t from = it->position() + it->length();
            size_t to = line.find(';', from);
            string rest = line.substr(from, to == string::npos ? string::npos : to - from);
            for (sregex_iterator it2(rest.cbegin(), rest.cend(), commaRegex), end2; it2 != end2; ++it2) {
                declaredVariables.insert((*it2)[1].str());
            }
        }

        for (sregex_iterator it(line.cbegin(), line.cend(), userVariableRegex), end; it != end; ++it) {
            if (userTypes.count((*it)[1].str()))
                declaredVariables.insert((*it)[2].str());
        }
    }
}

// Function to check for missing semicolons
void checkForMissingSemicolons(const vector<string> &cleanLines, vector<string> &errors) {
    // lines that start with these words usually don't end with ;
    set<string> blockWords = {"if", "for", "while", "else", "switch", "do", "try", "catch",
                              "class", "struct", "namespace", "enum", "union", "template"};

    vector<bool> braceIsList;  // true if the { is for a list like  int a[] = {1, 2, 3};
    string previousLine;

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        string line = trim(cleanLines[i]);
        bool insideList = !braceIsList.empty() && braceIsList.back();

        // update the brace stack for the next lines
        for (size_t j = 0; j < line.size(); j++) {
            if (line[j] == '{') {
                string before = trim(line.substr(0, j));
                bool nowInList = !braceIsList.empty() && braceIsList.back();
                bool isList = nowInList || before.find("enum") != string::npos ||
                              (!before.empty() && strchr("=,({[", before.back())) ||
                              (before.empty() && !previousLine.empty() && previousLine.back() == '=');
                braceIsList.push_back(isList);
            } else if (line[j] == '}') {
                if (!braceIsList.empty()) braceIsList.pop_back();
            }
        }

        if (line.empty()) continue;
        previousLine = line;
        if (line[0] == '#' || insideList) continue;

        char last = line.back();
        bool endsWithIncrement = line.size() >= 2 &&
                                 (line.compare(line.size() - 2, 2, "++") == 0 || line.compare(line.size() - 2, 2, "--") == 0);

        if (strchr(";{},:([\\", last)) continue;
        if (strchr("+-*/%=<>&|!?.^~", last) && !endsWithIncrement) continue;  // statement continues in next line

        // next line that is not empty
        string nextLine;
        for (size_t k = i + 1; k < cleanLines.size() && nextLine.empty(); k++) {
            nextLine = trim(cleanLines[k]);
        }

        // cout << "a"
        //      << "b";   <- the first line doesn't need a ;
        if (!nextLine.empty()) {
            if (nextLine.compare(0, 2, "<<") == 0 || nextLine.compare(0, 2, ">>") == 0 ||
                nextLine.compare(0, 2, "&&") == 0 || nextLine.compare(0, 2, "||") == 0 ||
                strchr(".?:)+,", nextLine[0])) {
                continue;
            }
        }

        // first word of the line (skip a } at the start like in "} else")
        size_t k = 0;
        while (k < line.size() && (line[k] == '}' || line[k] == ' ')) k++;
        string firstWord;
        while (k < line.size() && isWordChar(line[k])) firstWord += line[k++];
        bool startsWithBrace = line[0] == '}';

        bool needsSemicolon = true;
        if (firstWord == "while" && startsWithBrace) {
            needsSemicolon = true;  // } while (x);  of a do-while
        } else if (firstWord == "if" || firstWord == "for" || firstWord == "while" ||
                   firstWord == "switch" || firstWord == "catch") {
            // "if (x)" is fine, "if (x) y = 5" needs a ;
            needsSemicolon = (last != ')');
        } else if (firstWord == "else") {
            string rest = trim(line.substr(line.find("else") + 4));
            needsSemicolon = !(rest.empty() || (rest.compare(0, 2, "if") == 0 && last == ')'));
        } else if (blockWords.count(firstWord)) {
            needsSemicolon = false;
        } else if (last == ')' && !nextLine.empty() && nextLine[0] == '{') {
            needsSemicolon = false;  // function header like "int main()"
        }

        if (needsSemicolon) {
            errors.push_back("Error: Missing ';' at the end of line " + to_string(i + 1));
        }
    }
}

void checkBracketErrors(const vector<string>& cleanLines, vector<string>& errors) {
    stack<pair<char, int>> bracketStack;  // bracket + line number
    for (size_t i = 0; i < cleanLines.size(); ++i) {
        const string& line = cleanLines[i];
        for (char c: line) {
            switch (c) {
                case '(':
                case '[':
                case '{':
                    bracketStack.push({c, (int)i + 1});
                    break;
                case ')':
                    if (bracketStack.empty() || bracketStack.top().first != '(') {
                        errors.push_back("Error: Mismatched ')' at line " + to_string(i + 1));
                    } else {
                        bracketStack.pop();
                    }
                    break;
                case ']':
                    if (bracketStack.empty() || bracketStack.top().first != '[') {
                        errors.push_back("Error: Mismatched ']' at line " + to_string(i + 1));
                    } else {
                        bracketStack.pop();
                    }
                    break;
                case '}':
                    if (bracketStack.empty() || bracketStack.top().first != '{') {
                        errors.push_back("Error: Mismatched '}' at line " + to_string(i + 1));
                    } else {
                        bracketStack.pop();
                    }
                    break;
            }
        }
    }

    // Check for any unclosed brackets
    while (!bracketStack.empty()) {
        string message = "Error: Unclosed '";
        message += bracketStack.top().first;
        message += "' opened at line " + to_string(bracketStack.top().second);
        errors.push_back(message);
        bracketStack.pop();
    }
}

// Levenshtein distance + swapping two letters counts as 1 (so "retrun" is close to "return")
int levenshteinDistance(const string& s1, const string& s2) {
    int m = s1.size();
    int n = s2.size();

    vector<vector<int>> dp(m + 1, vector<int>(n + 1, 0));

    for (int i = 0; i <= m; i++) {
        dp[i][0] = i;
    }
    for (int j = 0; j <= n; j++) {
        dp[0][j] = j;
    }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                dp[i][j] = min(dp[i][j], dp[i - 2][j - 2] + 1);
            }
        }
    }

    return dp[m][n];
}

unordered_set<string> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept",
        "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t", "char16_t",
        "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete",
        "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
        "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "reflexpr", "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof",
        "static", "static_assert", "static_cast", "string", "struct", "switch", "synchronized", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
        "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
};

unordered_set<string> functionNames = {
        "strlen", "strcpy", "strcat", "strcmp",  // String functions
        "printf", "scanf", "fprintf", "fscanf",   // Input/Output functions
        "getline", "to_string", "stoi", "stod", "swap", "max", "min", "rand", "srand"
};

unordered_set<string> generalKeywords = {
        "float", "int", "double", "char", "bool", "void", "auto",
        "main", "if", "else", "for", "while", "do", "switch", "case",
        "break", "continue", "return", "class", "struct", "namespace",
        "using", "template", "typename", "try", "catch", "throw", "new",
        "delete", "this", "sizeof", "typedef", "enum", "union", "const",
        "static", "friend", "public", "private", "protected", "virtual",
        "override", "final", "constexpr", "constinit", "consteval",
        "include", "iostream", "string", "vector", "cout", "cin", "endl"
};

// words that are fine even if the user didn't declare them
unordered_set<string> knownWords = {
        "main", "std", "cout", "cin", "cerr", "endl", "string", "vector", "map", "set", "pair",
        "NULL", "EOF", "max", "min", "swap", "abs", "rand", "srand", "time", "exit", "system",
        "getline", "to_string", "stoi", "stod", "size", "length", "push_back", "pop_back",
        "begin", "end", "npos", "include", "ifstream", "ofstream", "fstream", "stringstream",
        "argc", "argv", "size_t", "getchar", "putchar", "puts", "gets"
};

bool isKnownWord(const string &word) {
    if (keywords.count(word) || cppKeywords.count(word) || dataTypes.count(word) ||
        generalKeywords.count(word) || functionNames.count(word) || knownWords.count(word))
        return true;

    for (const auto &lib : libraryFunctions) {
        if (find(lib.second.begin(), lib.second.end(), word) != lib.second.end()) return true;
    }
    return false;
}

void checkSpellingErrors(
        const vector<string>& cleanLines,
        const unordered_set<string>& keywords,
        vector<string>& errors,
        const unordered_set<string>& declaredVariables
) {
    regex wordRegex(R"(\b([a-zA-Z_]\w*)\b)");

    // words we compare against
    vector<string> candidates(keywords.begin(), keywords.end());
    candidates.push_back("cout");
    candidates.push_back("cin");
    candidates.push_back("endl");
    candidates.push_back("main");

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        const string &line = cleanLines[i];
        if (trim(line).empty() || trim(line)[0] == '#') continue;

        set<string> reported;  // report each word only once per line
        for (sregex_iterator it(line.cbegin(), line.cend(), wordRegex), end; it != end; ++it) {
            string word = it->str();
            int pos = it->position();

            if (word.size() < 3 || isKnownWord(word) || declaredVariables.count(word)) continue;
            // something like obj.name or ptr->name or std::name
            if (pos > 0 && (line[pos - 1] == '.' || line[pos - 1] == '>' || line[pos - 1] == ':')) continue;

            int minDistance = numeric_limits<int>::max(); // Initialize with max value
            string closestKeyword;

            for (const auto& keyword: candidates) {
                int distance = levenshteinDistance(word, keyword);
                if (distance < minDistance) {
                    minDistance = distance;
                    closestKeyword = keyword;
                }
            }

            // short words need to be really close, otherwise almost everything is an "error"
            const int DISTANCE_THRESHOLD = (word.size() <= 5) ? 1 : 2;

            if (minDistance <= DISTANCE_THRESHOLD && !reported.count(word)) {
                reported.insert(word);
                errors.push_back("Spelling Error: '" + word +
                                 "' at line " + to_string(i + 1) +
                                 ". Did you mean '" + closestKeyword + "'?");
            }
        }
    }
}

void checkVariableNamingErrors(const vector<string>& cleanLines,
                               const unordered_set<string>& keywords,
                               vector<string>& errors) {
    regex variableRegex(R"(\b(int|float|double|char|bool|string|long|short|unsigned)\s+([^\s;=(),\[\]{}<>:]+))");
    regex validName(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    unordered_set<string> typeWords = {"int", "long", "short", "char", "double", "float",
                                       "unsigned", "signed", "const", "bool"};

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        const string &line = cleanLines[i];
        for (sregex_iterator it(line.cbegin(), line.cend(), variableRegex), end; it != end; ++it) {
            string variableName = (*it)[2].str();

            // int *p  or  int &r
            while (!variableName.empty() && (variableName[0] == '*' || variableName[0] == '&'))
                variableName.erase(0, 1);
            // unsigned int x -> "int" is not the name
            if (variableName.empty() || typeWords.count(variableName)) continue;

            if (isdigit((unsigned char)variableName[0])) {
                errors.push_back("Error: Variable name '" + variableName +
                                 "' cannot start with a digit at line " + to_string(i + 1));
            } else if (keywords.find(variableName) != keywords.end()) {
                errors.push_back("Error: Variable name '" + variableName +
                                 "' cannot be a keyword at line " + to_string(i + 1));
            } else if (!regex_match(variableName, validName)) {
                errors.push_back("Error: Invalid character in variable name '" + variableName +
                                 "' at line " + to_string(i + 1));
            }
        }
    }
}

void checkUndeclaredVariables(const vector<string>& cleanLines, vector<string>& errors, const unordered_set<string>& declaredVariables) {
    // a name that gets a value:  x = 5,  x += 2,  x++
    regex variableUsageRegex(R"(\b([a-zA-Z_]\w*)\s*(\+\+|--|[-+*/%]?=(?!=)))");

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        const string& line = cleanLines[i];
        if (trim(line).empty() || trim(line)[0] == '#') continue;

        set<string> reported;
        for (sregex_iterator it(line.cbegin(), line.cend(), variableUsageRegex), end; it != end; ++it) {
            string variable = (*it)[1].str();
            int pos = it->position();

            if (pos > 0 && (line[pos - 1] == '.' || line[pos - 1] == '>' || line[pos - 1] == ':')) continue;
            if (declaredVariables.count(variable) || isKnownWord(variable) || reported.count(variable)) continue;

            reported.insert(variable);
            errors.push_back("Error: Variable '" + variable + "' is used but not declared at line " + to_string(i + 1));
        }
    }
}

void checkStringErrors(const vector<string>& lines, vector<string>& errors) {
    bool inBlockComment = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        const string& line = lines[i];
        bool inString = false;
        bool inChar = false;

        for (size_t j = 0; j < line.size(); ++j) {
            char c = line[j];
            char next = (j + 1 < line.size()) ? line[j + 1] : 0;

            if (inBlockComment) {
                if (c == '*' && next == '/') {
                    inBlockComment = false;
                    j++;
                }
                continue;
            }

            if (inString || inChar) {
                if (c == '\\') {
                    j++;  // skip the escaped char
                } else if (inString && c == '"') {
                    inString = false;
                } else if (inChar && c == '\'') {
                    inChar = false;
                }
                continue;
            }

            if (c == '/' && next == '/') break;  // rest of the line is a comment
            if (c == '/' && next == '*') {
                inBlockComment = true;
                j++;
            } else if (c == '"') {
                inString = true;
            } else if (c == '\'') {
                inChar = true;
            }
        }

        if (inString) {
            errors.push_back("Error: Unclosed string literal at line " + to_string(i + 1));
        }
        if (inChar) {
            errors.push_back("Error: Unclosed character literal at line " + to_string(i + 1));
        }
    }
}

void checkMultilineCommentErrors(const vector<string>& lines, vector<string>& errors) {
    bool inMultilineComment = false;
    int startLine = -1;

    for (size_t i = 0; i < lines.size(); ++i) {
        bool wasInComment = inMultilineComment;
        removeStringsAndComments(lines[i], inMultilineComment);
        if (!wasInComment && inMultilineComment) {
            startLine = i + 1;
        }
    }

    if (inMultilineComment) {
        errors.push_back("Error: Unclosed multiline comment started at line " + to_string(startLine));
    }
}

// checks if a library function is used without its #include
void checkCodeErrors(const vector<string>& cleanLines, vector<string>& collectedErrors, const unordered_set<string>& declaredVariables) {
    set<string> includes(currentIncludes.begin(), currentIncludes.end());
    if (includes.count("<bits/stdc++.h>")) return;  // this one includes everything
    if (includes.count("<math.h>")) includes.insert("<cmath>");
    if (includes.count("<string.h>")) includes.insert("<cstring>");

    // Build a map from symbol to its required library
    map<string, string> symbolToLibrary;
    for (const auto& lib : libraryFunctions) {
        for (const auto& symbol : lib.second) {
            symbolToLibrary[symbol] = lib.first;
        }
    }

    regex identifierRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    set<string> reported;

    for (const string &line : cleanLines) {
        if (trim(line).empty() || trim(line)[0] == '#') continue;

        for (sregex_iterator it(line.cbegin(), line.cend(), identifierRegex), end; it != end; ++it) {
            string identifier = it->str();
            int pos = it->position();

            if (!symbolToLibrary.count(identifier) || declaredVariables.count(identifier)) continue;
            if (pos > 0 && (line[pos - 1] == '.' || line[pos - 1] == '>')) continue;  // v.size(), p->x

            const string& requiredLibrary = symbolToLibrary[identifier];
            if (!includes.count(requiredLibrary) && !reported.count(identifier)) {
                reported.insert(identifier);
                collectedErrors.push_back("Error: Missing include for '" + identifier + "'. Include " + requiredLibrary + " to use it.");
            }
        }
    }
}

void checkForElseIfChains(const vector<string>& cleanLines, vector<string>& warnings) {
    regex elseIfRegex(R"(\belse\s*if\s*\()");
    int elseIfCount = 0;
    int chainStartLine = -1;
    bool inChain = false;

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        // comments are already removed in cleanLines
        string line = trim(cleanLines[i]);

        // Ignore empty lines
        if (line.empty()) {
            if (inChain) {
                inChain = false;
                if (elseIfCount >= 3) {
                    warnings.push_back("Warning: Consider using switch-case for chain of " +
                                       to_string(elseIfCount) + " else-if statements starting at line " +
                                       to_string(chainStartLine));
                }
                elseIfCount = 0;
                chainStartLine = -1;
            }
            continue;
        }

        if (regex_search(line, elseIfRegex)) {
            if (!inChain) {
                inChain = true;
                chainStartLine = i + 1;
            }
            elseIfCount++;
        } else if (line.find("else") != string::npos && inChain) {
            if (elseIfCount >= 3) {
                warnings.push_back("Warning: Consider using switch-case for chain of " +
                                   to_string(elseIfCount) + " else-if statements starting at line " +
                                   to_string(chainStartLine));
            }
            inChain = false;
            elseIfCount = 0;
            chainStartLine = -1;
        }
    }

    if (inChain && elseIfCount >= 3) {
        warnings.push_back("Warning: Consider using switch-case for chain of " +
                           to_string(elseIfCount) + " else-if statements starting at line " +
                           to_string(chainStartLine));
    }
}

// counts the arguments inside ( ), only the commas that are not inside other brackets
int countArguments(const string &args) {
    string trimmed = trim(args);
    if (trimmed.empty() || trimmed == "void") return 0;

    int depth = 0;
    int count = 1;
    for (char c : trimmed) {
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') depth--;
        else if (c == ',' && depth == 0) count++;
    }
    return count;
}

map<string, int> functionParams;  // function name -> number of parameters (-1 = don't check it)
void processFunctionDefinitions(const vector<string>& cleanLines) {
    functionParams.clear();
    regex funcDefRegex(R"(^\s*(?:(?:static|inline|virtual)\s+)*([a-zA-Z_][\w:<>]*)[\s\*&]+([a-zA-Z_]\w*)\s*\(([^()]*)\)\s*(?:const\s*)?[{;]?\s*$)");
    set<string> notTypes = {"return", "else", "new", "delete", "throw", "case", "goto", "using", "typedef"};

    for (const auto& line : cleanLines) {
        smatch match;
        if (regex_search(line, match, funcDefRegex)) {
            if (notTypes.count(match[1].str())) continue;

            string funcName = match[2];
            string params = match[3];
            int paramCount = countArguments(params);

            // default values make the number of arguments different, just skip those
            if (params.find('=') != string::npos) paramCount = -1;

            // same name with different parameters = overloading, skip it too
            if (functionParams.count(funcName) && functionParams[funcName] != paramCount)
                paramCount = -1;

            functionParams[funcName] = paramCount;
        }
    }
}

void checkFunctionCalls(const vector<string>& cleanLines, vector<string>& errors) {
    regex funcCallRegex(R"(\b([a-zA-Z_]\w*)\s*\()");

    for (size_t i = 0; i < cleanLines.size(); ++i) {
        const string &line = cleanLines[i];

        for (sregex_iterator it(line.cbegin(), line.cend(), funcCallRegex), end; it != end; ++it) {
            string funcName = (*it)[1].str();
            int pos = it->position();
            if (!functionParams.count(funcName) || functionParams[funcName] == -1) continue;
            if (pos > 0 && (line[pos - 1] == '.' || line[pos - 1] == '>')) continue;

            // find the ) that belongs to this (
            size_t open = pos + it->length() - 1;
            int depth = 0;
            size_t close = string::npos;
            for (size_t j = open; j < line.size(); j++) {
                if (line[j] == '(') depth++;
                else if (line[j] == ')') {
                    depth--;
                    if (depth == 0) {
                        close = j;
                        break;
                    }
                }
            }
            if (close == string::npos) continue;  // call continues on the next line

            int expected = functionParams[funcName];
            int actual = countArguments(line.substr(open + 1, close - open - 1));

            if (actual != expected) {
                errors.push_back("Error: Function '" + funcName + "' expects " +
                                 to_string(expected) + " arguments but got " +
                                 to_string(actual) + " at line " + to_string(i + 1));
            }
        }
    }
}

// runs all the checks and fills the error / warning lists
void analyzeCode(const vector<string> &lines, vector<string> &errors, vector<string> &warnings,
                 unordered_set<string> &declaredVariables) {
    errors.clear();
    warnings.clear();
    declaredVariables.clear();

    vector<string> cleanLines = getCleanLines(lines);
    currentIncludes = extractIncludedLibraries(getCode(cleanLines));

    updateDeclaredVariables(cleanLines, declaredVariables);
    processFunctionDefinitions(cleanLines);

    checkForMissingSemicolons(cleanLines, errors); // Check syntax errors
    checkBracketErrors(cleanLines, errors);
    checkStringErrors(lines, errors);
    checkMultilineCommentErrors(lines, errors);
    checkVariableNamingErrors(cleanLines, keywords, errors);
    checkUndeclaredVariables(cleanLines, errors, declaredVariables);
    checkSpellingErrors(cleanLines, keywords, errors, declaredVariables);
    checkCodeErrors(cleanLines, errors, declaredVariables);
    checkFunctionCalls(cleanLines, errors);

    checkForElseIfChains(cleanLines, warnings);
}


//--------------------
//---Autocomplete-----
//--------------------

// finds the best word from the list for what the user typed so far
string findClosestWord(const string& word, const vector<string>& candidates) {
    if (word.empty()) return "";

    // first try the words that start with what we typed ("ret" -> "return")
    string best;
    for (const auto& candidate : candidates) {
        if (candidate.size() > word.size() && candidate.compare(0, word.size(), word) == 0) {
            if (best.empty() || candidate.size() < best.size() ||
                (candidate.size() == best.size() && candidate < best))
                best = candidate;
        }
    }
    if (!best.empty()) return best;

    // no word starts like this, maybe it's a typo ("retrun" -> "return")
    int minDistance = numeric_limits<int>::max();
    for (const auto& candidate : candidates) {
        int distance = levenshteinDistance(word, candidate);
        if (distance < minDistance || (distance == minDistance && candidate < best)) {
            minDistance = distance;
            best = candidate;
        }
    }

    const int DISTANCE_THRESHOLD = (word.size() <= 4) ? 1 : 2;
    if (minDistance <= DISTANCE_THRESHOLD) {
        return best;
    } else {
        return ""; // No close match found
    }
}

vector<string> getKeywordCandidates() {
    vector<string> result(cppKeywords.begin(), cppKeywords.end());
    result.insert(result.end(), dataTypes.begin(), dataTypes.end());
    return result;
}

vector<string> getGeneralCandidates() {
    return vector<string>(generalKeywords.begin(), generalKeywords.end());
}

// functions from the included libraries + the user's own functions
vector<string> getFunctionCandidates() {
    vector<string> result(functionNames.begin(), functionNames.end());
    for (const auto& lib : currentIncludes) {
        if (libraryFunctions.count(lib)) {
            result.insert(result.end(), libraryFunctions[lib].begin(), libraryFunctions[lib].end());
        }
    }
    for (const auto& func : functionParams) {
        result.push_back(func.first);
    }
    return result;
}

// replaces the word before the cursor with the suggestion
void autocompleteWord(EditorState &ed, const vector<string> &candidates) {
    string &line = ed.lines[ed.currentLine];
    int wordStart = ed.cursorPos;
    while (wordStart > 0 && isWordChar(line[wordStart - 1])) {
        wordStart--;
    }

    string currentWord = line.substr(wordStart, ed.cursorPos - wordStart);
    string suggestion = findClosestWord(currentWord, candidates);
    if (suggestion.empty() || suggestion == currentWord) return;

    saveState(ed);
    line.replace(wordStart, currentWord.size(), suggestion);
    ed.cursorPos = wordStart + suggestion.size();
    ed.selectionActive = false;
}


//--------------------
//--Compile and run---
//--------------------

bool compileCode(const string& code, const string& projectName, vector<string>& compilerMessages) {
    compilerMessages.clear();

    // First, save the code to a temporary file
    string filePath = projectName + ".cpp";
    ofstream outputFile(filePath);
    if (!outputFile.is_open()) {
        compilerMessages.push_back("Error: could not create " + filePath);
        return false;
    }
    outputFile << code;
    outputFile.close();

    // 2>&1 because g++ writes the errors to stderr
    string compileCommand = "g++ \"" + filePath + "\" -o \"" + projectName + ".exe\" 2>&1";

    // Use popen to capture compiler output
    FILE* pipe = popen(compileCommand.c_str(), "r");
    if (!pipe) {
        compilerMessages.push_back("Error: could not run g++");
        return false;
    }

    string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output += buffer;
    }
    int result = pclose(pipe);

    // g++ prints a lot of extra lines, keep only the ones with error/warning
    vector<string> allLines;
    stringstream ss(output);
    string line;
    while (getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // "temp_project.cpp:5:3: error: ..." -> "line 5:3: error: ..."
        if (line.compare(0, filePath.size() + 1, filePath + ":") == 0) {
            line = "line " + line.substr(filePath.size() + 1);
        }
        allLines.push_back("g++: " + line);

        if (line.find("error") != string::npos || line.find("warning") != string::npos) {
            compilerMessages.push_back("g++: " + line);
        }
    }

    if (result != 0 && compilerMessages.empty()) {
        compilerMessages = allLines;  // something else went wrong (like g++ not being installed)
    }
    return result == 0;
}

void runProgram(const string& projectName) {
    // run in a new cmd window so the program can use cin and the window stays open at the end
    string runCommand = "start \"CMorgh - Run\" cmd /c \"" + projectName + ".exe & echo. & pause\"";
    system(runCommand.c_str());
}


//--------------------
//-----Others---------
//--------------------

struct TopButton {
    string text;
    int x;
    int width;
};

enum TopButtonId { BTN_FILE, BTN_EDIT, BTN_VIEW, BTN_COMPILE, BTN_RUN, BTN_HELP };

vector<string> fileMenuItems = {"New Project", "Save Project", "Save As...", "Exit"};
vector<string> editMenuItems = {"Undo", "Redo", "Cut", "Copy", "Paste", "Select All", "Go to Line"};
vector<string> viewMenuItems = {"Light Mode", "Dark Mode"};

// returns which item of an open menu is under the mouse, -1 if none
int getMenuItemAt(int mouseX, int mouseY, int menuX, int itemCount) {
    if (mouseX < menuX || mouseX > menuX + MENU_WIDTH) return -1;
    if (mouseY < TOP_BAR_HEIGHT || mouseY >= TOP_BAR_HEIGHT + itemCount * MENU_ITEM_HEIGHT) return -1;
    return (mouseY - TOP_BAR_HEIGHT) / MENU_ITEM_HEIGHT;
}

const char* HELP_TEXT =
        "Shortcuts:\n\n"
        "Ctrl + N            New project\n"
        "Ctrl + S            Save\n"
        "Ctrl + Shift + S    Save as\n"
        "Ctrl + Z / Ctrl + Y    Undo / Redo\n"
        "Ctrl + X / C / V    Cut / Copy / Paste\n"
        "Ctrl + A            Select all\n"
        "Ctrl + G            Go to line\n"
        "Ctrl + Space        Complete a keyword\n"
        "Ctrl + Shift + F    Complete a function name\n"
        "Ctrl + Shift + G    Complete a general word\n"
        "F5                  Compile and run\n"
        "F7                  Debug & compile\n\n"
        "Select text with Shift + arrow keys or by dragging the mouse.\n"
        "Click a project on the left panel to open it.";


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
    SDL_Window *window = SDL_CreateWindow("CMorgh IDE",
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

    // the assets folder is copied next to the exe by cmake
    string iconPath = "assets\\icon.bmp";
    char* basePath = SDL_GetBasePath();
    if (basePath) {
        iconPath = string(basePath) + iconPath;
        SDL_free(basePath);
    }
    SDL_Surface* icon = SDL_LoadBMP(iconPath.c_str());
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    } else {
        cerr << "Icon could not be loaded! SDL_Error: " << SDL_GetError() << endl;
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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);  // for the transparent selection

    // Load fonts
    TTF_Font *font = TTF_OpenFont(UI_FONT_PATH, 18);
    TTF_Font *codefont = TTF_OpenFont(CODE_FONT_PATH, 16);
    if (!font || !codefont) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        if (font) TTF_CloseFont(font);
        if (codefont) TTF_CloseFont(codefont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    LINE_HEIGHT = TTF_FontHeight(codefont) + 2;
    const int charWidth = getSpaceWidth(codefont);  // consolas is monospace so every char has the same width

    // Basic editor state
    EditorState editor;
    loadProjectsList();

    // open a file if it was given in the command line:  CMorghIDE.exe test.cpp
    if (argc > 1 && readFileToLines(argv[1], editor.lines)) {
        editor.projectPath = argv[1];
        editor.projectName = editor.projectPath.substr(editor.projectPath.find_last_of("\\/") + 1);
        if (editor.projectName.size() > 4 && editor.projectName.substr(editor.projectName.size() - 4) == ".cpp")
            editor.projectName.erase(editor.projectName.size() - 4);
    }

    string savedCode = getCode(editor.lines);    // what is saved on disk (to know if there are unsaved changes)
    string lastAnalyzedCode;
    string lastTitle;

    int projectScrollOffset = 0;
    int errorScrollOffset = 0;

    // top bar buttons, the width is measured after the font is loaded
    vector<TopButton> topButtons = {{"File", 10, 0}, {"Edit", 60, 0}, {"View", 110, 0},
                                    {"Debug & Compile", 165, 0}, {"Run", 310, 0}, {"Help", 360, 0}};
    for (auto &button : topButtons) {
        TTF_SizeText(font, button.text.c_str(), &button.width, nullptr);
    }
    int openMenu = -1;  // -1 = no menu, otherwise BTN_FILE / BTN_EDIT / BTN_VIEW

    bool mouseSelecting = false;

    // Timer for cursor blinking
    Uint32 lastCursorToggle = SDL_GetTicks();
    bool cursorVisible = true;
    const Uint32 CURSOR_BLINK_INTERVAL = 500; // ms

    vector<string> collectedErrors;
    vector<string> collectedWarnings;
    vector<string> compilerMessages;
    unordered_set<string> declaredVariables;

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        // things that are requested in the events and done after the event loop
        bool newProjectRequested = false, saveRequested = false, saveAsRequested = false;
        bool goToLineRequested = false, compileRequested = false, runRequested = false;
        bool helpRequested = false, quitRequested = false;
        int openProjectIndex = -1;

        // Event loop
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quitRequested = true;
            } else if (e.type == SDL_MOUSEWHEEL) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                int amount = e.wheel.y * LINE_HEIGHT * 3;

                if (mouseX > PROJECT_PANEL_WIDTH && mouseY < CODE_BOTTOM) { // Code panel area
                    editor.scrollOffset = max(0, editor.scrollOffset - amount);
                } else if (mouseX < PROJECT_PANEL_WIDTH) { // Project panel area
                    projectScrollOffset = max(0, projectScrollOffset - amount);
                } else { // Error panel area
                    errorScrollOffset = max(0, errorScrollOffset - amount);
                }
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;
                SDL_Keymod mod = SDL_GetModState();
                bool ctrl = mod & KMOD_CTRL;
                bool shift = mod & KMOD_SHIFT;

                // show the cursor while typing
                cursorVisible = true;
                lastCursorToggle = SDL_GetTicks();

                if (ctrl) {
                    switch (key) {
                        case SDLK_c: copySelection(editor); break;
                        case SDLK_x: cutSelection(editor); break;
                        case SDLK_v: pasteClipboard(editor); break;
                        case SDLK_a: selectAll(editor); break;
                        case SDLK_z:
                            if (shift) redo(editor);
                            else undo(editor);
                            break;
                        case SDLK_y: redo(editor); break;
                        case SDLK_n: newProjectRequested = true; break;
                        case SDLK_s:
                            if (shift) saveAsRequested = true;
                            else saveRequested = true;
                            break;
                        case SDLK_g:
                            if (shift) autocompleteWord(editor, getGeneralCandidates());
                            else goToLineRequested = true;
                            break;
                        case SDLK_f:
                            if (shift) autocompleteWord(editor, getFunctionCandidates());
                            break;
                        case SDLK_SPACE: autocompleteWord(editor, getKeywordCandidates()); break;
                        default: break;
                    }
                } else if (isMovementKey(key)) {
                    moveCursor(editor, key, shift);
                } else {
                    switch (key) {
                        case SDLK_BACKSPACE:
                            saveState(editor);
                            backspace(editor);
                            break;
                        case SDLK_DELETE:
                            saveState(editor);
                            deleteKey(editor);
                            break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            saveState(editor);
                            newLine(editor);
                            break;
                        case SDLK_TAB:
                            saveState(editor);
                            deleteSelection(editor);
                            insertText(editor, string(TAB_SIZE, ' '));
                            break;
                        case SDLK_ESCAPE:
                            openMenu = -1;
                            editor.selectionActive = false;
                            break;
                        case SDLK_F1: helpRequested = true; break;
                        case SDLK_F5: runRequested = true; break;
                        case SDLK_F7: compileRequested = true; break;
                        default: break;
                    }
                }
            } else if (e.type == SDL_TEXTINPUT) {
                // on windows ctrl+space also sends a text event, we don't want that space
                SDL_Keymod mod = SDL_GetModState();
                if ((mod & KMOD_CTRL) && !(mod & KMOD_ALT)) continue;

                saveState(editor);
                typeText(editor, e.text.text);
                cursorVisible = true;
                lastCursorToggle = SDL_GetTicks();
            } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mouseX = e.button.x;
                int mouseY = e.button.y;
                int menuThatWasOpen = openMenu;

                // click on an open menu
                if (openMenu != -1) {
                    int menuX = topButtons[openMenu].x - 5;
                    int item = -1;
                    openMenu = -1;

                    if (menuThatWasOpen == BTN_FILE) {
                        item = getMenuItemAt(mouseX, mouseY, menuX, fileMenuItems.size());
                        if (item == 0) newProjectRequested = true;
                        else if (item == 1) saveRequested = true;
                        else if (item == 2) saveAsRequested = true;
                        else if (item == 3) quitRequested = true;
                    } else if (menuThatWasOpen == BTN_EDIT) {
                        item = getMenuItemAt(mouseX, mouseY, menuX, editMenuItems.size());
                        if (item == 0) undo(editor);
                        else if (item == 1) redo(editor);
                        else if (item == 2) cutSelection(editor);
                        else if (item == 3) copySelection(editor);
                        else if (item == 4) pasteClipboard(editor);
                        else if (item == 5) selectAll(editor);
                        else if (item == 6) goToLineRequested = true;
                    } else if (menuThatWasOpen == BTN_VIEW) {
                        item = getMenuItemAt(mouseX, mouseY, menuX, viewMenuItems.size());
                        if (item == 0) isDarkMode = false;  // Light Mode
                        else if (item == 1) isDarkMode = true;   // Dark Mode
                    }

                    // clicking outside of the menu only closes it
                    if (item != -1 || mouseY >= TOP_BAR_HEIGHT) continue;
                }

                if (mouseY < TOP_BAR_HEIGHT) {
                    // top bar buttons
                    for (int i = 0; i < (int)topButtons.size(); i++) {
                        if (mouseX < topButtons[i].x - 5 || mouseX > topButtons[i].x + topButtons[i].width + 5) continue;

                        if (i == BTN_FILE || i == BTN_EDIT || i == BTN_VIEW) {
                            openMenu = (menuThatWasOpen == i) ? -1 : i;  // Toggle the visibility of the menu
                        } else if (i == BTN_COMPILE) {
                            compileRequested = true;
                        } else if (i == BTN_RUN) {
                            runRequested = true;
                        } else if (i == BTN_HELP) {
                            helpRequested = true;
                        }
                    }
                } else if (mouseX < PROJECT_PANEL_WIDTH) {
                    // project list (only the visible part, the title is not clickable)
                    if (mouseY > TOP_BAR_HEIGHT + 35) {
                        for (size_t i = 0; i < savedProjects.size(); i++) {
                            if (isInside(mouseX, mouseY, savedProjects[i].rect)) openProjectIndex = i;
                        }
                    }
                } else if (mouseY < CODE_BOTTOM) {
                    // click in the code -> move the cursor there (and start a mouse selection)
                    int line, column;
                    mouseToTextPosition(editor, mouseX, mouseY, charWidth, line, column);

                    if (SDL_GetModState() & KMOD_SHIFT) {
                        if (!editor.selectionActive) {
                            editor.selectionActive = true;
                            editor.selectionStartLine = editor.currentLine;
                            editor.selectionStartPos = editor.cursorPos;
                        }
                    } else {
                        editor.selectionActive = true;
                        editor.selectionStartLine = line;
                        editor.selectionStartPos = column;
                    }
                    editor.currentLine = line;
                    editor.cursorPos = column;
                    mouseSelecting = true;
                    cursorVisible = true;
                    lastCursorToggle = SDL_GetTicks();
                }
            } else if (e.type == SDL_MOUSEMOTION && mouseSelecting) {
                int line, column;
                mouseToTextPosition(editor, e.motion.x, e.motion.y, charWidth, line, column);
                editor.currentLine = line;
                editor.cursorPos = column;
                ensureCursorVisible(editor);
            } else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                mouseSelecting = false;
            }
        }

        string currentCode = getCode(editor.lines);

        // run the checks only when the code changes (they are too slow to run every frame)
        if (currentCode != lastAnalyzedCode) {
            analyzeCode(editor.lines, collectedErrors, collectedWarnings, declaredVariables);
            compilerMessages.clear();  // old compiler output is not valid anymore
            lastAnalyzedCode = currentCode;
        }
        bool unsavedChanges = (currentCode != savedCode);

        //--------------------------
        //---Menu / Button actions--
        //--------------------------

        if (newProjectRequested) {
            if (!unsavedChanges || askYesNo(window, "New Project", "You have unsaved changes. Discard them?")) {
                editor = EditorState();
                historyStack.clear();
                redoStack.clear();
                savedCode = getCode(editor.lines);
            }
        }

        if (openProjectIndex != -1) {
            ProjectInfo project = savedProjects[openProjectIndex];
            if (!unsavedChanges || askYesNo(window, "Open Project", "You have unsaved changes. Discard them?")) {
                vector<string> loadedLines;
                if (readFileToLines(project.filePath, loadedLines)) {
                    editor = EditorState();
                    editor.lines = loadedLines;
                    editor.projectName = project.name;
                    editor.projectPath = project.filePath;
                    historyStack.clear();
                    redoStack.clear();
                    savedCode = getCode(editor.lines);
                } else {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                             ("Failed to open project: " + project.name + "\n" + project.filePath).c_str(),
                                             window);
                }
            }
        }

        if (saveRequested) {
            if (!editor.projectPath.empty()) {
                // already has a file -> just save it
                if (writeLinesToFile(editor.projectPath, editor.lines))
                    savedCode = currentCode;
                else
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not write the file.", window);
            } else {
                vector<string> values;
                if (showInputDialog("Save Project", {"Project name:"}, values, "Save", font)) {
                    string projectName = trim(values[0]);
                    if (!isValidProjectName(projectName)) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning",
                                                 "Invalid project name.\nDon't use these characters: \\ / : * ? \" < > | ,", window);
                    } else if (findProject(projectName) != -1) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning",
                                                 "A project with this name already exists.\nUse Save As if you want to replace it.", window);
                    } else if (saveProject(projectName, editor.lines)) {
                        editor.projectName = projectName;
                        editor.projectPath = projectName + ".cpp";
                        savedCode = currentCode;
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Success", "Project saved successfully", window);
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not write the file.", window);
                    }
                }
                SDL_RaiseWindow(window);
            }
        }

        if (saveAsRequested) {
            vector<string> values = {editor.projectName, ""};
            if (showInputDialog("Save Project As", {"Project name:", "Folder (empty = current folder):"}, values, "Save", font)) {
                string projectName = trim(values[0]);
                string folder = trim(values[1]);
                string filePath;

                if (!isValidProjectName(projectName)) {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning",
                                             "Invalid project name.\nDon't use these characters: \\ / : * ? \" < > | ,", window);
                } else if (saveAsProject(projectName, editor.lines, folder, filePath)) {
                    editor.projectName = projectName;
                    editor.projectPath = filePath;
                    savedCode = currentCode;
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Success", "Project saved successfully", window);
                } else {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                                             "Could not write the file. Check that the folder exists.", window);
                }
            }
            SDL_RaiseWindow(window);
        }

        if (goToLineRequested) {
            vector<string> values;
            string label = "Line number (1 - " + to_string(editor.lines.size()) + "):";
            if (showInputDialog("Go to Line", {label}, values, "Go", font, true) && !values[0].empty()) {
                int lineNumber = -1;
                if (values[0].size() < 9) lineNumber = stoi(values[0]) - 1;  // Convert to 0-based indexing

                if (lineNumber >= 0 && lineNumber < (int)editor.lines.size()) {
                    editor.currentLine = lineNumber;
                    editor.cursorPos = editor.lines[lineNumber].size(); // Go to the end of the line
                    editor.selectionActive = false;
                    ensureCursorVisible(editor);
                } else {
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Invalid Line Number", "Line number is out of range.", window);
                }
            }
            SDL_RaiseWindow(window);
        }

        if (compileRequested || runRequested) {
            SDL_SetWindowTitle(window, "CMorgh IDE - compiling...");
            lastTitle = "";

            bool compiled = compileCode(currentCode, "temp_project", compilerMessages);
            errorScrollOffset = 0;

            if (!compiled) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Compilation Error",
                                         "Compilation failed. Check the error panel for the details.", window);
            } else if (runRequested) {
                runProgram("temp_project");
            } else if (compilerMessages.empty()) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Debug & Compile",
                                         "Compiled successfully, no errors or warnings.", window);
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Debug & Compile",
                                         "Compiled successfully, but there are some warnings (see the error panel).", window);
            }
        }

        if (helpRequested) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Help", HELP_TEXT, window);
        }

        if (quitRequested) {
            if (!unsavedChanges || askYesNo(window, "Exit", "You have unsaved changes. Exit anyway?")) {
                quit = true;
                break;
            }
        }

        // the actions above can change the text, so check again
        currentCode = getCode(editor.lines);
        unsavedChanges = (currentCode != savedCode);

        string title = "CMorgh IDE - " + (editor.projectName.empty() ? string("untitled") : editor.projectName);
        if (unsavedChanges) title += " *";
        if (title != lastTitle) {
            SDL_SetWindowTitle(window, title.c_str());
            lastTitle = title;
        }

        Uint32 currentTime = SDL_GetTicks();
        if (currentTime > lastCursorToggle + CURSOR_BLINK_INTERVAL) {
            cursorVisible = !cursorVisible;
            lastCursorToggle = currentTime;
        }

        // keep the cursor visible when the line is longer than the screen
        int cursorPixelX = editor.cursorPos * charWidth;
        int codeViewWidth = SCREEN_WIDTH - CODE_X - 20;
        if (cursorPixelX - editor.scrollX > codeViewWidth) {
            editor.scrollX = cursorPixelX - codeViewWidth;
        } else if (cursorPixelX < editor.scrollX) {
            editor.scrollX = max(0, cursorPixelX - codeViewWidth / 3);
        }

        int maxCodeScrollOffset = max(0, (int)editor.lines.size() * LINE_HEIGHT - (CODE_BOTTOM - CODE_TOP));
        editor.scrollOffset = max(0, min(maxCodeScrollOffset, editor.scrollOffset));


        //-----------------
        //----Rendering----
        //-----------------

        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        SDL_Color textColor, backgroundColor;
        Uint8 navbarColor, panelColor, currentLineColor;
        // Set the colors based on the current mode
        if (isDarkMode) {
            backgroundColor = darkBackgroundColor;
            textColor = darkTextColor;
            navbarColor = 50;
            panelColor = 40;
            currentLineColor = 38;
        } else {
            backgroundColor = lightBackgroundColor;
            textColor = lightTextColor;
            navbarColor = 180;
            panelColor = 100;
            currentLineColor = 205;
        }
        SDL_Color white = {255, 255, 255, 255};
        SDL_Color gray = {140, 140, 140, 255};

        SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, 255);
        SDL_RenderClear(renderer);

        //-----------------
        //---Code Area-----
        //-----------------

        SDL_Rect codeArea = {PROJECT_PANEL_WIDTH, TOP_BAR_HEIGHT, SCREEN_WIDTH - PROJECT_PANEL_WIDTH, CODE_BOTTOM - TOP_BAR_HEIGHT};
        SDL_Rect textArea = {CODE_X - 4, TOP_BAR_HEIGHT, SCREEN_WIDTH - CODE_X + 4, CODE_BOTTOM - TOP_BAR_HEIGHT};

        // highlight the line with the cursor
        SDL_RenderSetClipRect(renderer, &codeArea);
        SDL_Rect currentLineRect = {PROJECT_PANEL_WIDTH, CODE_TOP + editor.currentLine * LINE_HEIGHT - editor.scrollOffset,
                                    SCREEN_WIDTH - PROJECT_PANEL_WIDTH, LINE_HEIGHT};
        SDL_SetRenderDrawColor(renderer, currentLineColor, currentLineColor, currentLineColor, 255);
        SDL_RenderFillRect(renderer, &currentLineRect);

        SDL_RenderSetClipRect(renderer, &textArea);

        // Draw selection rectangles if selection is active
        if (hasSelection(editor)) {
            int startLine, startPos, endLine, endPos;
            getSelectionRange(editor, startLine, startPos, endLine, endPos);

            for (int line = startLine; line <= endLine; ++line) {
                int yPos = CODE_TOP + line * LINE_HEIGHT - editor.scrollOffset;
                if (yPos + LINE_HEIGHT <= TOP_BAR_HEIGHT || yPos >= CODE_BOTTOM) continue;

                int leftX = (line == startLine) ? startPos : 0;
                int rightX = (line == endLine) ? endPos : editor.lines[line].size() + 1;  // +1 to show the new line is selected

                SDL_Rect selectionRect = {CODE_X + leftX * charWidth - editor.scrollX, yPos,
                                          (rightX - leftX) * charWidth, LINE_HEIGHT};
                SDL_SetRenderDrawColor(renderer, 0, 100, 255, 90); // Blue color
                SDL_RenderFillRect(renderer, &selectionRect);
            }
        }

        // Render the code (only the visible lines, but block comments need all lines before)
        bool inBlockComment = false;
        for (int i = 0; i < (int)editor.lines.size(); ++i) {
            int y = CODE_TOP + i * LINE_HEIGHT - editor.scrollOffset;
            if (y >= CODE_BOTTOM) break;

            vector<Token> tokens = tokenizeLine(editor.lines[i], inBlockComment, declaredVariables);
            if (y + LINE_HEIGHT <= TOP_BAR_HEIGHT) continue;

            // Render each token with its corresponding color
            for (const Token &token : tokens) {
                int x = CODE_X + token.column * charWidth - editor.scrollX;
                if (x > SCREEN_WIDTH) break;
                if (x + (int)token.text.size() * charWidth < CODE_X) continue;
                renderText(renderer, codefont, token.text, getCurrentColors()[token.type], x, y + 1);
            }
        }

        // Render the cursor
        int cursorY = CODE_TOP + editor.currentLine * LINE_HEIGHT - editor.scrollOffset;
        if (cursorVisible) {
            int cursorX = CODE_X + editor.cursorPos * charWidth - editor.scrollX;
            SDL_Rect cursorRect = {cursorX, cursorY, 2, LINE_HEIGHT};
            SDL_SetRenderDrawColor(renderer, textColor.r, textColor.g, textColor.b, 255);
            SDL_RenderFillRect(renderer, &cursorRect);
        }

        // Render line numbers (right aligned before the code)
        SDL_Rect lineNumberArea = {PROJECT_PANEL_WIDTH, TOP_BAR_HEIGHT, CODE_X - PROJECT_PANEL_WIDTH - 4, CODE_BOTTOM - TOP_BAR_HEIGHT};
        SDL_RenderSetClipRect(renderer, &lineNumberArea);
        for (int i = editor.scrollOffset / LINE_HEIGHT; i < (int)editor.lines.size(); ++i) {
            int y = CODE_TOP + i * LINE_HEIGHT - editor.scrollOffset;
            if (y >= CODE_BOTTOM) break;

            string number = to_string(i + 1);
            int numberWidth = 0;
            TTF_SizeText(codefont, number.c_str(), &numberWidth, nullptr);
            renderText(renderer, codefont, number, (i == editor.currentLine) ? textColor : gray,
                       CODE_X - 12 - numberWidth, y + 1);
        }
        SDL_RenderSetClipRect(renderer, nullptr);


        //--------------------
        //---Projects Panel---
        //--------------------

        SDL_SetRenderDrawColor(renderer, panelColor, panelColor, panelColor, 255);
        SDL_Rect projectPanel = {0, TOP_BAR_HEIGHT, PROJECT_PANEL_WIDTH, SCREEN_HEIGHT - TOP_BAR_HEIGHT};
        SDL_RenderFillRect(renderer, &projectPanel);
        renderText(renderer, font, "Projects", white, 15, TOP_BAR_HEIGHT + 8);

        int listTop = TOP_BAR_HEIGHT + 38;
        int itemHeight = 32;
        int maxProjectScrollOffset = max(0, (int)savedProjects.size() * itemHeight - (SCREEN_HEIGHT - listTop));
        projectScrollOffset = max(0, min(maxProjectScrollOffset, projectScrollOffset));

        SDL_Rect projectListArea = {0, listTop, PROJECT_PANEL_WIDTH, SCREEN_HEIGHT - listTop};
        SDL_RenderSetClipRect(renderer, &projectListArea);

        if (savedProjects.empty()) {
            renderText(renderer, font, "No project saved...", white, 15, listTop + 5);
        } else {
            for (size_t i = 0; i < savedProjects.size(); i++) {
                int itemY = listTop + (int)i * itemHeight - projectScrollOffset;
                savedProjects[i].rect = {10, itemY, PROJECT_PANEL_WIDTH - 20, itemHeight - 4};

                if (itemY + itemHeight < listTop || itemY > SCREEN_HEIGHT) continue;

                bool isOpen = (savedProjects[i].filePath == editor.projectPath);
                bool hover = isInside(mouseX, mouseY, savedProjects[i].rect) && mouseY > listTop;
                if (isOpen) SDL_SetRenderDrawColor(renderer, 0, 90, 160, 255);
                else if (hover) SDL_SetRenderDrawColor(renderer, panelColor + 25, panelColor + 25, panelColor + 25, 255);
                else SDL_SetRenderDrawColor(renderer, panelColor - 20, panelColor - 20, panelColor - 20, 255);
                SDL_RenderFillRect(renderer, &savedProjects[i].rect);

                SDL_SetRenderDrawColor(renderer, panelColor + 20, panelColor + 20, panelColor + 20, 255);
                SDL_RenderDrawRect(renderer, &savedProjects[i].rect);

                // cut long names so they fit in the box
                string name = savedProjects[i].name;
                int nameWidth = 0;
                TTF_SizeText(font, name.c_str(), &nameWidth, nullptr);
                while (name.size() > 1 && nameWidth > savedProjects[i].rect.w - 15) {
                    name.pop_back();
                    TTF_SizeText(font, (name + "..").c_str(), &nameWidth, nullptr);
                }
                if (name != savedProjects[i].name) name += "..";
                renderText(renderer, font, name, white, 18, itemY + 3);
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);


        //-----------------
        //---Error Panel---
        //-----------------

        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect errorPanel = {PROJECT_PANEL_WIDTH, CODE_BOTTOM, SCREEN_WIDTH - PROJECT_PANEL_WIDTH, ERROR_PANEL_HEIGHT};
        SDL_RenderFillRect(renderer, &errorPanel);
        SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
        SDL_RenderDrawLine(renderer, PROJECT_PANEL_WIDTH, CODE_BOTTOM, SCREEN_WIDTH, CODE_BOTTOM);

        int errorCount = collectedErrors.size() + compilerMessages.size();
        string header = "Problems:  " + to_string(errorCount) + " errors,  " +
                        to_string(collectedWarnings.size()) + " warnings";
        renderText(renderer, font, header, white, 210, CODE_BOTTOM + 5);

        // all messages in one list with their color
        vector<pair<string, SDL_Color>> messages;
        for (const string &message : compilerMessages) messages.push_back({message, {255, 140, 60, 255}});
        for (const string &message : collectedErrors) messages.push_back({message, {255, 90, 90, 255}});
        for (const string &message : collectedWarnings) messages.push_back({message, {255, 220, 0, 255}});
        if (messages.empty()) messages.push_back({"No problems found.", {120, 220, 120, 255}});

        int messageTop = CODE_BOTTOM + 32;
        int messageHeight = TTF_FontLineSkip(font) + 2;
        int maxErrorScrollOffset = max(0, (int)messages.size() * messageHeight - (SCREEN_HEIGHT - messageTop - 5));
        errorScrollOffset = max(0, min(maxErrorScrollOffset, errorScrollOffset));

        SDL_Rect messageArea = {PROJECT_PANEL_WIDTH, messageTop, SCREEN_WIDTH - PROJECT_PANEL_WIDTH, SCREEN_HEIGHT - messageTop};
        SDL_RenderSetClipRect(renderer, &messageArea);
        int errorY = messageTop - errorScrollOffset;
        for (const auto &message : messages) {
            if (errorY + messageHeight > messageTop && errorY < SCREEN_HEIGHT) {
                renderText(renderer, font, message.first, message.second, 210, errorY);
            }
            errorY += messageHeight;
        }
        SDL_RenderSetClipRect(renderer, nullptr);


        //-----------------
        //-----Top Bar-----
        //-----------------

        SDL_SetRenderDrawColor(renderer, navbarColor, navbarColor, navbarColor, 255);
        SDL_Rect topBar = {0, 0, SCREEN_WIDTH, TOP_BAR_HEIGHT};
        SDL_RenderFillRect(renderer, &topBar);

        for (int i = 0; i < (int)topButtons.size(); i++) {
            SDL_Rect buttonRect = {topButtons[i].x - 5, 4, topButtons[i].width + 10, TOP_BAR_HEIGHT - 8};
            if (isInside(mouseX, mouseY, buttonRect) || openMenu == i) {
                Uint8 c = isDarkMode ? navbarColor + 25 : navbarColor - 25;
                SDL_SetRenderDrawColor(renderer, c, c, c, 255);
                SDL_RenderFillRect(renderer, &buttonRect);
            }

            SDL_Color color = textColor;
            if (i == BTN_RUN) color = isDarkMode ? SDL_Color{110, 220, 110, 255} : SDL_Color{0, 120, 0, 255};
            renderText(renderer, font, topButtons[i].text, color, topButtons[i].x, 7);
        }

        // dropdown menu
        if (openMenu != -1) {
            vector<string> &items = (openMenu == BTN_FILE) ? fileMenuItems :
                                    (openMenu == BTN_EDIT) ? editMenuItems : viewMenuItems;
            int menuX = topButtons[openMenu].x - 5;

            SDL_Rect menuRect = {menuX, TOP_BAR_HEIGHT, MENU_WIDTH, (int)items.size() * MENU_ITEM_HEIGHT};
            SDL_SetRenderDrawColor(renderer, navbarColor, navbarColor, navbarColor, 255);
            SDL_RenderFillRect(renderer, &menuRect);

            int hoverItem = getMenuItemAt(mouseX, mouseY, menuX, items.size());
            for (int i = 0; i < (int)items.size(); i++) {
                SDL_Rect itemRect = {menuX, TOP_BAR_HEIGHT + i * MENU_ITEM_HEIGHT, MENU_WIDTH, MENU_ITEM_HEIGHT};
                if (i == hoverItem) {
                    SDL_SetRenderDrawColor(renderer, 0, 110, 200, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                }
                // show a check next to the current mode
                string text = items[i];
                if (openMenu == BTN_VIEW && (i == 1) == isDarkMode) text += "  *";
                renderText(renderer, font, text, (i == hoverItem) ? white : textColor, menuX + 10, itemRect.y + 6);
            }

            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(renderer, &menuRect);
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
