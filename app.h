#pragma once
// Shared Office chrome: title bar, ribbon, status bar, Backstage (File) view and About/credits.
#include "ui.h"
#include "osk.h"

static const int TITLE_H = 36, TABS_H = 32, RIB_H = 96, STATUS_H = 26;
static const int CONTENT_TOP = TITLE_H + TABS_H + RIB_H;

static std::vector<std::string> wrapText(const std::string& s, int maxw, int size, int weight = REG) {
    std::vector<std::string> out; size_t i = 0;
    while (i <= s.size()) {
        size_t e = s.find('\n', i); if (e == std::string::npos) e = s.size();
        std::string para = s.substr(i, e - i), line, word;
        size_t j = 0;
        while (j <= para.size()) {
            size_t sp = para.find(' ', j); if (sp == std::string::npos) sp = para.size();
            word = para.substr(j, sp - j);
            std::string t = line.empty() ? word : line + " " + word;
            if (textW(t, size, weight) > maxw && !line.empty()) { out.push_back(line); line = word; } else line = t;
            j = sp + 1;
        }
        out.push_back(line); i = e + 1;
    }
    return out;
}
static std::string uniqueName(const std::string& base, const std::string& ext) {
    for (int i = 1; i < 999; i++) {
        std::string n = base + std::to_string(i) + "." + ext; FILE* f = fopen((DOCS() + n).c_str(), "rb");
        if (!f) return n; fclose(f);
    }
    return base + "x." + ext;
}

// ---------- ribbon widgets
static bool ribBtn(int x, int y, int w, const char* glyph, const char* label, Col ac, bool active = false, int gsize = 30) {
    Rect r{x, y, w, 70};
    if (active) { fillRect(r, alpha(ac, 38)); frameRect(r, alpha(ac, 160), 1); }
    else if (pressIn(r)) fillRect(r, C(205, 205, 205));
    Col gc = active ? ac : C(70, 70, 70);
    text(glyph, x + w / 2, y + 6 + (30 - gsize) / 2, gsize, gc, REG, 1);
    text(label, x + w / 2, y + 44, 15, C(70, 70, 70), REG, 1);
    return tapIn(r);
}
static bool ribBtnG(int x, int y, int w, const char* label, Col ac, bool active, const std::function<void(int, int, Col)>& glyph) {
    Rect r{x, y, w, 70};
    if (active) { fillRect(r, alpha(ac, 38)); frameRect(r, alpha(ac, 160), 1); }
    else if (pressIn(r)) fillRect(r, C(205, 205, 205));
    glyph(x + w / 2, y + 22, active ? ac : C(70, 70, 70));
    text(label, x + w / 2, y + 44, 15, C(70, 70, 70), REG, 1);
    return tapIn(r);
}
static void alignIcon(int cx, int cy, int mode, Col c) {
    int ws[4] = {26, 18, 26, 14};
    for (int i = 0; i < 4; i++) {
        int w = ws[i], x = mode == 0 ? cx - 13 : (mode == 1 ? cx - w / 2 : cx + 13 - w);
        fillRect({x, cy - 10 + i * 7, w, 3}, c);
    }
}
static void ribGroupEnd(int x0, int x1, const char* label, Rect area) {
    text(label, (x0 + x1) / 2, area.y + RIB_H - 22, 13, C(120, 120, 120), REG, 1);
    vline(x1 + 4, area.y + 8, RIB_H - 16, C(215, 215, 215));
}

// ---------- the app base class
struct App {
    int id = 0; std::string docName = "Untitled", path; bool dirty = false;
    int tab = 0; bool backstage = false; int bsPage = 0; Field saveAs; Scroll bsScroll;
    Uint32 lastSave = 0;
    virtual ~App() {}
    virtual const char* ext() = 0;
    virtual std::vector<std::string> openExts() = 0;
    virtual std::vector<const char*> tabNames() = 0;
    virtual void ribbon(int tab, Rect area) = 0;
    virtual void content(Rect area) = 0;
    virtual std::string status() { return "Ready"; }
    virtual void statusExtra(int x1, int y) {}   // right side of the status bar (x1 = right edge)
    virtual std::string serialize() = 0;
    virtual bool deserialize(const std::string& s) = 0;
    virtual void reset() = 0;
    virtual bool undo() { return false; }
    virtual bool redo() { return false; }
    virtual bool wantsFlatBg() { return false; }
    virtual bool fullscreen() { return false; }
    virtual bool canSave() { return true; }
    virtual const char* baseName() { return "Document"; }

    void newDoc() { reset(); path.clear(); docName = stemOf(uniqueName(baseName(), ext())); dirty = false; tab = 0; backstage = false; clearFocus(); }
    bool saveTo(const std::string& p) {
        if (!canSave()) { toast("Nothing to save"); return false; }
        mkdirs(DOCS());
        if (!writeFile(p, serialize())) { toast("Could not save - check the SD card"); return false; }
        path = p; size_t s = p.rfind('/'); docName = stemOf(p.substr(s == std::string::npos ? 0 : s + 1));
        dirty = false; lastSave = SDL_GetTicks(); return true;
    }
    bool save() {
        if (path.empty()) {
            std::string p = DOCS() + (docName.empty() ? std::string("Document") : docName) + "." + ext();
            FILE* f = fopen(p.c_str(), "rb"); if (f) { fclose(f); p = DOCS() + uniqueName(baseName(), ext()); }
            path = p;
        }
        bool ok = saveTo(path); if (ok) toast("Saved " + docName); return ok;
    }
    bool openFile(const std::string& p) {
        std::string d; if (!readFile(p, d)) { toast("Could not open file"); return false; }
        reset(); if (!deserialize(d)) { toast("Unsupported file"); return false; }
        path = (extOf(p) == std::string(ext())) ? p : std::string(); size_t s = p.rfind('/'); docName = stemOf(p.substr(s == std::string::npos ? 0 : s + 1));
        dirty = false; tab = 0; backstage = false; clearFocus(); return true;
    }
    void autosave() {
        if (dirty && !path.empty() && SDL_GetTicks() - lastSave > 20000) { saveTo(path); }
        if (dirty && path.empty() && SDL_GetTicks() - lastSave > 30000) { save(); }
    }
};

// ---------- About / credits panel (shown in Backstage Info and from the Office home page)
static void drawAbout(Rect r, Col ac, Scroll& sc) {
    int cw = r.w - 80;
    int ch = 760;
    scrollUpdate(sc, r, r.w, ch);
    clip(r);
    int y = r.y + 10 - (int)sc.y, x = r.x + 40;
    drawOfficeLogo((float)x, (float)y, 84, ac, C(255, 255, 255));
    text("Office", x + (int)officeLogoW(84) + 22, y + 6, 58, ac, LIGHT);
    text("Version 1.0.0  -  Nintendo Switch edition", x + (int)officeLogoW(84) + 24, y + 70, 18, C(110, 110, 110));
    y += 112;
    text("Credits", x, y, 28, ac, LIGHT); y += 42;
    const char* paras[] = {
        "Credits to Microsoft and to all of its developers and designers. Office, Word, Excel, PowerPoint, Access and OneNote - their products, icons, logos and design language - were created by them. This app is only a tribute to their work.",
        "Typeface: Selawik, by Microsoft (SIL Open Font License 1.1).",
        "Built with SDL2, SDL2_ttf, libnx and devkitPro.",
        "Disclaimer: this is an unofficial, fan-made recreation for Nintendo Switch homebrew. It is not affiliated with, endorsed by or sponsored by Microsoft. Microsoft, Office, Word, Excel, PowerPoint, Access and OneNote are trademarks of the Microsoft group of companies."};
    for (auto p : paras) {
        for (auto& ln : wrapText(p, cw, 19)) { text(ln, x, y, 19, C(60, 60, 60)); y += 27; }
        y += 12;
    }
    y += 6; drawMsBrand(x, y, 30, C(90, 90, 90));
    unclip();
}

// ---------- title / tabs / status
static void arcArrow(int cx, int cy, bool redo, Col c) {
    float d = redo ? 1.f : -1.f;
    float pts[6][2] = {{9, 4}, {9, -2}, {5, -6}, {-1, -6}, {-6, -2}, {-6, 3}};
    for (int i = 0; i < 5; i++) lineP(cx - d * pts[i][0], cy + pts[i][1], cx - d * pts[i + 1][0], cy + pts[i + 1][1], c, 2.6f);
    float ex = cx + d * 6, ey = cy + 3;
    poly({{ex - 6.f, ey - 1.f}, {ex + 6.f, ey - 1.f}, {ex, ey + 7.f}}, c);
}
static void drawTitleBar(App& a, Col ac, bool* close) {
    fillRect({0, 0, W, TITLE_H}, ac);
    drawAppIcon(a.id, 8, 4, 28, true, ac);
    int x = 46; Col wh = C(255, 255, 255);
    Rect sv{x, 2, 32, 32}, un{x + 34, 2, 32, 32}, re{x + 68, 2, 32, 32};
    Rect bs[3] = {sv, un, re};
    for (auto& r : bs) if (pressIn(r)) fillRect(r, C(255, 255, 255, 50));
    frameRect({sv.x + 7, sv.y + 7, 18, 18}, wh, 2); fillRect({sv.x + 11, sv.y + 7, 10, 7}, wh); fillRect({sv.x + 12, sv.y + 18, 8, 5}, wh);
    arcArrow(un.x + 16, un.y + 18, false, wh); arcArrow(re.x + 16, re.y + 18, true, wh);
    if (tapIn(sv)) a.save();
    if (tapIn(un)) a.undo();
    if (tapIn(re)) a.redo();
    std::string t = a.docName + (a.dirty ? " *" : "") + "  -  " + APPS[a.id].name;
    text(t, W / 2, 7, 17, wh, REG, 1);
    Rect cl{W - 64, 0, 64, TITLE_H};
    if (pressIn(cl)) fillRect(cl, C(232, 17, 35));
    lineP(cl.x + 24, 11, cl.x + 40, 25, wh, 2); lineP(cl.x + 40, 11, cl.x + 24, 25, wh, 2);
    if (tapIn(cl) && close) *close = true;
}
static const Col RIB_BG = {243, 243, 243, 255};
static void drawTabs(App& a, Col ac) {
    int y = TITLE_H;
    fillRect({0, y, W, TABS_H}, ac);
    fillRect({0, y + TABS_H, W, RIB_H}, RIB_BG);
    hline(0, y + TABS_H + RIB_H - 1, W, C(213, 213, 213));
    Rect file{0, y, 78, TABS_H};
    if (pressIn(file)) fillRect(file, mixc(ac, C(0, 0, 0), 0.25f));
    text("File", 39, y + 5, 17, C(255, 255, 255), REG, 1);
    if (tapIn(file)) { a.backstage = true; a.bsPage = 0; clearFocus(); }
    int x = 84; auto names = a.tabNames();
    for (int i = 0; i < (int)names.size(); i++) {
        int w = textW(names[i], 17) + 30; Rect r{x, y, w, TABS_H};
        bool act = a.tab == i;
        if (act) { fillRect({x, y + 2, w, TABS_H - 2}, RIB_BG); vline(x, y + 2, TABS_H - 2, C(213, 213, 213)); vline(x + w - 1, y + 2, TABS_H - 2, C(213, 213, 213)); hline(x, y + 2, w, C(213, 213, 213)); }
        else if (pressIn(r)) fillRect(r, C(255, 255, 255, 30));
        text(names[i], x + w / 2, y + 5, 17, act ? ac : C(255, 255, 255), REG, 1);
        if (tapIn(r)) a.tab = i;
        x += w;
    }
    a.ribbon(a.tab, Rect{0, TITLE_H + TABS_H, W, RIB_H});
}
static void drawStatusBar(App& a, Col ac) {
    int y = H - oskH() - STATUS_H;
    fillRect({0, y, W, STATUS_H}, ac);
    text(a.status(), 14, y + 3, 15, C(255, 255, 255));
    if (SDL_GetTicks() - toastT < 1800 && !toastMsg.empty()) text(toastMsg, W / 2, y + 3, 15, C(255, 255, 255), BOLD, 1);
    a.statusExtra(W - 14, y);
}
// zoom slider for the status bar. Returns new zoom (percent 50..160) if the user changed it.
static bool statusZoom(int x1, int y, float& zoom, float lo, float hi) {
    Rect minus{x1 - 250, y, 24, STATUS_H}, plus{x1 - 96, y, 24, STATUS_H}, track{x1 - 222, y, 124, STATUS_H};
    Col wh = C(255, 255, 255); bool ch = false;
    text(std::to_string((int)(zoom * 100 + 0.5f)) + "%", x1, y + 3, 15, wh, REG, 2);
    hline(minus.x + 6, y + 13, 12, wh); hline(plus.x + 6, y + 13, 12, wh); vline(plus.x + 12, y + 7, 12, wh);
    hline(track.x, y + 13, track.w, C(255, 255, 255, 170));
    float t = (zoom - lo) / (hi - lo); fillRect({track.x + (int)(t * (track.w - 8)), y + 6, 8, 14}, wh);
    if (tapIn(minus)) { zoom = std::max(lo, zoom - 0.1f); ch = true; }
    if (tapIn(plus)) { zoom = std::min(hi, zoom + 0.1f); ch = true; }
    if ((in.down || in.released) && hit(track, in.sx, in.sy) && !in.blocked) { float u = (in.x - track.x - 4) / (float)(track.w - 8); u = std::max(0.f, std::min(1.f, u)); zoom = lo + u * (hi - lo); ch = true; }
    return ch;
}

// ---------- Backstage (File)
static void drawBackstage(App& a, Col ac, bool* goHome) {
    fillRect({0, 0, W, H}, C(255, 255, 255));
    int pw = 280; fillRect({0, 0, pw, H}, ac);
    Rect back{22, 22, 52, 52};
    circle(48, 48, 25, C(255, 255, 255, pressIn(back) ? 90 : 40)); drawChevronLeft(46, 48, 11, C(255, 255, 255));
    if (tapIn(back) || in.btnB) { a.backstage = false; clearFocus(); return; }
    const char* items[] = {"Info", "New", "Open", "Save", "Save As", "Close"};
    for (int i = 0; i < 6; i++) {
        Rect r{0, 110 + i * 62, pw, 58};
        bool act = a.bsPage == i;
        if (act) fillRect(r, mixc(ac, C(0, 0, 0), 0.25f)); else if (pressIn(r)) fillRect(r, mixc(ac, C(255, 255, 255), 0.15f));
        text(items[i], 40, r.y + 12, 24, C(255, 255, 255), LIGHT);
        if (tapIn(r)) {
            if (i == 3) { a.save(); a.backstage = false; clearFocus(); return; }
            if (i == 5) { if (a.dirty) a.save(); if (goHome) *goHome = true; return; }
            a.bsPage = i; a.bsScroll = Scroll();
            if (i == 4) { a.saveAs.t = a.docName; a.saveAs.cur = (int)a.saveAs.t.size(); setFocus(&a.saveAs); } else clearFocus();
        }
    }
    drawMsBrand(30, H - 56, 22, C(255, 255, 255));
    Rect page{pw, 0, W - pw, H - oskH()};
    int x = pw + 50;
    if (a.bsPage == 0) {
        text("Info", x, 24, 44, ac, LIGHT);
        text(a.docName, x, 90, 26, C(40, 40, 40));
        text(a.path.empty() ? "Not saved yet - tap Save to store it on the SD card" : a.path, x, 126, 16, C(120, 120, 120));
        hline(x, 160, page.w - 100, C(220, 220, 220));
        drawAbout({pw + 10, 170, page.w - 20, H - 180}, ac, a.bsScroll);
    } else if (a.bsPage == 1) {
        text("New", x, 24, 44, ac, LIGHT);
        Rect card{x, 110, 260, 200};
        fillRect(card, C(245, 245, 245)); frameRect(card, pressIn(card) ? ac : C(200, 200, 200), 2);
        drawAppIcon(a.id, (float)card.x + 70, (float)card.y + 24, 120);
        text(std::string("Blank ") + APPS[a.id].name, card.x + 130, card.y + 160, 20, C(50, 50, 50), REG, 1);
        if (tapIn(card)) { if (a.dirty) a.save(); a.newDoc(); }
    } else if (a.bsPage == 2) {
        text("Open", x, 24, 44, ac, LIGHT);
        auto files = listDocs(a.openExts());
        Rect lr{x, 100, page.w - 100, page.h - 110};
        scrollUpdate(a.bsScroll, lr, lr.w, (int)files.size() * 64 + 10);
        clip(lr);
        if (files.empty()) text("No files yet. Create something and tap Save.", x, 110, 20, C(120, 120, 120));
        for (int i = 0; i < (int)files.size(); i++) {
            Rect r{lr.x, lr.y + i * 64 - (int)a.bsScroll.y, lr.w, 60};
            if (r.y + r.h < lr.y || r.y > lr.y + lr.h) continue;
            if (pressIn(r)) fillRect(r, C(235, 235, 235)); else hline(r.x, r.y + 61, r.w, C(232, 232, 232));
            drawAppIcon(a.id, (float)r.x + 6, (float)r.y + 10, 40);
            text(fitText(files[i].name, lr.w - 360, 22), r.x + 64, r.y + 6, 22, C(40, 40, 40));
            text(fmtTime(files[i].mtime), r.x + 64, r.y + 34, 14, C(130, 130, 130));
            if (tapIn(r)) { if (a.dirty) a.save(); a.openFile(files[i].path); }
        }
        unclip();
    } else if (a.bsPage == 4) {
        text("Save As", x, 24, 44, ac, LIGHT);
        text("File name", x, 110, 18, C(90, 90, 90));
        bool en = drawField({x, 140, 560, 48}, a.saveAs, "File name", ac, 24);
        text(std::string("Saved in ") + DOCS(), x, 200, 15, C(130, 130, 130));
        Rect sb{x, 232, 140, 48};
        fillRect(sb, pressIn(sb) ? mixc(ac, C(0, 0, 0), 0.25f) : ac); text("Save", sb.x + 70, sb.y + 9, 22, C(255, 255, 255), REG, 1);
        if (tapIn(sb) || en) {
            std::string n = a.saveAs.t; for (auto& c : n) if (c == '/' || c == '\\' || c == ':') c = '_';
            if (n.empty()) n = "Document";
            if (extOf(n) != a.ext()) n += std::string(".") + a.ext();
            if (a.saveTo(DOCS() + n)) toast("Saved " + a.docName);
            a.backstage = false; clearFocus();
        }
    }
}
