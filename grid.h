#pragma once
// Generic spreadsheet-style grid with formula bar, used by Excel and Access.
#include "app.h"

struct GridCfg {
    int nRows = 0, nCols = 0;
    std::function<std::string(int, int)> disp, raw;
    std::function<void(int, int, const std::string&)> set;
    std::function<std::string(int)> colName;
    std::function<int(int)> colW;
    bool numRight = true; Col accent = {33, 115, 70, 255};
    const char* barLabel = "fx"; bool growOnEnter = false;
};
static bool looksNumeric(const std::string& s) {
    if (s.empty()) return false; char* e = nullptr; strtod(s.c_str(), &e); return e && *e == 0;
}
struct GridView {
    Scroll sc; int selR = 0, selC = 0, lastR = -1, lastC = -1; Field edit; bool wantVis = true, changed = false;
    std::string cellName(GridCfg& g) { return g.colName(selC) + std::to_string(selR + 1); }
    bool editing() { return isFocus(&edit); }
    void load(GridCfg& g) { edit.t = g.raw(selR, selC); edit.cur = (int)edit.t.size(); lastR = selR; lastC = selC; }
    void commit(GridCfg& g) { if (lastR >= 0 && lastR < g.nRows && lastC < g.nCols && edit.t != g.raw(lastR, lastC)) { g.set(lastR, lastC, edit.t); changed = true; } }
    void flush(GridCfg& g) { if (editing()) commit(g); }
    void beginEdit(GridCfg& g, const std::string& prefill) { edit.t = prefill; edit.cur = (int)prefill.size(); lastR = selR; lastC = selC; setFocus(&edit); }
    void select(GridCfg& g, int r, int c) {
        if (editing()) commit(g);
        selR = std::max(0, std::min(g.nRows - 1, r)); selC = std::max(0, std::min(g.nCols - 1, c)); wantVis = true;
        load(g);
    }
    void draw(Rect area, GridCfg& g) {
        const int barH = 46, hdrH = 32, rowHdrW = 58, rowH = 40;
        fillRect(area, C(255, 255, 255));
        // formula bar
        Rect bar{area.x, area.y, area.w, barH}; fillRect(bar, C(246, 246, 246)); hline(area.x, area.y + barH - 1, area.w, C(205, 205, 205));
        Rect nb{area.x + 8, area.y + 6, 96, 34}; fillRect(nb, C(255, 255, 255)); frameRect(nb, C(180, 180, 180));
        text(cellName(g), nb.x + nb.w / 2, nb.y + 5, 19, C(40, 40, 40), REG, 1);
        text(g.barLabel, area.x + 126, area.y + 8, 22, g.accent, BOLD, 1);
        if (lastR != selR || lastC != selC) { if (!editing()) load(g); }
        bool en = drawField({area.x + 150, area.y + 6, area.w - 160, 34}, edit, "Tap here to type", g.accent, 20);
        if (en) { commit(g); int nr = selR + 1; if (nr >= g.nRows + (g.growOnEnter ? 1 : 0)) nr = g.nRows - 1 + (g.growOnEnter ? 1 : 0); selR = nr; wantVis = true; load(g); edit.cur = (int)edit.t.size(); }
        Rect gr{area.x, area.y + barH, area.w, area.h - barH};
        int cw = rowHdrW; for (int c = 0; c < g.nCols; c++) cw += g.colW(c);
        int chh = hdrH + g.nRows * rowH;
        Rect inner{gr.x + rowHdrW, gr.y + hdrH, gr.w - rowHdrW, gr.h - hdrH};
        scrollUpdate(sc, inner, cw - rowHdrW, chh - hdrH, true);
        if (wantVis) {
            int x0 = 0; for (int c = 0; c < selC; c++) x0 += g.colW(c);
            int x1 = x0 + g.colW(selC), y0 = selR * rowH, y1 = y0 + rowH;
            if (x0 < sc.x) sc.x = (float)x0; else if (x1 > sc.x + inner.w) sc.x = (float)(x1 - inner.w);
            if (y0 < sc.y) sc.y = (float)y0; else if (y1 > sc.y + inner.h) sc.y = (float)(y1 - inner.h);
            if (sc.x < 0) sc.x = 0; if (sc.y < 0) sc.y = 0; wantVis = false;
        }
        // column positions
        std::vector<int> cx(g.nCols + 1, 0); for (int c = 0; c < g.nCols; c++) cx[c + 1] = cx[c] + g.colW(c);
        int r0 = std::max(0, (int)sc.y / rowH), r1 = std::min(g.nRows, (int)(sc.y + inner.h) / rowH + 2);
        int c0 = 0, c1 = g.nCols; while (c0 < g.nCols && cx[c0 + 1] < sc.x) c0++; while (c1 > c0 + 1 && cx[c1 - 1] > sc.x + inner.w) c1--;
        Col line = C(212, 212, 212);
        clip(inner);
        for (int r = r0; r < r1; r++) {
            int y = inner.y + r * rowH - (int)sc.y;
            for (int c = c0; c < c1; c++) {
                int x = inner.x + cx[c] - (int)sc.x, w = g.colW(c);
                Rect cell{x, y, w, rowH}; std::string d = g.disp(r, c);
                if (!d.empty()) {
                    bool right = g.numRight && (looksNumeric(d) || (d.size() && d[0] == '#' ? false : false));
                    std::string t = fitText(d, w - 12, 20);
                    if (right) text(t, x + w - 8, y + 8, 20, C(30, 30, 30), REG, 2); else text(t, x + 8, y + 8, 20, C(30, 30, 30));
                }
                vline(x + w - 1, y, rowH, line);
            }
            hline(inner.x, y + rowH - 1, inner.w, line);
        }
        // selection
        {
            int x = inner.x + cx[selC] - (int)sc.x, y = inner.y + selR * rowH - (int)sc.y;
            frameRect({x - 1, y - 1, g.colW(selC) + 1, rowH + 1}, g.accent, 3);
        }
        unclip();
        // headers
        clip({gr.x + rowHdrW, gr.y, gr.w - rowHdrW, hdrH});
        for (int c = c0; c < c1; c++) {
            int x = inner.x + cx[c] - (int)sc.x, w = g.colW(c); bool s = c == selC;
            fillRect({x, gr.y, w, hdrH}, s ? alpha(g.accent, 50) : C(238, 238, 238));
            vline(x + w - 1, gr.y, hdrH, C(190, 190, 190));
            text(g.colName(c), x + w / 2, gr.y + 5, 19, s ? g.accent : C(70, 70, 70), s ? BOLD : REG, 1);
        }
        unclip();
        clip({gr.x, inner.y, rowHdrW, inner.h});
        for (int r = r0; r < r1; r++) {
            int y = inner.y + r * rowH - (int)sc.y; bool s = r == selR;
            fillRect({gr.x, y, rowHdrW, rowH}, s ? alpha(g.accent, 50) : C(238, 238, 238));
            hline(gr.x, y + rowH - 1, rowHdrW, C(190, 190, 190));
            text(std::to_string(r + 1), gr.x + rowHdrW / 2, y + 8, 19, s ? g.accent : C(70, 70, 70), s ? BOLD : REG, 1);
        }
        unclip();
        fillRect({gr.x, gr.y, rowHdrW, hdrH}, C(238, 238, 238)); hline(gr.x, gr.y + hdrH - 1, gr.w, C(190, 190, 190)); vline(gr.x + rowHdrW - 1, gr.y, gr.h, C(190, 190, 190));
        // taps
        if (tapIn(inner)) {
            int lx = in.sx - inner.x + (int)sc.x, ly = in.sy - inner.y + (int)sc.y; int c = 0; while (c < g.nCols - 1 && cx[c + 1] <= lx) c++;
            int r = ly / rowH; if (r >= g.nRows) r = g.nRows - 1;
            if (r == selR && c == selC && !editing()) beginEdit(g, g.raw(r, c)); else select(g, r, c);
        }
        Rect colHdr{inner.x, gr.y, inner.w, hdrH};
        if (tapIn(colHdr)) { int lx = in.sx - inner.x + (int)sc.x; int c = 0; while (c < g.nCols - 1 && cx[c + 1] <= lx) c++; select(g, selR, c); }
        Rect rowHdr{gr.x, inner.y, rowHdrW, inner.h};
        if (tapIn(rowHdr)) { int r = (in.sy - inner.y + (int)sc.y) / rowH; select(g, r, selC); }
    }
};
