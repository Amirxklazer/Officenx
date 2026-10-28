// Office for Nintendo Switch - homebrew NRO (C++ / SDL2 / libnx)
// Fan-made tribute. Credits to Microsoft and all of its developers and designers.
#include "ui.h"
#include "osk.h"
#include "app.h"
#include "editor.h"
#include "word.h"
#if __has_include("excel.h")
#include "excel.h"
#define HAVE_EXCEL 1
#endif
#if __has_include("ppt.h")
#include "ppt.h"
#define HAVE_PPT 1
#endif
#if __has_include("access.h")
#include "access.h"
#define HAVE_ACCESS 1
#endif
#if __has_include("onenote.h")
#include "onenote.h"
#define HAVE_ONE 1
#endif
#if __has_include("pdf.h")
#include "pdf.h"
#define HAVE_PDF 1
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif

struct StubApp : App {
    DocEditor ed; int which;
    StubApp(int w) : which(w) { id = w; docName = "Untitled"; }
    const char* ext() override { return "dat"; }
    std::vector<std::string> openExts() override { return {"dat"}; }
    std::vector<const char*> tabNames() override { return {"HOME"}; }
    void ribbon(int, Rect) override {}
    void content(Rect a) override { fillRect(a, C(240, 240, 240)); text("Coming soon", a.x + 40, a.y + 40, 30, C(80, 80, 80)); }
    std::string serialize() override { return ""; }
    bool deserialize(const std::string&) override { return true; }
    void reset() override {}
};
static App* makeApp(int id) {
    switch (id) {
    case A_WORD: return new WordApp();
#ifdef HAVE_EXCEL
    case A_EXCEL: return new ExcelApp();
#endif
#ifdef HAVE_PPT
    case A_PPT: return new PptApp();
#endif
#ifdef HAVE_ACCESS
    case A_ACCESS: return new AccessApp();
#endif
#ifdef HAVE_ONE
    case A_ONE: return new OneNoteApp();
#endif
#ifdef HAVE_PDF
    case A_PDF: return new PdfApp();
#endif
    }
    return new StubApp(id);
}

enum Scr { S_SPLASH, S_HOME, S_APPSPLASH, S_APP, S_ABOUT };
static Scr scr = S_SPLASH; static Uint32 scrT = 0;
static App* apps[A_COUNT]; static App* cur = nullptr;
static int sel = 0; static Scroll aboutScroll, recScroll;
static bool running = true;
static std::string shotName;

static void go(Scr s) { scr = s; scrT = SDL_GetTicks(); }

// ------------------------------------------------------------ loading screen
static void drawSpinner(int cx, int cy, Uint32 t, Col c) {
    for (int i = 0; i < 5; i++) {
        double u = fmod(t / 2200.0 - i * 0.07 + 10.0, 1.0);
        double ang = 360.0 * (0.5 - 0.5 * cos(u * 3.14159265)) - 90.0;
        double rad = ang * 3.14159265 / 180;
        circle((float)(cx + cos(rad) * 24), (float)(cy + sin(rad) * 24), 4.f, c);
    }
}
static void drawSplash(Uint32 t) {
    Col bg = HX(OFFICE_ORANGE);
    fillRect({0, 0, W, H}, bg);
    int a = (int)(255 * std::min(1.f, t / 450.f));
    float lh = 96, lw = officeLogoW(lh); int fs = 64; int tw = textW("Office", fs, LIGHT);
    float total = lw + 26 + tw, x0 = (W - total) / 2, y0 = H * 0.40f - lh / 2;
    drawOfficeLogo(x0, y0, lh, C(255, 255, 255, a), bg);
    text("Office", (int)(x0 + lw + 26), (int)(y0 + lh / 2 - textH(fs) / 2 - 2), fs, C(255, 255, 255, a), LIGHT);
    text("We're getting things ready", W / 2, (int)(H * 0.66f), 30, C(255, 255, 255, (int)(a * 0.75f)), LIGHT, 1);
    drawSpinner(W / 2, (int)(H * 0.80f), t, C(255, 255, 255, a));
}
static void drawAppSplash(int id, Uint32 t) {
    Col bg = HX(APPS[id].ui); fillRect({0, 0, W, H}, bg);
    int a = (int)(255 * std::min(1.f, t / 250.f));
    drawAppIcon(id, W / 2 - 80.f, H * 0.28f, 160, true, bg);
    text(APPS[id].name, W / 2, (int)(H * 0.28f + 190), 56, C(255, 255, 255, a), LIGHT, 1);
    drawSpinner(W / 2, (int)(H * 0.82f), t, C(255, 255, 255, a));
}

// ------------------------------------------------------------ home (Office start page)
static const char* BLANK_LABEL[A_COUNT] = {"Blank document", "Blank workbook", "Blank presentation", "Blank database", "Blank notebook", "Welcome guide"};
static void launch(int id, const std::string& path) {
    cur = apps[id];
    if (path.empty()) { if (id != A_PDF || true) cur->newDoc(); } else cur->openFile(path);
    cur->backstage = false; go(S_APPSPLASH);
}
static void closeApp() {
    if (cur) { if (cur->dirty) cur->save(); }
    clearFocus(); cur = nullptr; go(S_HOME);
}
static void drawHome() {
    Col red = HX(OFFICE_RED);
    fillRect({0, 0, W, H}, C(243, 243, 243));
    int rw = 360; fillRect({0, 0, rw, H}, red);
    drawOfficeLogo(26, 26, 56, C(255, 255, 255), red);
    text("Office", 26 + (int)officeLogoW(56) + 16, 28, 44, C(255, 255, 255), LIGHT);
    for (int i = 0; i < A_COUNT; i++) {
        Rect r{0, 112 + i * 66, rw, 62};
        if (sel == i) fillRect(r, C(255, 255, 255, 46)); else if (pressIn(r)) fillRect(r, C(255, 255, 255, 24));
        drawAppIcon(i, 24, (float)r.y + 8, 46);
        text(APPS[i].name, 92, r.y + 14, 26, C(255, 255, 255), LIGHT);
        if (tapIn(r)) sel = i;
    }
    Rect info{0, H - 142, rw, 50}, ex{0, H - 92, rw, 50};
    for (int k = 0; k < 2; k++) {
        Rect r = k == 0 ? info : ex;
        if (pressIn(r)) fillRect(r, C(255, 255, 255, 30));
        circle(46, (float)r.y + 25, 14, C(255, 255, 255, 0));
        if (k == 0) { circle(46, (float)r.y + 25, 14, C(255, 255, 255)); text("i", 46, r.y + 11, 20, red, BOLD, 1); }
        else { lineP(38, r.y + 17, 54, r.y + 33, C(255, 255, 255), 3); lineP(54, r.y + 17, 38, r.y + 33, C(255, 255, 255), 3); }
        text(k == 0 ? "Info" : "Exit", 78, r.y + 10, 24, C(255, 255, 255), LIGHT);
    }
    if (tapIn(info)) { go(S_ABOUT); aboutScroll = Scroll(); }
    if (tapIn(ex)) running = false;
    drawMsBrand(26, H - 36, 20, C(255, 255, 255));

    // right pane
    int x = rw + 44; Col ac = HX(APPS[sel].ui);
    text(APPS[sel].name, x, 26, 48, ac, LIGHT);
    text(APPS[sel].blurb, x, 86, 20, C(110, 110, 110));
    text("New", x, 130, 22, C(60, 60, 60));
    Rect card{x, 166, 250, 178};
    fillRect(card, C(255, 255, 255)); frameRect(card, pressIn(card) ? ac : C(205, 205, 205), pressIn(card) ? 2 : 1);
    drawAppIcon(sel, card.x + 78.f, card.y + 20.f, 94);
    text(BLANK_LABEL[sel], card.x + card.w / 2, card.y + 130, 19, C(50, 50, 50), REG, 1);
    if (tapIn(card) || in.btnA) launch(sel, "");
    text("Recent", x, 366, 22, C(60, 60, 60));
    Rect lr{x, 402, W - x - 30, H - 412};
    auto files = listDocs(apps[sel]->openExts());
    scrollUpdate(recScroll, lr, lr.w, (int)files.size() * 60);
    clip(lr);
    if (files.empty()) text("No recent files yet. Create one and it will appear here.", x, 410, 19, C(130, 130, 130));
    for (int i = 0; i < (int)files.size(); i++) {
        Rect r{lr.x, lr.y + i * 60 - (int)recScroll.y, lr.w, 56};
        if (r.y + r.h < lr.y || r.y > lr.y + lr.h) continue;
        fillRect(r, pressIn(r) ? C(232, 232, 232) : C(255, 255, 255));
        drawAppIcon(sel, (float)r.x + 8, (float)r.y + 8, 40);
        text(fitText(files[i].name, r.w - 340, 21), r.x + 64, r.y + 6, 21, C(40, 40, 40));
        text(fmtTime(files[i].mtime), r.x + 64, r.y + 32, 14, C(130, 130, 130));
        if (tapIn(r)) launch(sel, files[i].path);
    }
    unclip();
    if (in.dpadY) sel = (sel + in.dpadY + A_COUNT) % A_COUNT;
    if (in.btnPlus) running = false;
}
static void drawAboutScreen() {
    Col red = HX(OFFICE_RED);
    fillRect({0, 0, W, H}, C(255, 255, 255)); fillRect({0, 0, 120, H}, red);
    Rect back{34, 30, 52, 52};
    circle(60, 56, 25, C(255, 255, 255, pressIn(back) ? 90 : 40)); drawChevronLeft(58, 56, 11, C(255, 255, 255));
    if (tapIn(back) || in.btnB) go(S_HOME);
    text("Info", 160, 20, 44, red, LIGHT);
    drawAbout({140, 80, W - 160, H - 90}, red, aboutScroll);
}

// ------------------------------------------------------------ app screen
static void drawApp() {
    App& a = *cur; Col ac = HX(APPS[a.id].ui); bool home = false, close = false;
    a.autosave();
    if (a.backstage) {
        drawBackstage(a, ac, &home);
        if (home) closeApp();
        return;
    }
    if (a.fullscreen()) { a.content(Rect{0, 0, W, H}); return; }
    fillRect({0, 0, W, H}, a.wantsFlatBg() ? C(255, 255, 255) : C(230, 230, 230));
    drawTitleBar(a, ac, &close);
    drawTabs(a, ac);
    a.content(Rect{0, CONTENT_TOP, W, H - oskH() - STATUS_H - CONTENT_TOP});
    drawStatusBar(a, ac);
    if (close) closeApp();
    if (in.btnB && oskWant) oskWant = false;
}

// ------------------------------------------------------------ preview script driver (desktop only)
#ifndef __SWITCH__
struct Cmd { std::string op; std::vector<std::string> a; };
static std::vector<Cmd> script; static size_t sp = 0; static int waitF = 0;
static void loadScript(const char* f) {
    std::string d; if (!readFile(f, d)) return;
    size_t i = 0;
    while (i < d.size()) {
        size_t e = d.find('\n', i); if (e == std::string::npos) e = d.size();
        std::string ln = d.substr(i, e - i); i = e + 1;
        if (ln.empty() || ln[0] == '#') continue;
        Cmd c; size_t p = ln.find(' ');
        c.op = ln.substr(0, p);
        if (c.op == "type" || c.op == "shot") { if (p != std::string::npos) c.a.push_back(ln.substr(p + 1)); }
        else while (p != std::string::npos) { size_t q = ln.find(' ', p + 1); c.a.push_back(ln.substr(p + 1, q == std::string::npos ? q : q - p - 1)); p = q; }
        script.push_back(c);
    }
}
static void pushMouse(int type, int x, int y) {
    SDL_Event e; SDL_zero(e);
    if (type == SDL_MOUSEMOTION) { e.type = type; e.motion.x = x; e.motion.y = y; e.motion.xrel = x - in.x; e.motion.yrel = y - in.y; e.motion.state = SDL_BUTTON_LMASK; }
    else { e.type = type; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y; e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED; }
    SDL_PushEvent(&e);
}
static void runScript() {
    if (waitF > 0) { waitF--; return; }
    while (sp < script.size()) {
        Cmd& c = script[sp++];
        auto n = [&](int k) { return k < (int)c.a.size() ? atoi(c.a[k].c_str()) : 0; };
        if (c.op == "wait") { waitF = n(0); return; }
        else if (c.op == "click") { pushMouse(SDL_MOUSEBUTTONDOWN, n(0), n(1)); pushMouse(SDL_MOUSEBUTTONUP, n(0), n(1)); waitF = 3; return; }
        else if (c.op == "mdown") { pushMouse(SDL_MOUSEBUTTONDOWN, n(0), n(1)); waitF = 1; return; }
        else if (c.op == "mmove") { pushMouse(SDL_MOUSEMOTION, n(0), n(1)); waitF = 1; return; }
        else if (c.op == "mup") { pushMouse(SDL_MOUSEBUTTONUP, n(0), n(1)); waitF = 2; return; }
        else if (c.op == "type") {
            std::string s = c.a.empty() ? "" : c.a[0];
            for (size_t k = 0; k < s.size(); k += 20) { SDL_Event e; SDL_zero(e); e.type = SDL_TEXTINPUT; snprintf(e.text.text, sizeof e.text.text, "%s", s.substr(k, 20).c_str()); SDL_PushEvent(&e); }
            waitF = 2; return;
        }
        else if (c.op == "key") {
            std::string k = c.a.empty() ? "" : c.a[0];
            int t = k == "bksp" ? 1 : k == "enter" ? 2 : k == "left" ? 3 : k == "right" ? 4 : k == "up" ? 5 : k == "down" ? 6 : 7;
            keyq.push_back({t, ""}); waitF = 2; return;
        }
        else if (c.op == "shot") { shotName = c.a.empty() ? "shot" : c.a[0]; waitF = 3; return; }
        else if (c.op == "exit") { running = false; return; }
    }
    running = false;
}
static void saveShot() {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    SDL_RenderReadPixels(R, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    std::string out = "shots/" + shotName + ".bmp"; SDL_SaveBMP(s, out.c_str()); SDL_FreeSurface(s);
    shotName.clear();
}
#endif

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER);
    TTF_Init();
    for (int i = 0; i < SDL_NumJoysticks(); i++) SDL_JoystickOpen(i);
#ifdef __SWITCH__
    gWin = SDL_CreateWindow("Office", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, 0);
    R = SDL_CreateRenderer(gWin, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
    bool scripted = false;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--script") && i + 1 < argc) { loadScript(argv[i + 1]); scripted = true; }
    gWin = SDL_CreateWindow("Office", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, scripted ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    R = SDL_CreateRenderer(gWin, -1, scripted ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_StartTextInput();
#endif
    if (!R) return 1;
    SDL_RenderSetLogicalSize(R, W, H);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    mkdirs(DOCS());
    for (int i = 0; i < A_COUNT; i++) apps[i] = makeApp(i);
    scrT = SDL_GetTicks();
    while (running && !in.quit) {
#ifndef __SWITCH__
        if (scripted) runScript();
#endif
        frameReset(); pollEvents(); oskUpdate();
        Uint32 t = SDL_GetTicks() - scrT;
        switch (scr) {
        case S_SPLASH: drawSplash(t); if (t > 2600 || (in.pressed && t > 600)) go(S_HOME); break;
        case S_HOME: drawHome(); break;
        case S_APPSPLASH: drawAppSplash(cur->id, t); if (t > 650) go(S_APP); break;
        case S_APP: drawApp(); break;
        case S_ABOUT: drawAboutScreen(); break;
        }
        if (scr == S_APP && cur) oskDraw(HX(APPS[cur->id].ui));
        keyq.clear();
#ifndef __SWITCH__
        if (!shotName.empty()) saveShot();
#endif
        SDL_RenderPresent(R);
#ifndef __SWITCH__
        if (scripted) SDL_Delay(6);
#endif
    }
    if (cur && cur->dirty) cur->save();
    TTF_Quit(); SDL_Quit();
    return 0;
}
