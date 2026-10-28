#pragma once
#include "editor.h"

struct ONPage { Field title; std::string created; DocEditor body; };
struct ONSection { std::string name; int color = 0; std::vector<ONPage> pages; };
static const unsigned ON_COLORS[7] = {0x4F81D4, 0x4FA84F, 0xE98B2A, 0x8E5BC0, 0xD64F4F, 0x2BA3A3, 0xD1B52E};

struct OneNoteApp : App {
    std::vector<ONSection> secs; int cs = 0, cp = 0; Scroll pageSc; bool renaming = false; Field renameF;
    OneNoteApp() { id = A_ONE; reset(); docName = "Notebook1"; }
    const char* ext() override { return "onx"; }
    const char* baseName() override { return "Notebook"; }
    std::vector<std::string> openExts() override { return {"onx"}; }
    std::vector<const char*> tabNames() override { return {"Home", "Insert", "View"}; }
    static std::string now() { char b[64]; time_t n = time(0); strftime(b, sizeof b, "%A, %d %B %Y   %H:%M", localtime(&n)); return b; }
    ONPage newPage(const std::string& t = "") { ONPage p; p.title.t = t; p.created = now(); p.body.clear(); return p; }
    void reset() override {
        secs.clear(); ONSection s; s.name = "Quick Notes"; s.color = 0; s.pages.push_back(newPage()); secs.push_back(s); cs = cp = 0; renaming = false; clearFocus();
    }
    ONPage& page() { return secs[cs].pages[cp]; }
    bool undo() override { return isFocus(&page().body) ? page().body.undo() : false; }
    bool redo() override { return isFocus(&page().body) ? page().body.redo() : false; }
    std::string serialize() override {
        std::string o = "ONX1\n";
        for (auto& s : secs) {
            o += "SEC|" + std::to_string(s.color) + "|" + s.name + "\n";
            for (auto& p : s.pages) {
                o += "PAGE|" + p.created + "|" + p.title.t + "\n";
                std::string b = p.body.serialize(); size_t i = 0;
                while (i < b.size()) { size_t e = b.find('\n', i); o += "L:" + b.substr(i, e - i) + "\n"; i = e + 1; }
            }
        }
        return o;
    }
    bool deserialize(const std::string& d) override {
        if (d.compare(0, 5, "ONX1\n") != 0) return false;
        secs.clear(); size_t i = 5; std::string body;
        auto flush = [&]() { if (!secs.empty() && !secs.back().pages.empty() && !body.empty()) secs.back().pages.back().body.parse(body); body.clear(); };
        while (i < d.size()) {
            size_t e = d.find('\n', i); if (e == std::string::npos) e = d.size();
            std::string ln = d.substr(i, e - i); i = e + 1;
            if (ln.compare(0, 4, "SEC|") == 0) { flush(); ONSection s; size_t b = ln.find('|', 4); s.color = atoi(ln.substr(4, b - 4).c_str()) % 7; s.name = ln.substr(b + 1); secs.push_back(s); }
            else if (ln.compare(0, 5, "PAGE|") == 0 && !secs.empty()) { flush(); size_t b = ln.find('|', 5); ONPage p = newPage(); p.created = ln.substr(5, b - 5); p.title.t = ln.substr(b + 1); secs.back().pages.push_back(p); }
            else if (ln.compare(0, 2, "L:") == 0) body += ln.substr(2) + "\n";
        }
        flush();
        if (secs.empty()) return false;
        for (auto& s : secs) if (s.pages.empty()) s.pages.push_back(newPage());
        cs = cp = 0; return true;
    }
    void addPage() { secs[cs].pages.insert(secs[cs].pages.begin() + cp + 1, newPage()); cp++; dirty = true; clearFocus(); }
    void ribbon(int t, Rect a) override {
        Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x; DocEditor& e = page().body;
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 62, "B", "Bold", ac, e.cur().b)) { e.toggleB(); dirty = true; } x += 64;
            if (ribBtn(x, y, 62, "I", "Italic", ac, e.cur().i)) { e.toggleI(); dirty = true; } x += 64;
            if (ribBtn(x, y, 62, "U", "Underline", ac, e.cur().u)) { e.toggleU(); dirty = true; } x += 64;
            if (ribBtn(x, y, 66, "*", "Bullets", ac, e.cur().bullet, 36)) { e.toggleBullet(); dirty = true; } x += 68;
            grp("Basic Text");
            if (ribBtn(x, y, 96, "H1", "Heading 1", ac, e.cur().style == 1, 28)) { e.setStyle(e.cur().style == 1 ? 0 : 1); dirty = true; } x += 98;
            if (ribBtn(x, y, 96, "H2", "Heading 2", ac, e.cur().style == 2, 28)) { e.setStyle(e.cur().style == 2 ? 0 : 2); dirty = true; } x += 98;
            grp("Styles");
            if (ribBtnG(x, y, 90, "To Do", ac, e.cur().todo != 0, [](int cx, int cy, Col c) { frameRect({cx - 11, cy - 11, 22, 22}, c, 2); lineP(cx - 6, cy, cx - 2, cy + 5, c, 3); lineP(cx - 2, cy + 5, cx + 7, cy - 6, c, 3); })) { e.toggleTodo(); dirty = true; } x += 92;
            grp("Tags");
            if (ribBtn(x, y, 96, "+", "New page", ac, false, 40)) { addPage(); return; } x += 98;
            if (ribBtn(x, y, 96, "X", "Delete page", ac, false, 30)) { auto& pg = secs[cs].pages; if (pg.size() > 1) { pg.erase(pg.begin() + cp); if (cp >= (int)pg.size()) cp = (int)pg.size() - 1; dirty = true; clearFocus(); return; } else toast("A section needs at least one page"); } x += 98;
            grp("Pages");
        } else if (t == 1) {
            if (ribBtn(x, y, 100, "+", "New section", ac, false, 40)) { ONSection s; s.name = "New Section"; s.color = (int)secs.size() % 7; s.pages.push_back(newPage()); secs.push_back(s); cs = (int)secs.size() - 1; cp = 0; dirty = true; clearFocus(); return; } x += 102;
            if (ribBtn(x, y, 100, "Ab", "Rename section", ac, false, 28)) { renaming = true; renameF.t = secs[cs].name; renameF.cur = (int)renameF.t.size(); setFocus(&renameF); } x += 102;
            if (ribBtn(x, y, 100, "X", "Delete section", ac, false, 30)) { if (secs.size() > 1) { secs.erase(secs.begin() + cs); cs = std::max(0, cs - 1); cp = 0; dirty = true; clearFocus(); return; } else toast("A notebook needs at least one section"); } x += 102;
            grp("Sections");
            if (ribBtn(x, y, 100, "Date", "Insert date", ac, false, 24)) { char b[64]; time_t n = time(0); strftime(b, sizeof b, "%d/%m/%Y", localtime(&n)); e.insertText(b); setFocus(&e, false); dirty = true; } x += 102;
            if (ribBtn(x, y, 100, "Time", "Insert time", ac, false, 24)) { char b[64]; time_t n = time(0); strftime(b, sizeof b, "%H:%M", localtime(&n)); e.insertText(b); setFocus(&e, false); dirty = true; } x += 102;
            grp("Insert");
        } else {
            text("Notebook", x + 4, a.y + 10, 17, C(60, 60, 60));
            text(std::to_string(secs.size()) + " sections", x + 4, a.y + 36, 17, C(110, 110, 110));
            int n = 0; for (auto& s : secs) n += (int)s.pages.size();
            text(std::to_string(n) + " pages", x + 4, a.y + 60, 17, C(110, 110, 110)); x += 160; grp("Statistics");
        }
    }
    void content(Rect a) override {
        int vh = H - oskH() - STATUS_H - a.y; Col ac = HX(APPS[id].ui);
        int lw = 190, rw = 210;
        Rect left{a.x, a.y, lw, vh}, right{W - rw, a.y, rw, vh}, mid{lw, a.y, W - lw - rw, vh};
        // left: notebooks
        fillRect(left, C(238, 238, 238)); vline(lw - 1, a.y, vh, C(205, 205, 205));
        text("Notebooks", left.x + 14, a.y + 10, 17, ac, BOLD);
        Rect nb{left.x, a.y + 44, lw - 1, 40}; fillRect(nb, alpha(ac, 45));
        fillRect({nb.x + 12, nb.y + 10, 8, 20}, ac); fillRect({nb.x + 22, nb.y + 10, 12, 20}, alpha(ac, 170));
        text(fitText(docName, lw - 64, 17), nb.x + 42, nb.y + 8, 17, C(40, 40, 40));
        // section tabs
        fillRect({mid.x, a.y, mid.w, 34}, C(250, 250, 250)); hline(mid.x, a.y + 33, mid.w, C(205, 205, 205));
        int tx = mid.x + 8;
        for (int i = 0; i < (int)secs.size(); i++) {
            int w = std::max(90, textW(secs[i].name, 16) + 28); if (tx + w > mid.x + mid.w - 10) break;
            Rect r{tx, a.y + 4, w, 30}; Col sc = HX(ON_COLORS[secs[i].color]);
            fillRect(r, i == cs ? sc : alpha(sc, 150));
            if (i == cs && renaming) { drawField(r, renameF, "Section name", ac, 16); if (!isFocus(&renameF)) { renaming = false; } if (in.btnA) {} }
            else text(fitText(secs[i].name, w - 16, 16), r.x + w / 2, r.y + 5, 16, C(255, 255, 255), i == cs ? BOLD : REG, 1);
            if (tapIn(r) && !(i == cs && renaming)) { if (i == cs) { renaming = true; renameF.t = secs[cs].name; renameF.cur = (int)renameF.t.size(); setFocus(&renameF); } else { cs = i; cp = 0; clearFocus(); renaming = false; return; } }
            tx += w + 3;
        }
        if (renaming) {
            for (auto& k : keyq) if (k.type == 2) { secs[cs].name = renameF.t.empty() ? secs[cs].name : renameF.t; renaming = false; clearFocus(); dirty = true; break; }
        }
        // page
        Rect pv{mid.x, a.y + 34, mid.w, vh - 34}; fillRect(pv, C(255, 255, 255));
        if (cp >= (int)secs[cs].pages.size()) cp = 0;
        ONPage& pg = page();
        Col sc = HX(ON_COLORS[secs[cs].color]);
        bool en = drawField({pv.x + 20, pv.y + 12, pv.w - 40, 54}, pg.title, "Untitled page", sc, 36, false);
        hline(pv.x + 20, pv.y + 70, 120, sc); hline(pv.x + 20, pv.y + 71, pv.w - 40, C(215, 215, 215));
        text(pg.created, pv.x + 22, pv.y + 76, 15, C(130, 130, 130));
        if (en) setFocus(&pg.body);
        EdOpts o; o.paper = false; o.baseSize = 21; o.pad = 20; o.color = C(40, 40, 40);
        pg.body.draw({pv.x, pv.y + 104, pv.w, pv.h - 104}, o);
        if (pg.body.changed) { dirty = true; pg.body.changed = false; }
        if (pg.title.t != lastTitle(cs, cp)) dirty = true;
        // right: page list
        fillRect(right, C(245, 245, 245)); vline(right.x, a.y, vh, C(205, 205, 205));
        Rect add{right.x + 10, a.y + 8, rw - 20, 34}; fillRect(add, pressIn(add) ? C(215, 215, 215) : C(255, 255, 255)); frameRect(add, C(200, 200, 200));
        text("+  Add Page", add.x + add.w / 2, add.y + 6, 17, ac, REG, 1);
        if (tapIn(add)) { addPage(); return; }
        Rect lr{right.x, a.y + 50, rw, vh - 50}; auto& pages = secs[cs].pages;
        scrollUpdate(pageSc, lr, rw, (int)pages.size() * 40);
        clip(lr);
        for (int i = 0; i < (int)pages.size(); i++) {
            Rect r{lr.x, lr.y + i * 40 - (int)pageSc.y, rw, 38};
            if (i == cp) { fillRect(r, alpha(sc, 60)); fillRect({r.x, r.y, 4, r.h}, sc); } else if (pressIn(r)) fillRect(r, C(225, 225, 225));
            std::string tt = pages[i].title.t.empty() ? "Untitled page" : pages[i].title.t;
            text(fitText(tt, rw - 30, 17), r.x + 16, r.y + 7, 17, C(50, 50, 50));
            if (tapIn(r)) { cp = i; clearFocus(); }
        }
        unclip();
        rememberTitle();
    }
    std::string lastT; int lastS = -1, lastP = -1;
    std::string lastTitle(int s, int p) { return (s == lastS && p == lastP) ? lastT : page().title.t; }
    void rememberTitle() { lastS = cs; lastP = cp; lastT = page().title.t; }
    std::string status() override { return "Notebook: " + docName + "     Section: " + secs[cs].name + "     Page " + std::to_string(cp + 1) + " of " + std::to_string(secs[cs].pages.size()); }
};
