#pragma once
// Core UI toolkit: rendering, input, text, fields, files, logos and icons.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include "fontdata.h"

static const int W = 1280, H = 720;
static SDL_Window* gWin = nullptr;
static SDL_Renderer* R = nullptr;

struct Col { Uint8 r, g, b, a; };
static inline Col C(int r, int g, int b, int a = 255) { return Col{(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a}; }
static inline Col HX(unsigned v, int a = 255) { return C((v >> 16) & 255, (v >> 8) & 255, v & 255, a); }
static inline Col alpha(Col c, int a) { c.a = (Uint8)a; return c; }
static inline Col mixc(Col a, Col b, float t) {
    return C((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t));
}
struct Rect { int x, y, w, h; };
static inline bool hit(const Rect& r, int px, int py) { return px >= r.x && py >= r.y && px < r.x + r.w && py < r.y + r.h; }

// ---------------------------------------------------------------- input
struct KeyEv { int type; std::string s; };  // 0 text 1 bksp 2 enter 3 left 4 right 5 up 6 down 7 delete
static std::vector<KeyEv> keyq;

struct Input {
    int x = 0, y = 0, sx = 0, sy = 0, dx = 0, dy = 0, wheel = 0;
    bool down = false, pressed = false, released = false, moved = false, blocked = false;
    bool btnA = false, btnB = false, btnPlus = false;
    int dpadX = 0, dpadY = 0;
    bool quit = false;
    Uint32 downT = 0;
} in;

static void frameReset() {
    in.pressed = in.released = false; in.dx = in.dy = 0; in.wheel = 0;
    in.btnA = in.btnB = in.btnPlus = false; in.dpadX = in.dpadY = 0;
}
static void pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: in.quit = true; break;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                in.down = true; in.pressed = true; in.moved = false; in.blocked = false;
                in.x = in.sx = e.button.x; in.y = in.sy = e.button.y; in.downT = SDL_GetTicks();
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) { in.down = false; in.released = true; in.x = e.button.x; in.y = e.button.y; }
            break;
        case SDL_MOUSEMOTION:
            in.x = e.motion.x; in.y = e.motion.y;
            if (in.down) {
                in.dx += e.motion.xrel; in.dy += e.motion.yrel;
                if (abs(in.x - in.sx) > 14 || abs(in.y - in.sy) > 14) in.moved = true;
            }
            break;
        case SDL_MOUSEWHEEL: in.wheel += e.wheel.y; break;
        case SDL_TEXTINPUT: keyq.push_back({0, e.text.text}); break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_BACKSPACE: keyq.push_back({1, ""}); break;
            case SDLK_RETURN: keyq.push_back({2, ""}); break;
            case SDLK_LEFT: keyq.push_back({3, ""}); break;
            case SDLK_RIGHT: keyq.push_back({4, ""}); break;
            case SDLK_UP: keyq.push_back({5, ""}); break;
            case SDLK_DOWN: keyq.push_back({6, ""}); break;
            case SDLK_DELETE: keyq.push_back({7, ""}); break;
            case SDLK_ESCAPE: in.btnB = true; break;
            }
            break;
        case SDL_JOYBUTTONDOWN:
            switch (e.jbutton.button) {
            case 0: in.btnA = true; break;
            case 1: in.btnB = true; break;
            case 10: in.btnPlus = true; break;
            case 12: in.dpadX = -1; break;
            case 13: in.dpadY = -1; break;
            case 14: in.dpadX = 1; break;
            case 15: in.dpadY = 1; break;
            }
            break;
        }
    }
}
static inline bool tapIn(const Rect& r) { return in.released && !in.moved && !in.blocked && hit(r, in.sx, in.sy) && hit(r, in.x, in.y); }
static inline bool pressIn(const Rect& r) { return in.down && !in.moved && !in.blocked && hit(r, in.sx, in.sy); }
static inline bool dragIn(const Rect& r) { return in.down && in.moved && !in.blocked && hit(r, in.sx, in.sy); }

// ---------------------------------------------------------------- utf8
static int u8prev(const std::string& s, int i) { if (i <= 0) return 0; i--; while (i > 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--; return i; }
static int u8next(const std::string& s, int i) { int n = (int)s.size(); if (i >= n) return n; i++; while (i < n && (((unsigned char)s[i]) & 0xC0) == 0x80) i++; return i; }

// ---------------------------------------------------------------- text
enum { LIGHT = 0, REG = 1, BOLD = 2 };
enum { STY_I = 1, STY_U = 2 };
static TTF_Font* getFont(int size, int weight, int sty) {
    static std::map<int, TTF_Font*> fm;
    int key = size * 100 + weight * 10 + sty;
    auto it = fm.find(key); if (it != fm.end()) return it->second;
    const unsigned char* d = font_reg; unsigned n = font_reg_len;
    if (weight == LIGHT) { d = font_light; n = font_light_len; }
    if (weight == BOLD) { d = font_bold; n = font_bold_len; }
    TTF_Font* f = TTF_OpenFontRW(SDL_RWFromConstMem(d, (int)n), 0, size);
    if (f) TTF_SetFontStyle(f, ((sty & 1) ? TTF_STYLE_ITALIC : 0) | ((sty & 2) ? TTF_STYLE_UNDERLINE : 0));
    fm[key] = f; return f;
}
struct TexE { SDL_Texture* t; int w, h; };
static std::unordered_map<std::string, TexE> tcache;
static std::unordered_map<std::string, int> wcache;
static std::string tkey(const std::string& s, int size, int weight, int sty) {
    char b[32]; snprintf(b, sizeof b, "%d,%d,%d|", size, weight, sty); return std::string(b) + s;
}
static int textW(const std::string& s, int size, int weight = REG, int sty = 0) {
    if (s.empty()) return 0;
    std::string k = tkey(s, size, weight, sty);
    auto it = wcache.find(k); if (it != wcache.end()) return it->second;
    int w = 0, h = 0; TTF_SizeUTF8(getFont(size, weight, sty), s.c_str(), &w, &h);
    if (wcache.size() > 30000) wcache.clear();
    wcache[k] = w; return w;
}
static int textH(int size) { return (int)(size * 1.3f); }
// align: 0 left, 1 center, 2 right. y = top of the line box.
static int text(const std::string& s, int x, int y, int size, Col c, int weight = REG, int align = 0, int sty = 0) {
    if (s.empty()) return 0;
    std::string k = tkey(s, size, weight, sty);
    auto it = tcache.find(k);
    if (it == tcache.end()) {
        if (tcache.size() > 2500) { for (auto& p : tcache) SDL_DestroyTexture(p.second.t); tcache.clear(); }
        SDL_Surface* sf = TTF_RenderUTF8_Blended(getFont(size, weight, sty), s.c_str(), SDL_Color{255, 255, 255, 255});
        if (!sf) return 0;
        SDL_Texture* t = SDL_CreateTextureFromSurface(R, sf);
        TexE e{t, sf->w, sf->h}; SDL_FreeSurface(sf);
        it = tcache.emplace(k, e).first;
    }
    TexE& e = it->second;
    int dx = x; if (align == 1) dx = x - e.w / 2; else if (align == 2) dx = x - e.w;
    SDL_SetTextureColorMod(e.t, c.r, c.g, c.b); SDL_SetTextureAlphaMod(e.t, c.a);
    SDL_Rect d{dx, y, e.w, e.h}; SDL_RenderCopy(R, e.t, nullptr, &d);
    return e.w;
}
static std::string fitText(const std::string& s, int maxw, int size, int weight = REG) {
    if (textW(s, size, weight) <= maxw) return s;
    std::string t = s;
    while (!t.empty() && textW(t + "...", size, weight) > maxw) t = t.substr(0, u8prev(t, (int)t.size()));
    return t + "...";
}

// ---------------------------------------------------------------- primitives
static void setCol(Col c) { SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a); }
static void fillRect(Rect r, Col c) { setCol(c); SDL_Rect q{r.x, r.y, r.w, r.h}; SDL_RenderFillRect(R, &q); }
static void frameRect(Rect r, Col c, int t = 1) {
    fillRect({r.x, r.y, r.w, t}, c); fillRect({r.x, r.y + r.h - t, r.w, t}, c);
    fillRect({r.x, r.y, t, r.h}, c); fillRect({r.x + r.w - t, r.y, t, r.h}, c);
}
static void hline(int x, int y, int w, Col c) { fillRect({x, y, w, 1}, c); }
static void vline(int x, int y, int h, Col c) { fillRect({x, y, 1, h}, c); }
static void lineP(float x1, float y1, float x2, float y2, Col c, float t = 2) {
    float dx = x2 - x1, dy = y2 - y1, l = sqrtf(dx * dx + dy * dy); if (l < 0.01f) return;
    float nx = -dy / l * t / 2, ny = dx / l * t / 2;
    SDL_Vertex v[4];
    float px[4] = {x1 + nx, x1 - nx, x2 - nx, x2 + nx}, py[4] = {y1 + ny, y1 - ny, y2 - ny, y2 + ny};
    for (int i = 0; i < 4; i++) { v[i].position = {px[i], py[i]}; v[i].color = {c.r, c.g, c.b, c.a}; v[i].tex_coord = {0, 0}; }
    int idx[6] = {0, 1, 2, 0, 2, 3}; SDL_RenderGeometry(R, nullptr, v, 4, idx, 6);
}
static void poly(const std::vector<SDL_FPoint>& p, Col c) {
    int n = (int)p.size(); if (n < 3) return;
    std::vector<SDL_Vertex> v(n); std::vector<int> idx;
    for (int i = 0; i < n; i++) { v[i].position = p[i]; v[i].color = {c.r, c.g, c.b, c.a}; v[i].tex_coord = {0, 0}; }
    for (int i = 1; i < n - 1; i++) { idx.push_back(0); idx.push_back(i); idx.push_back(i + 1); }
    SDL_RenderGeometry(R, nullptr, v.data(), n, idx.data(), (int)idx.size());
}
static void circle(float cx, float cy, float r, Col c, float a0 = 0, float a1 = 360) {
    std::vector<SDL_FPoint> p; p.push_back({cx, cy});
    int seg = std::max(8, (int)(r * 1.2f));
    for (int i = 0; i <= seg; i++) { float a = (a0 + (a1 - a0) * i / seg) * 3.14159265f / 180; p.push_back({cx + cosf(a) * r, cy + sinf(a) * r}); }
    poly(p, c);
}
static void clip(Rect r) { SDL_Rect q{r.x, r.y, r.w, r.h}; SDL_RenderSetClipRect(R, &q); }
static void unclip() { SDL_RenderSetClipRect(R, nullptr); }

// ---------------------------------------------------------------- scrolling
struct Scroll { float x = 0, y = 0, vx = 0, vy = 0; };
static void scrollUpdate(Scroll& s, Rect view, int contentW, int contentH, bool horiz = false) {
    bool drag = dragIn(view);
    if (drag) { s.y -= in.dy; s.vy = (float)-in.dy; if (horiz) { s.x -= in.dx; s.vx = (float)-in.dx; } }
    else if (!in.down) { s.y += s.vy; s.vy *= 0.9f; if (fabsf(s.vy) < 0.4f) s.vy = 0; if (horiz) { s.x += s.vx; s.vx *= 0.9f; if (fabsf(s.vx) < 0.4f) s.vx = 0; } }
    if (!in.blocked && hit(view, in.x, in.y) && in.wheel) s.y -= in.wheel * 60;
    float maxY = (float)std::max(0, contentH - view.h), maxX = (float)std::max(0, contentW - view.w);
    if (s.y < 0) { s.y = 0; s.vy = 0; } if (s.y > maxY) { s.y = maxY; s.vy = 0; }
    if (s.x < 0) { s.x = 0; s.vx = 0; } if (s.x > maxX) { s.x = maxX; s.vx = 0; }
}

// ---------------------------------------------------------------- toast
static std::string toastMsg; static Uint32 toastT = 0;
static void toast(const std::string& m) { toastMsg = m; toastT = SDL_GetTicks(); }

// ---------------------------------------------------------------- focus / text fields
static const void* gFocus = nullptr;
static bool oskWant = false;
static void setFocus(const void* p, bool osk = true) { gFocus = p; if (osk) oskWant = true; }
static void clearFocus() { gFocus = nullptr; oskWant = false; }
static inline bool isFocus(const void* p) { return gFocus == p; }
static bool blinkOn() { return (SDL_GetTicks() / 500) % 2 == 0; }

struct Field { std::string t; int cur = 0; };
// Returns true when Enter was pressed
static bool fieldEdit(Field& f) {
    bool enter = false;
    for (auto& k : keyq) {
        if (f.cur > (int)f.t.size()) f.cur = (int)f.t.size();
        switch (k.type) {
        case 0: f.t.insert(f.cur, k.s); f.cur += (int)k.s.size(); break;
        case 1: if (f.cur > 0) { int p = u8prev(f.t, f.cur); f.t.erase(p, f.cur - p); f.cur = p; } break;
        case 7: if (f.cur < (int)f.t.size()) { int n = u8next(f.t, f.cur); f.t.erase(f.cur, n - f.cur); } break;
        case 2: enter = true; break;
        case 3: f.cur = u8prev(f.t, f.cur); break;
        case 4: f.cur = u8next(f.t, f.cur); break;
        }
    }
    return enter;
}
static int colAtX(const std::string& s, int px, int size, int weight = REG, int sty = 0) {
    int i = 0;
    while (i < (int)s.size()) {
        int n = u8next(s, i);
        int w0 = textW(s.substr(0, i), size, weight, sty), w1 = textW(s.substr(0, n), size, weight, sty);
        if (px < (w0 + w1) / 2) return i;
        i = n;
    }
    return (int)s.size();
}
// Draw + handle a single-line text field. Returns true when Enter pressed while focused.
static bool drawField(Rect r, Field& f, const char* hint, Col accent, int size = 22, bool bordered = true) {
    bool foc = isFocus(&f);
    fillRect(r, C(255, 255, 255));
    if (bordered) frameRect(r, foc ? accent : C(171, 171, 171), foc ? 2 : 1);
    int tx = r.x + 10, ty = r.y + (r.h - textH(size)) / 2;
    clip({r.x + 2, r.y + 2, r.w - 4, r.h - 4});
    int off = 0;
    if (foc) {
        if (f.cur > (int)f.t.size()) f.cur = (int)f.t.size();
        int cw = textW(f.t.substr(0, f.cur), size);
        if (cw > r.w - 30) off = cw - (r.w - 30);
    }
    if (f.t.empty() && !foc) text(hint, tx, ty, size, C(150, 150, 150));
    else text(f.t, tx - off, ty, size, C(30, 30, 30));
    if (foc && blinkOn()) {
        int cw = textW(f.t.substr(0, f.cur), size);
        fillRect({tx - off + cw, ty + 2, 2, textH(size) - 4}, C(30, 30, 30));
    }
    unclip();
    if (tapIn(r)) {
        setFocus(&f);
        f.cur = colAtX(f.t, in.sx - tx, size);
    }
    bool en = false;
    if (foc) en = fieldEdit(f);
    return en;
}

// ---------------------------------------------------------------- files
#ifdef __SWITCH__
static const char* BASE = "sdmc:/switch/Office/";
#else
static const char* BASE = "./sd/Office/";
#endif
static std::string DOCS() { return std::string(BASE) + "Documents/"; }
static void mkdirs(const std::string& p) {
    for (size_t i = 1; i < p.size(); i++) if (p[i] == '/' && p[i - 1] != ':') mkdir(p.substr(0, i).c_str(), 0777);
    mkdir(p.c_str(), 0777);
}
static bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? n : 0); if (n > 0 && fread(&out[0], 1, n, f) != (size_t)n) { fclose(f); return false; }
    fclose(f); return true;
}
static bool writeFile(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
    fwrite(data.data(), 1, data.size(), f); fclose(f); return true;
}
struct FileEnt { std::string name, path, ext; time_t mtime; };
static std::string extOf(const std::string& n) { size_t p = n.rfind('.'); if (p == std::string::npos) return ""; std::string e = n.substr(p + 1); for (auto& c : e) c = (char)tolower(c); return e; }
static std::string stemOf(const std::string& n) { size_t p = n.rfind('.'); return p == std::string::npos ? n : n.substr(0, p); }
static std::vector<FileEnt> listDocs(const std::vector<std::string>& exts) {
    std::vector<FileEnt> v; std::string d = DOCS();
    DIR* dp = opendir(d.c_str()); if (!dp) return v;
    while (dirent* e = readdir(dp)) {
        std::string n = e->d_name; if (n.empty() || n[0] == '.') continue;
        std::string x = extOf(n); bool ok = exts.empty();
        for (auto& q : exts) if (q == x) ok = true;
        if (!ok) continue;
        struct stat st; time_t mt = 0; if (stat((d + n).c_str(), &st) == 0) mt = st.st_mtime;
        v.push_back({n, d + n, x, mt});
    }
    closedir(dp);
    std::sort(v.begin(), v.end(), [](const FileEnt& a, const FileEnt& b) { return a.mtime > b.mtime; });
    return v;
}
static std::string fmtTime(time_t t) {
    struct tm* tm = localtime(&t); if (!tm) return "";
    char b[64]; strftime(b, sizeof b, "%d/%m/%Y %H:%M", tm); return b;
}

// ---------------------------------------------------------------- apps / icons / logos
enum AppId { A_WORD, A_EXCEL, A_PPT, A_ACCESS, A_ONE, A_PDF, A_COUNT };
struct AppInfo { const char* name; const char* letter; unsigned ui, ico; const char* blurb; };
static const AppInfo APPS[A_COUNT] = {
    {"Word", "W", 0x2B579A, 0x2A3A8C, "Create and edit documents"},
    {"Excel", "X", 0x217346, 0x0A6E34, "Build spreadsheets and formulas"},
    {"PowerPoint", "P", 0xD24726, 0xD9531E, "Design and present slides"},
    {"Access", "A", 0xA4373A, 0x9B2D30, "Store and search records"},
    {"OneNote", "N", 0x80397B, 0x66297E, "Keep notes in notebooks"},
    {"PDF Reader", "PDF", 0xC4302B, 0xC4302B, "Read PDF documents"},
};
static const unsigned OFFICE_ORANGE = 0xEB3D01;   // splash screen
static const unsigned OFFICE_RED = 0xD83B01;      // logo / home

static void polyU(const std::vector<SDL_FPoint>& u, float ox, float oy, float s, Col c) {
    std::vector<SDL_FPoint> p; for (auto& q : u) p.push_back({ox + q.x * s, oy + q.y * s}); poly(p, c);
}
// Office logo (the open-door cube). Drawn in unit space 100 x 116.8. h = pixel height.
static void drawOfficeLogo(float x, float y, float h, Col fg, Col bg) {
    float s = h / 116.8f;
    polyU({{0, 23.2f}, {64.3f, 0}, {63.2f, 19.6f}, {23.9f, 38.2f}}, x, y, s, fg);
    polyU({{64.3f, 0}, {100, 8.9f}, {100, 101.8f}, {64.3f, 116.8f}, {63.2f, 100.4f}, {63.2f, 19.6f}}, x, y, s, fg);
    polyU({{0, 92.9f}, {64.3f, 116.8f}, {63.2f, 100.4f}, {23.9f, 90.4f}}, x, y, s, fg);
    polyU({{0, 23.2f}, {23.9f, 38.2f}, {23.9f, 90.4f}, {0, 92.9f}}, x, y, s, fg);
    polyU({{4.6f, 92.1f}, {23.9f, 88.0f}, {63.2f, 100.4f}}, x, y, s, bg);
}
static float officeLogoW(float h) { return h * 100.f / 116.8f; }

// App icon in unit space ~100 x 98: dark door-badge with letter + white page with content.
static void drawAppIcon(int app, float x, float y, float size, bool mono = false, Col tile = C(0, 0, 0)) {
    const AppInfo& a = APPS[app]; float s = size / 100.f;
    Col ico = HX(a.ico), white = C(255, 255, 255);
    // page
    Col pageCol = mono ? alpha(white, 235) : white, ink = mono ? tile : ico;
    int bw = std::max(1, (int)(1.4f * s));
    auto ell = [&](float cx, float cy, float rx, float ry, Col c) {
        std::vector<SDL_FPoint> p; for (int i = 0; i < 28; i++) { float an = i * 6.2831853f / 28; p.push_back({x + (cx + cosf(an) * rx) * s, y + (cy + sinf(an) * ry) * s}); }
        poly(p, c);
    };
    if (app == A_ACCESS) {
        // three stacked database cylinders
        Col outer = mono ? white : ico, inner = mono ? tile : white;
        ell(79, 22, 21, 8, outer); ell(79, 78, 21, 8, outer); fillRect({(int)(x + 58 * s), (int)(y + 22 * s), (int)(42 * s), (int)(56 * s)}, outer);
        ell(79, 22, 17.5f, 5, inner); ell(79, 78, 17.5f, 5, inner); fillRect({(int)(x + 61.5f * s), (int)(y + 22 * s), (int)(35 * s), (int)(56 * s)}, inner);
        for (float ty : {41.f, 60.f}) {
            for (int i = 0; i < 14; i++) {
                float a0 = i * 3.14159265f / 14, a1 = (i + 1) * 3.14159265f / 14;
                lineP(x + (79 + cosf(a0) * 19.5f) * s, y + (ty + sinf(a0) * 6.5f) * s, x + (79 + cosf(a1) * 19.5f) * s, y + (ty + sinf(a1) * 6.5f) * s, outer, 3.4f * s);
            }
        }
    } else {
        Rect pg{(int)(x + 58 * s), (int)(y + 11 * s), (int)(42 * s), (int)(77 * s)};
        if (app == A_ONE) pg.w = (int)(36 * s);
        fillRect(pg, pageCol); if (!mono) frameRect(pg, ico, bw);
        float px = x + 64 * s, pw = 30 * s;
        switch (app) {
        case A_WORD:
            for (int i = 0; i < 7; i++) fillRect({(int)px, (int)(y + (20 + i * 9) * s), (int)pw, std::max(1, (int)(3.2f * s))}, ink);
            break;
        case A_PDF:
            poly({{x + 88 * s, y + 11 * s}, {x + 100 * s, y + 23 * s}, {x + 88 * s, y + 23 * s}}, mono ? alpha(tile, 120) : C(200, 200, 200));
            for (int i = 0; i < 5; i++) fillRect({(int)px, (int)(y + (32 + i * 10) * s), (int)((i == 4 ? 18 : 30) * s), std::max(1, (int)(3.2f * s))}, ink);
            break;
        case A_ONE:
            for (int i = 0; i < 7; i++) fillRect({pg.x, (int)(y + (17 + i * 10) * s), pg.w, std::max(1, (int)(5.5f * s))}, ink);
            for (float ty : {24.f, 46.f, 66.f}) { Rect tb{(int)(x + 94 * s), (int)(y + ty * s), (int)(7 * s), (int)(13 * s)}; fillRect(tb, pageCol); frameRect(tb, ink, bw); }
            break;
        case A_EXCEL:
            for (int i = 0; i < 4; i++) fillRect({(int)px, (int)(y + (20 + i * 17) * s), (int)pw, std::max(2, (int)(7 * s))}, ink);
            for (int i = 0; i < 2; i++) fillRect({(int)(px + (i * 13 + 7) * s), (int)(y + 20 * s), std::max(1, (int)(2.4f * s)), (int)(58 * s)}, ink);
            break;
        case A_PPT:
            circle(px + 11 * s, y + 34 * s, 12 * s, ink, -90, 180);
            circle(px + 13 * s, y + 32 * s, 12 * s, ink, 190, 265);
            fillRect({(int)px, (int)(y + 60 * s), (int)(30 * s), std::max(1, (int)(3.4f * s))}, ink);
            fillRect({(int)px, (int)(y + 70 * s), (int)(22 * s), std::max(1, (int)(3.4f * s))}, ink);
            break;
        }
    }
    // badge (door in perspective)
    Col badge = mono ? white : ico;
    polyU({{0, 9.6f}, {60.4f, 0}, {60.4f, 98.3f}, {0, 86.7f}}, x, y, s, badge);
    Col lc = mono ? tile : white;
    std::string L = a.letter;
    int fs = (int)((L.size() > 1 ? 24 : 54) * s);
    int w = textW(L, fs, BOLD);
    text(L, (int)(x + 28 * s) - w / 2, (int)(y + 48 * s) - textH(fs) / 2, fs, lc, BOLD);
}
static void drawMsLogo(int x, int y, int sz) {
    int g = std::max(1, sz / 12), q = (sz - g) / 2;
    fillRect({x, y, q, q}, HX(0xF25022)); fillRect({x + q + g, y, q, q}, HX(0x7FBA00));
    fillRect({x, y + q + g, q, q}, HX(0x00A4EF)); fillRect({x + q + g, y + q + g, q, q}, HX(0xFFB900));
}
static int drawMsBrand(int x, int y, int sz, Col textCol) {  // logo + wordmark; returns width
    drawMsLogo(x, y, sz);
    int fs = (int)(sz * 1.0f);
    int w = text("Microsoft", x + sz + sz / 2, y + sz / 2 - textH(fs) / 2, fs, textCol, REG);
    return sz + sz / 2 + w;
}
static void drawChevronDown(int cx, int cy, int s, Col c) { lineP(cx - s, cy - s / 2, cx, cy + s / 2, c, 3); lineP(cx, cy + s / 2, cx + s, cy - s / 2, c, 3); }
static void drawChevronLeft(int cx, int cy, int s, Col c) { lineP(cx + s / 2, cy - s, cx - s / 2, cy, c, 3); lineP(cx - s / 2, cy, cx + s / 2, cy + s, c, 3); }
