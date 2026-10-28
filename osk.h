#pragma once
// Built-in on-screen keyboard (touch). Emits KeyEv into keyq for the focused widget.
#include "ui.h"

struct OKey { const char* label; const char* code; float w; };
static float oskAnim = 0;
static bool oskShift = false, oskCaps = false, oskSym = false, oskCapture = false;
static Uint32 oskShiftT = 0, oskRepT = 0;
static std::string oskPressed;
static const int OSK_H = 262;
static const int OSK_KEY_H = 54, OSK_GAP = 6;
static int oskH() { return (int)(OSK_H * oskAnim + 0.5f); }

static std::vector<std::vector<OKey>> oskLayout() {
    static const char* lo[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    static const char* up[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    static std::vector<std::string> store;  // keeps label strings alive
    store.clear(); store.reserve(128);
    std::vector<std::vector<OKey>> rows(4);
    auto add = [&](int r, const std::string& s, float w) { store.push_back(s); rows[r].push_back({store.back().c_str(), store.back().c_str(), w}); };
    if (!oskSym) {
        const char** L = (oskShift || oskCaps) ? up : lo;
        for (const char* p = L[0]; *p; p++) add(0, std::string(1, *p), 1);
        rows[0].push_back({"", "BS", 2});
        for (const char* p = L[1]; *p; p++) add(1, std::string(1, *p), 1);
        add(1, "'", 1); rows[1].push_back({"Enter", "ENTER", 2});
        rows[2].push_back({"", "SHIFT", 2});
        for (const char* p = L[2]; *p; p++) add(2, std::string(1, *p), 1);
        add(2, ",", 1); add(2, ".", 1); add(2, "?", 1);
        rows[3].push_back({"?123", "SYM", 2});
    } else {
        for (const char* p = "1234567890"; *p; p++) add(0, std::string(1, *p), 1);
        rows[0].push_back({"", "BS", 2});
        for (const char* p = "@#$%&*-+=/"; *p; p++) add(1, std::string(1, *p), 1);
        rows[1].push_back({"Enter", "ENTER", 2});
        for (const char* p = "()[]{}<>:;_\\"; *p; p++) add(2, std::string(1, *p), 1);
        rows[3].push_back({"ABC", "SYM", 2});
    }
    rows[3].push_back({"Space", "SPACE", 6});
    rows[3].push_back({"", "LEFT", 1}); rows[3].push_back({"", "RIGHT", 1}); rows[3].push_back({"", "HIDE", 2});
    return rows;
}
struct OKeyR { Rect r; std::string code, label; };
static std::vector<OKeyR> oskRects() {
    std::vector<OKeyR> out; auto rows = oskLayout();
    int top = H - oskH() + 10; float unit = (W - 16) / 12.0f;
    for (int r = 0; r < 4; r++) {
        float x = 8; int y = top + r * (OSK_KEY_H + OSK_GAP);
        for (auto& k : rows[r]) {
            Rect rc{(int)(x + OSK_GAP / 2), y, (int)(unit * k.w - OSK_GAP), OSK_KEY_H};
            out.push_back({rc, k.code, k.label}); x += unit * k.w;
        }
    }
    return out;
}
static void oskUpdate() {
    float tgt = (oskWant && gFocus) ? 1.f : 0.f;
    oskAnim += (tgt - oskAnim) * 0.35f; if (fabsf(tgt - oskAnim) < 0.02f) oskAnim = tgt;
    if (oskAnim <= 0.f) { oskCapture = false; return; }
    Rect panel{0, H - oskH(), W, oskH()};
    if (in.pressed && hit(panel, in.x, in.y)) {
        oskCapture = true;
        for (auto& k : oskRects()) if (hit(k.r, in.x, in.y)) {
            oskPressed = k.code; oskRepT = SDL_GetTicks() + 450;
            const std::string& c = k.code;
            auto emitKey = [&](int t, const std::string& s) { keyq.push_back({t, s}); };
            if (c == "BS") emitKey(1, ""); else if (c == "ENTER") emitKey(2, "");
            else if (c == "LEFT") emitKey(3, ""); else if (c == "RIGHT") emitKey(4, "");
            else if (c == "SPACE") emitKey(0, " ");
            else if (c == "HIDE") oskWant = false;
            else if (c == "SYM") oskSym = !oskSym;
            else if (c == "SHIFT") {
                Uint32 t = SDL_GetTicks();
                if (oskCaps) { oskCaps = false; oskShift = false; }
                else if (oskShift && t - oskShiftT < 450) { oskCaps = true; }
                else oskShift = !oskShift;
                oskShiftT = t;
            } else {
                emitKey(0, c); if (oskShift && !oskCaps) oskShift = false;
            }
            break;
        }
    }
    if (oskCapture) {
        in.blocked = true;
        if (in.down && (oskPressed == "BS" || oskPressed == "LEFT" || oskPressed == "RIGHT") && SDL_GetTicks() > oskRepT) {
            oskRepT = SDL_GetTicks() + 60;
            keyq.push_back({oskPressed == "BS" ? 1 : (oskPressed == "LEFT" ? 3 : 4), ""});
        }
        if (!in.down) { oskCapture = false; oskPressed.clear(); }
    }
}
static void oskDraw(Col accent) {
    if (oskAnim <= 0.f) return;
    Rect panel{0, H - oskH(), W, OSK_H};
    fillRect(panel, C(30, 30, 32));
    hline(0, panel.y, W, accent);
    for (auto& k : oskRects()) {
        bool special = k.code.size() > 1 && k.code != "SPACE" ? true : false;
        bool on = oskPressed == k.code && oskCapture && !(k.code.size() == 1 && false);
        bool isKey = k.label.size() > 0 || k.code.size() > 1;
        (void)isKey;
        Col bg = special ? C(58, 58, 62) : C(78, 78, 82);
        if (k.code == "SPACE") bg = C(78, 78, 82);
        if (k.code == "SHIFT" && (oskShift || oskCaps)) bg = accent;
        if (k.code == "ENTER") bg = accent;
        if (on) bg = C(235, 235, 235);
        // single-char keys: pressed state keyed by char only when capture is on that char
        fillRect(k.r, bg);
        Col fg = on ? C(20, 20, 20) : C(245, 245, 245);
        int cx = k.r.x + k.r.w / 2, cy = k.r.y + k.r.h / 2;
        if (k.code == "BS") {
            poly({{cx - 18.f, (float)cy}, {cx - 5.f, cy - 11.f}, {cx + 18.f, cy - 11.f}, {cx + 18.f, cy + 11.f}, {cx - 5.f, cy + 11.f}}, fg);
            lineP(cx, cy - 5, cx + 10, cy + 5, bg, 2.5f); lineP(cx + 10, cy - 5, cx, cy + 5, bg, 2.5f);
        } else if (k.code == "SHIFT") {
            poly({{(float)cx, cy - 13.f}, {cx + 13.f, (float)cy + 1}, {cx + 6.f, (float)cy + 1}, {cx + 6.f, cy + 12.f}, {cx - 6.f, cy + 12.f}, {cx - 6.f, (float)cy + 1}, {cx - 13.f, (float)cy + 1}}, fg);
        } else if (k.code == "LEFT") drawChevronLeft(cx, cy, 10, fg);
        else if (k.code == "RIGHT") { lineP(cx - 5, cy - 10, cx + 5, cy, fg, 3); lineP(cx + 5, cy, cx - 5, cy + 10, fg, 3); }
        else if (k.code == "HIDE") drawChevronDown(cx, cy, 12, fg);
        else if (k.code == "SPACE") {}
        else text(k.label, cx, cy - textH(k.label.size() > 1 ? 20 : 26) / 2, k.label.size() > 1 ? 20 : 26, fg, REG, 1);
    }
}
