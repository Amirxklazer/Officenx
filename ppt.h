#pragma once
#include "editor.h"

struct Theme { Col bg, title, text, accent; const char* name; };
static const Theme THEMES[6] = {
    {{255, 255, 255, 255}, {64, 64, 64, 255}, {64, 64, 64, 255}, {210, 71, 38, 255}, "Office"},
    {{255, 255, 255, 255}, {43, 87, 154, 255}, {64, 64, 64, 255}, {43, 87, 154, 255}, "Blue"},
    {{255, 255, 255, 255}, {33, 115, 70, 255}, {64, 64, 64, 255}, {33, 115, 70, 255}, "Green"},
    {{255, 255, 255, 255}, {128, 57, 123, 255}, {64, 64, 64, 255}, {128, 57, 123, 255}, "Purple"},
    {{38, 38, 38, 255}, {255, 255, 255, 255}, {230, 230, 230, 255}, {255, 140, 0, 255}, "Dark"},
    {{255, 247, 238, 255}, {164, 55, 58, 255}, {70, 50, 50, 255}, {164, 55, 58, 255}, "Warm"},
};

struct Slide {
    DocEditor title, body; int layout = 1, theme = 0;
    void init(int lay, int th) {
        layout = lay; theme = th; title.clear(); body.clear();
        if (lay == 0) { title.doc.p[0].align = 1; body.doc.p[0].align = 1; } else body.doc.p[0].bullet = true;
        title.ver++; body.ver++;
    }
    std::string titleText() { std::string s; for (auto& p : title.doc.p) { if (!s.empty()) s += " "; s += p.t; } return s; }
};

struct PptApp : App {
    std::vector<Slide> slides; int cur = 0; Scroll thumbSc; bool show = false;
    PptApp() { id = A_PPT; reset(); docName = "Presentation1"; }
    const char* ext() override { return "sld"; }
    const char* baseName() override { return "Presentation"; }
    std::vector<std::string> openExts() override { return {"sld"}; }
    std::vector<const char*> tabNames() override { return {"Home", "Design", "Slide Show"}; }
    bool fullscreen() override { return show; }
    void reset() override { slides.clear(); slides.resize(1); slides[0].init(0, 0); cur = 0; show = false; clearFocus(); }
    std::string status() override { return "Slide " + std::to_string(cur + 1) + " of " + std::to_string(slides.size()); }
    DocEditor* focused() { if (cur >= (int)slides.size()) return nullptr; Slide& s = slides[cur]; if (isFocus(&s.title)) return &s.title; if (isFocus(&s.body)) return &s.body; return nullptr; }
    bool undo() override { DocEditor* e = focused(); return e ? e->undo() : false; }
    bool redo() override { DocEditor* e = focused(); return e ? e->redo() : false; }

    // ---- file
    std::string serialize() override {
        std::string o = "PPT1\n";
        for (auto& s : slides) {
            o += "S|" + std::to_string(s.layout) + "|" + std::to_string(s.theme) + "\n";
            std::string t = s.title.serialize(), b = s.body.serialize(); size_t i = 0;
            while (i < t.size()) { size_t e = t.find('\n', i); o += "T:" + t.substr(i, e - i) + "\n"; i = e + 1; }
            i = 0; while (i < b.size()) { size_t e = b.find('\n', i); o += "B:" + b.substr(i, e - i) + "\n"; i = e + 1; }
        }
        return o;
    }
    bool deserialize(const std::string& d) override {
        if (d.compare(0, 5, "PPT1\n") != 0) return false;
        slides.clear(); size_t i = 5; std::string tt, bb;
        auto flush = [&]() { if (slides.empty()) return; if (!tt.empty()) slides.back().title.parse(tt); if (!bb.empty()) slides.back().body.parse(bb); tt.clear(); bb.clear(); };
        while (i < d.size()) {
            size_t e = d.find('\n', i); if (e == std::string::npos) e = d.size();
            std::string ln = d.substr(i, e - i); i = e + 1;
            if (ln.compare(0, 2, "S|") == 0) { flush(); Slide s; int l = 1, th = 0; sscanf(ln.c_str(), "S|%d|%d", &l, &th); s.init(l, th); slides.push_back(s); }
            else if (ln.compare(0, 2, "T:") == 0) tt += ln.substr(2) + "\n";
            else if (ln.compare(0, 2, "B:") == 0) bb += ln.substr(2) + "\n";
        }
        flush(); if (slides.empty()) { slides.resize(1); slides[0].init(0, 0); }
        cur = 0; return true;
    }

    // ---- layout geometry (slide space 960 x 540)
    Rect titleBox(const Slide& s) { return s.layout == 0 ? Rect{80, 150, 800, 150} : Rect{60, 36, 840, 110}; }
    Rect bodyBox(const Slide& s) { return s.layout == 0 ? Rect{120, 320, 720, 120} : Rect{60, 170, 840, 330}; }
    int titleSize(const Slide& s) { return s.layout == 0 ? 66 : 46; }
    int bodySize(const Slide& s) { return s.layout == 0 ? 32 : 30; }
    Rect scaled(Rect r, Rect sl, float k) { return Rect{sl.x + (int)(r.x * k), sl.y + (int)(r.y * k), (int)(r.w * k), (int)(r.h * k)}; }

    void drawBackground(Slide& s, Rect sl, float k) {
        const Theme& th = THEMES[s.theme]; fillRect(sl, th.bg);
        if (s.layout == 0) { fillRect({sl.x, sl.y + (int)(306 * k), sl.w, std::max(2, (int)(8 * k))}, th.accent); fillRect({sl.x, sl.y, (int)(24 * k), sl.h}, th.accent); }
        else { fillRect({sl.x, sl.y, sl.w, std::max(2, (int)(12 * k))}, th.accent); }
    }
    void drawSlideEditable(Slide& s, Rect sl, float k, bool editable) {
        const Theme& th = THEMES[s.theme]; drawBackground(s, sl, k);
        EdOpts ot; ot.paper = false; ot.useStyles = false; ot.scroll = false; ot.readonly = !editable;
        ot.baseSize = std::max(8, (int)(titleSize(s) * k)); ot.color = th.title; ot.pad = std::max(2, (int)(8 * k));
        EdOpts ob = ot; ob.baseSize = std::max(8, (int)(bodySize(s) * k)); ob.color = th.text;
        Rect tb = scaled(titleBox(s), sl, k), bb = scaled(bodyBox(s), sl, k);
        // vertically centre single-title text a bit
        s.title.draw(tb, ot); s.body.draw(bb, ob);
        bool te = s.title.doc.p.size() == 1 && s.title.doc.p[0].t.empty(), be = s.body.doc.p.size() == 1 && s.body.doc.p[0].t.empty();
        if (editable) {
            int fs = std::max(10, (int)(24 * k));
            if (te && !isFocus(&s.title)) text(s.layout == 0 ? "Click to add title" : "Click to add title", tb.x + (s.layout == 0 ? tb.w / 2 : ot.pad), tb.y + ot.pad, fs, alpha(th.title, 120), REG, s.layout == 0 ? 1 : 0);
            if (be && !isFocus(&s.body)) text(s.layout == 0 ? "Click to add subtitle" : "Click to add text", bb.x + (s.layout == 0 ? bb.w / 2 : ot.pad), bb.y + ot.pad, fs, alpha(th.text, 120), REG, s.layout == 0 ? 1 : 0);
            for (int q = 0; q < 2; q++) {
                Rect b = q == 0 ? tb : bb; bool f = q == 0 ? isFocus(&s.title) : isFocus(&s.body);
                frameRect(b, f ? C(120, 120, 120) : C(190, 190, 190), 1);
                if (f) { int xs[3] = {b.x, b.x + b.w / 2, b.x + b.w}, ys[3] = {b.y, b.y + b.h / 2, b.y + b.h};
                    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) if (i != 1 || j != 1) { circle((float)xs[i], (float)ys[j], 5, C(120, 120, 120)); circle((float)xs[i], (float)ys[j], 3.5f, C(255, 255, 255)); } }
            }
        }
    }
    void drawThumb(Slide& s, Rect r) {
        const Theme& th = THEMES[s.theme]; float k = r.w / 960.f; drawBackground(s, r, k);
        std::string t = s.titleText(); int ts = std::max(8, (int)(titleSize(s) * k * 0.9f));
        clip(r);
        if (s.layout == 0) text(fitText(t, r.w - 20, ts), r.x + r.w / 2, r.y + (int)(150 * k), ts, th.title, REG, 1);
        else {
            text(fitText(t, r.w - 16, ts), r.x + (int)(60 * k), r.y + (int)(40 * k), ts, th.title);
            int y = r.y + (int)(180 * k), bs = std::max(7, (int)(30 * k));
            for (auto& p : s.body.doc.p) { if (y > r.y + r.h - 4) break; text(fitText(p.t, r.w - 30, bs), r.x + (int)(60 * k), y, bs, th.text); y += (int)(bs * 1.5f); }
        }
        unclip();
    }
    void newSlide(int lay) {
        Slide s; s.init(lay, slides[cur].theme); slides.insert(slides.begin() + cur + 1, s); cur++; dirty = true; clearFocus();
    }

    // ---- ribbon
    void ribbon(int t, Rect a) override {
        Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x; Slide& s = slides[cur]; DocEditor* fe = focused();
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 86, "+", "New slide", ac, false, 40)) { newSlide(1); return; } x += 88;
            if (ribBtn(x, y, 86, "Aa", "Title slide", ac, false, 30)) { newSlide(0); return; } x += 88;
            if (ribBtn(x, y, 86, "2x", "Duplicate", ac, false, 28)) { Slide c = s; slides.insert(slides.begin() + cur + 1, c); cur++; dirty = true; clearFocus(); return; } x += 88;
            if (ribBtn(x, y, 86, "X", "Delete", ac, false, 30)) { if (slides.size() > 1) { slides.erase(slides.begin() + cur); if (cur >= (int)slides.size()) cur = (int)slides.size() - 1; dirty = true; clearFocus(); return; } } x += 88;
            grp("Slides");
            if (ribBtn(x, y, 62, "B", "Bold", ac, fe && fe->cur().b)) { if (fe) { fe->toggleB(); dirty = true; } } x += 64;
            if (ribBtn(x, y, 62, "I", "Italic", ac, fe && fe->cur().i)) { if (fe) { fe->toggleI(); dirty = true; } } x += 64;
            if (ribBtn(x, y, 62, "A-", "Smaller", ac, false, 26)) { if (fe) { fe->bumpSize(-1); dirty = true; } } x += 64;
            if (ribBtn(x, y, 62, "A+", "Larger", ac, false, 26)) { if (fe) { fe->bumpSize(1); dirty = true; } } x += 64;
            grp("Font");
            if (ribBtn(x, y, 66, "*", "Bullets", ac, fe && fe->cur().bullet, 36)) { if (fe) { fe->toggleBullet(); dirty = true; } } x += 68;
            if (ribBtnG(x, y, 62, "Left", ac, fe && fe->cur().align == 0, [](int cx, int cy, Col c) { alignIcon(cx, cy, 0, c); })) { if (fe) { fe->setAlign(0); dirty = true; } } x += 64;
            if (ribBtnG(x, y, 62, "Center", ac, fe && fe->cur().align == 1, [](int cx, int cy, Col c) { alignIcon(cx, cy, 1, c); })) { if (fe) { fe->setAlign(1); dirty = true; } } x += 64;
            if (ribBtnG(x, y, 62, "Right", ac, fe && fe->cur().align == 2, [](int cx, int cy, Col c) { alignIcon(cx, cy, 2, c); })) { if (fe) { fe->setAlign(2); dirty = true; } } x += 64;
            grp("Paragraph");
        } else if (t == 1) {
            for (int i = 0; i < 6; i++) {
                Rect r{x, y, 96, 70}; const Theme& th = THEMES[i];
                fillRect(r, C(255, 255, 255)); frameRect(r, s.theme == i ? ac : C(200, 200, 200), s.theme == i ? 2 : 1);
                fillRect({r.x + 12, r.y + 8, 72, 34}, th.bg); frameRect({r.x + 12, r.y + 8, 72, 34}, C(190, 190, 190)); fillRect({r.x + 12, r.y + 8, 72, 5}, th.accent);
                fillRect({r.x + 20, r.y + 20, 38, 5}, th.title);
                text(th.name, r.x + 48, r.y + 46, 14, C(80, 80, 80), REG, 1);
                if (tapIn(r)) { for (auto& sl : slides) sl.theme = i; dirty = true; }
                x += 100;
            }
            grp("Themes");
        } else {
            if (ribBtn(x, y, 110, ">", "From beginning", ac, false, 36)) { show = true; cur = 0; clearFocus(); } x += 112;
            if (ribBtn(x, y, 110, ">|", "From current", ac, false, 30)) { show = true; clearFocus(); } x += 112;
            grp("Start Slide Show");
        }
    }

    void content(Rect a) override {
        if (show) {
            Slide& s = slides[cur]; fillRect({0, 0, W, H}, C(0, 0, 0));
            Rect sl{0, 0, W, H}; float k = W / 960.f; drawSlideEditable(s, sl, k, false);
            if (in.btnB || in.dpadX < 0 || false) { if (in.btnB) show = false; else if (cur > 0) cur--; }
            if (in.dpadX > 0 && cur + 1 < (int)slides.size()) cur++;
            if (tapIn(sl)) { if (in.sx < W / 4) { if (cur > 0) cur--; } else if (cur + 1 < (int)slides.size()) cur++; else show = false; }
            text(std::to_string(cur + 1) + " / " + std::to_string(slides.size()), W - 16, H - 28, 16, C(160, 160, 160), REG, 2);
            return;
        }
        int pw = 190, vh = H - oskH() - STATUS_H - a.y;
        Rect panel{a.x, a.y, pw, vh}, main{a.x + pw, a.y, a.w - pw, vh};
        fillRect(main, C(222, 222, 222)); fillRect(panel, C(240, 240, 240)); vline(pw - 1, a.y, vh, C(205, 205, 205));
        // thumbnails
        int th = 100; scrollUpdate(thumbSc, panel, pw, (int)slides.size() * th + 20);
        clip(panel);
        for (int i = 0; i < (int)slides.size(); i++) {
            Rect r{a.x + 34, a.y + 10 + i * th - (int)thumbSc.y, 140, 79};
            if (r.y + r.h < a.y || r.y > a.y + vh) continue;
            text(std::to_string(i + 1), a.x + 6, r.y + 2, 15, C(90, 90, 90));
            drawThumb(slides[i], r); frameRect(r, i == cur ? HX(APPS[id].ui) : C(190, 190, 190), i == cur ? 3 : 1);
            if (tapIn(r)) { cur = i; clearFocus(); }
        }
        unclip();
        // main slide
        Slide& s = slides[cur];
        float k = std::min((main.w - 40) / 960.f, (main.h - 24) / 540.f); if (k < 0.2f) k = 0.2f;
        Rect sl{main.x + (main.w - (int)(960 * k)) / 2, main.y + (main.h - (int)(540 * k)) / 2, (int)(960 * k), (int)(540 * k)};
        if (oskH() > 0) sl.y = main.y + 8;
        fillRect({sl.x + 3, sl.y + 3, sl.w, sl.h}, C(180, 180, 180));
        drawSlideEditable(s, sl, k, true);
        if (s.title.changed || s.body.changed) { dirty = true; s.title.changed = s.body.changed = false; }
    }
};
