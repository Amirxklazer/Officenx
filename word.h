#pragma once
#include "editor.h"

struct WordApp : App {
    DocEditor ed; float zoom = 1.f;
    WordApp() { id = A_WORD; reset(); docName = "Document1"; }
    const char* ext() override { return "wdoc"; }
    std::vector<std::string> openExts() override { return {"wdoc", "txt"}; }
    std::vector<const char*> tabNames() override { return {"Home", "Insert", "View"}; }
    void reset() override { ed.clear(); zoom = 1.f; setFocus(&ed, false); }
    bool undo() override { return ed.undo(); }
    bool redo() override { return ed.redo(); }
    std::string serialize() override { return "WDOC1\n" + ed.serialize(); }
    bool deserialize(const std::string& s) override {
        if (s.compare(0, 6, "WDOC1\n") == 0) ed.parse(s.substr(6)); else ed.parse(s, true);
        setFocus(&ed, false); return true;
    }
    std::string status() override {
        return "Page 1 of 1     " + std::to_string(ed.wordCount()) + " words     " + std::to_string(ed.charCount()) + " characters";
    }
    void statusExtra(int x1, int y) override { statusZoom(x1, y, zoom, 0.7f, 1.6f); }
    bool gallery(int x, int y, int w, const char* label, int style, bool active, Col ac) {
        Rect r{x, y, w, 70}; fillRect(r, C(255, 255, 255)); frameRect(r, active ? ac : C(200, 200, 200), active ? 2 : 1);
        if (pressIn(r)) fillRect(r, C(232, 238, 248));
        int sz = style == 3 ? 30 : (style == 1 ? 24 : (style == 2 ? 21 : 20)); int wt = style ? LIGHT : REG;
        Col col = (style == 1 || style == 2) ? HX(0x2E74B5) : C(40, 40, 40);
        text("AaBbCc", x + w / 2, y + 8 + (30 - textH(sz) + 8) / 2, sz, col, wt, 1);
        text(label, x + w / 2, y + 46, 14, C(80, 80, 80), REG, 1);
        return tapIn(r);
    }
    void ribbon(int t, Rect a) override {
        Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x; Para& p = ed.cur();
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 62, "B", "Bold", ac, p.b)) ed.toggleB(); x += 64;
            if (ribBtn(x, y, 62, "I", "Italic", ac, p.i)) ed.toggleI(); x += 64;
            if (ribBtn(x, y, 62, "U", "Underline", ac, p.u)) ed.toggleU(); x += 64;
            if (ribBtn(x, y, 62, "A-", "Smaller", ac, false, 26)) ed.bumpSize(-1); x += 64;
            if (ribBtn(x, y, 62, "A+", "Larger", ac, false, 26)) ed.bumpSize(1); x += 64;
            grp("Font");
            if (ribBtn(x, y, 66, "*", "Bullets", ac, p.bullet, 36)) ed.toggleBullet(); x += 68;
            if (ribBtnG(x, y, 62, "Left", ac, p.align == 0, [](int cx, int cy, Col c) { alignIcon(cx, cy, 0, c); })) ed.setAlign(0); x += 64;
            if (ribBtnG(x, y, 62, "Center", ac, p.align == 1, [](int cx, int cy, Col c) { alignIcon(cx, cy, 1, c); })) ed.setAlign(1); x += 64;
            if (ribBtnG(x, y, 62, "Right", ac, p.align == 2, [](int cx, int cy, Col c) { alignIcon(cx, cy, 2, c); })) ed.setAlign(2); x += 64;
            grp("Paragraph");
            const char* sn[4] = {"Normal", "Heading 1", "Heading 2", "Title"}; int ss[4] = {0, 1, 2, 3};
            for (int i = 0; i < 4; i++) { if (gallery(x, y, 108, sn[i], ss[i], p.style == ss[i], ac)) ed.setStyle(ss[i]); x += 112; }
            grp("Styles");
        } else if (t == 1) {
            if (ribBtn(x, y, 80, "Date", "Insert date", ac, false, 24)) { char b[64]; time_t n = time(0); strftime(b, sizeof b, "%d/%m/%Y", localtime(&n)); ed.insertText(b); setFocus(&ed, false); } x += 84;
            if (ribBtn(x, y, 80, "Time", "Insert time", ac, false, 24)) { char b[64]; time_t n = time(0); strftime(b, sizeof b, "%H:%M", localtime(&n)); ed.insertText(b); setFocus(&ed, false); } x += 84;
            grp("Text");
            if (ribBtn(x, y, 90, "* * *", "Divider", ac, false, 22)) { ed.insertPara("* * *"); ed.cur().align = 1; } x += 92;
            if (ribBtn(x, y, 90, "Aa", "Title page", ac, false, 28)) { ed.insertPara("Title"); ed.cur().style = 3; ed.insertPara(""); ed.cur().style = 0; } x += 92;
            grp("Page");
        } else {
            if (ribBtn(x, y, 80, "-", "Zoom out", ac, false, 40)) zoom = std::max(0.7f, zoom - 0.15f); x += 84;
            if (ribBtn(x, y, 80, "100%", "Reset zoom", ac, false, 24)) zoom = 1.f; x += 84;
            if (ribBtn(x, y, 80, "+", "Zoom in", ac, false, 40)) zoom = std::min(1.6f, zoom + 0.15f); x += 84;
            grp("Zoom");
        }
    }
    void content(Rect a) override {
        EdOpts o; o.zoom = zoom; o.baseSize = (int)(22 * zoom + 0.5f);
        Rect view{a.x, a.y + 22, a.w, H - oskH() - STATUS_H - a.y - 22};
        ed.draw(view, o);
        // ruler
        Rect rul{a.x, a.y, a.w, 22}; fillRect(rul, C(238, 238, 238));
        int pw = ed.pageW ? ed.pageW : 700, tx = ed.textX ? ed.textX : 56, px0 = (a.w - pw) / 2;
        fillRect({px0 + tx, a.y + 4, pw - 2 * tx, 14}, C(255, 255, 255));
        float step = 38 * zoom; int n = 0;
        for (float xx = px0 + tx; xx < px0 + pw - tx; xx += step / 2, n++) vline((int)xx, a.y + (n % 2 ? 9 : 7), n % 2 ? 4 : 8, C(140, 140, 140));
        hline(a.x, a.y + 21, a.w, C(200, 200, 200));
        if (ed.changed) { dirty = true; ed.changed = false; }
    }
};
