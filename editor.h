#pragma once
// Paragraph-based text editor used by Word, OneNote and PowerPoint text boxes.
#include "app.h"

struct Para {
    std::string t; int style = 0;  // 0 Normal 1 Heading1 2 Heading2 3 Title
    bool b = false, i = false, u = false, bullet = false; int todo = 0;  // 0 none 1 open 2 done
    int align = 0, sz = 0;
};
struct Doc { std::vector<Para> p{Para{}}; };
struct Seg { int start, len; };
struct LineInfo { int para, start, len, y, h, x, w; };
struct EdOpts {
    bool paper = true; float zoom = 1.f; int baseSize = 22; Col color = {30, 30, 30, 255};
    bool useStyles = true; int pad = 0; bool scroll = true; bool center = false; bool readonly = false;
};

struct DocEditor {
    Doc doc; int cp = 0, cc = 0; Scroll sc; bool needVis = false, changed = false;
    std::vector<Doc> undoS, redoS; int lastType = -1; Uint32 lastT = 0;
    // layout cache
    std::vector<LineInfo> lines; int contentH = 0, layW = -1, layVer = -1, ver = 0; float layZ = 0; int layBase = 0; bool layStyles = true;
    int textX = 0, textW_ = 0, pageW = 0;

    void clear() { doc = Doc(); cp = cc = 0; undoS.clear(); redoS.clear(); sc = Scroll(); ver++; }
    Para& cur() { return doc.p[cp]; }
    void touch() { ver++; changed = true; }
    void snap(int type) {
        Uint32 t = SDL_GetTicks();
        if (type != lastType || t - lastT > 1500 || type >= 100) { undoS.push_back(doc); if (undoS.size() > 60) undoS.erase(undoS.begin()); redoS.clear(); }
        lastType = type; lastT = t;
    }
    bool undo() { if (undoS.empty()) return false; redoS.push_back(doc); doc = undoS.back(); undoS.pop_back(); clampCur(); touch(); needVis = true; lastType = -1; return true; }
    bool redo() { if (redoS.empty()) return false; undoS.push_back(doc); doc = redoS.back(); redoS.pop_back(); clampCur(); touch(); needVis = true; lastType = -1; return true; }
    void clampCur() { if (cp >= (int)doc.p.size()) cp = (int)doc.p.size() - 1; if (cp < 0) cp = 0; if (cc > (int)cur().t.size()) cc = (int)cur().t.size(); }

    // ---- paragraph formatting helpers (used from ribbons)
    void setStyle(int s) { snap(100); cur().style = s; if (s) { cur().bullet = false; cur().todo = 0; } touch(); }
    void toggleB() { snap(101); cur().b = !cur().b; touch(); }
    void toggleI() { snap(102); cur().i = !cur().i; touch(); }
    void toggleU() { snap(103); cur().u = !cur().u; touch(); }
    void toggleBullet() { snap(104); cur().bullet = !cur().bullet; cur().todo = 0; touch(); }
    void toggleTodo() { snap(105); cur().todo = cur().todo ? 0 : 1; cur().bullet = false; touch(); }
    void setAlign(int a) { snap(106); cur().align = a; touch(); }
    void bumpSize(int d) { snap(107); cur().sz = std::max(-6, std::min(20, cur().sz + d)); touch(); }
    void insertText(const std::string& s) { snap(100); clampCur(); cur().t.insert(cc, s); cc += (int)s.size(); touch(); needVis = true; }
    void insertPara(const std::string& s) { snap(100); Para n; n.t = s; doc.p.insert(doc.p.begin() + cp + 1, n); cp++; cc = (int)s.size(); touch(); needVis = true; }

    // ---- metrics
    int pSize(const Para& p, const EdOpts& o) {
        float m = 1; if (o.useStyles) { if (p.style == 1) m = 1.55f; else if (p.style == 2) m = 1.3f; else if (p.style == 3) m = 2.0f; }
        return std::max(8, (int)(o.baseSize * m) + p.sz * 2);
    }
    int pWeight(const Para& p, const EdOpts& o) { if (p.b) return BOLD; if (o.useStyles && p.style) return LIGHT; return REG; }
    int pSty(const Para& p) { return (p.i ? STY_I : 0) | (p.u ? STY_U : 0); }
    Col pCol(const Para& p, const EdOpts& o) { if (o.useStyles && (p.style == 1 || p.style == 2)) return HX(0x2E74B5); return o.color; }

    void wrapPara(const Para& p, int size, int weight, int sty, int avail, std::vector<Seg>& out) {
        const std::string& s = p.t; int n = (int)s.size(), start = 0, lastSp = -1, i = 0;
        if (n == 0) { out.push_back({0, 0}); return; }
        while (i < n) {
            int nx = u8next(s, i);
            if (s[i] == ' ') lastSp = i;
            if (textW(s.substr(start, nx - start), size, weight, sty) > avail && nx - start > 1) {
                int brk = (lastSp > start) ? lastSp + 1 : (i > start ? i : nx);
                out.push_back({start, brk - start}); start = brk; lastSp = -1; i = brk; continue;
            }
            i = nx;
        }
        out.push_back({start, n - start});
    }
    void layout(Rect view, const EdOpts& o) {
        if (o.paper) { pageW = std::min(view.w - 24, (int)(800 * o.zoom)); textX = (int)(56 * o.zoom); }
        else { pageW = view.w; textX = o.pad; }
        textW_ = pageW - textX * 2;
        if (layW == pageW && layVer == ver && layZ == o.zoom && layBase == o.baseSize && layStyles == o.useStyles) return;
        layW = pageW; layVer = ver; layZ = o.zoom; layBase = o.baseSize; layStyles = o.useStyles;
        lines.clear(); int y = o.paper ? (int)(36 * o.zoom) : o.pad;
        for (int pi = 0; pi < (int)doc.p.size(); pi++) {
            Para& p = doc.p[pi]; int size = pSize(p, o), wt = pWeight(p, o), sty = pSty(p);
            int indent = (p.bullet || p.todo) ? (int)(size * 1.3f) : 0;
            if (o.useStyles && (p.style == 1 || p.style == 2) && pi > 0) y += size / 2;
            std::vector<Seg> segs; wrapPara(p, size, wt, sty, textW_ - indent, segs);
            int lh = (int)(size * 1.45f);
            for (auto& sg : segs) {
                int w = textW(p.t.substr(sg.start, sg.len), size, wt, sty);
                int w2 = w; if (!p.t.empty() && sg.len && p.t[sg.start + sg.len - 1] == ' ') w2 = textW(p.t.substr(sg.start, sg.len - 1), size, wt, sty);
                int x = textX + indent;
                if (p.align == 1) x = textX + indent + (textW_ - indent - w2) / 2; else if (p.align == 2) x = textX + textW_ - w2;
                lines.push_back({pi, sg.start, sg.len, y, lh, x, w2}); y += lh;
            }
            y += (int)(size * 0.45f);
        }
        contentH = y + (o.paper ? (int)(60 * o.zoom) : 8);
    }
    int lineOfCursor() {
        for (int i = 0; i < (int)lines.size(); i++) {
            auto& l = lines[i]; if (l.para != cp) continue;
            bool last = (i + 1 >= (int)lines.size() || lines[i + 1].para != cp);
            if (cc >= l.start && (cc < l.start + l.len || (last && cc <= l.start + l.len))) return i;
        }
        return 0;
    }
    int colFromX(int li, int px, const EdOpts& o) {
        auto& l = lines[li]; Para& p = doc.p[l.para]; int size = pSize(p, o);
        std::string sub = p.t.substr(l.start, l.len);
        if (!sub.empty() && sub.back() == ' ' && l.start + l.len < (int)p.t.size()) sub.pop_back();
        return l.start + colAtX(sub, px - l.x, size, pWeight(p, o), pSty(p));
    }
    void handleKeys(const EdOpts& o) {
        for (auto& k : keyq) {
            clampCur();
            switch (k.type) {
            case 0: snap(1); cur().t.insert(cc, k.s); cc += (int)k.s.size(); touch(); needVis = true; break;
            case 1:
                snap(2);
                if (cc > 0) { int p = u8prev(cur().t, cc); cur().t.erase(p, cc - p); cc = p; }
                else if (cur().bullet || cur().todo) { cur().bullet = false; cur().todo = 0; }
                else if (cur().style) cur().style = 0;
                else if (cp > 0) { snap(200); int pl = (int)doc.p[cp - 1].t.size(); doc.p[cp - 1].t += cur().t; doc.p.erase(doc.p.begin() + cp); cp--; cc = pl; }
                touch(); needVis = true; break;
            case 7:
                snap(2);
                if (cc < (int)cur().t.size()) { int n = u8next(cur().t, cc); cur().t.erase(cc, n - cc); }
                else if (cp + 1 < (int)doc.p.size()) { snap(200); cur().t += doc.p[cp + 1].t; doc.p.erase(doc.p.begin() + cp + 1); }
                touch(); break;
            case 2: {
                snap(200);
                if ((cur().bullet || cur().todo) && cur().t.empty()) { cur().bullet = false; cur().todo = 0; touch(); break; }
                Para n = cur(); n.t = cur().t.substr(cc); cur().t = cur().t.substr(0, cc);
                if (n.style) n.style = 0; if (n.todo) n.todo = 1;
                doc.p.insert(doc.p.begin() + cp + 1, n); cp++; cc = 0; touch(); needVis = true; break; }
            case 3: if (cc > 0) cc = u8prev(cur().t, cc); else if (cp > 0) { cp--; cc = (int)cur().t.size(); } needVis = true; break;
            case 4: if (cc < (int)cur().t.size()) cc = u8next(cur().t, cc); else if (cp + 1 < (int)doc.p.size()) { cp++; cc = 0; } needVis = true; break;
            case 5: case 6: {
                int li = lineOfCursor(); auto& l = lines[li];
                int px = l.x + textW(cur().t.substr(l.start, cc - l.start), pSize(cur(), o), pWeight(cur(), o), pSty(cur()));
                int ni = li + (k.type == 5 ? -1 : 1);
                if (ni >= 0 && ni < (int)lines.size()) { cp = lines[ni].para; cc = colFromX(ni, px, o); }
                needVis = true; break; }
            }
        }
    }
    int wordCount() { int n = 0; for (auto& p : doc.p) { bool in_ = false; for (char c : p.t) { if (c == ' ') in_ = false; else if (!in_) { in_ = true; n++; } } } return n; }
    int charCount() { int n = 0; for (auto& p : doc.p) n += (int)p.t.size(); return n; }

    // Draw and handle input. Returns true if tapped (focus taken).
    void draw(Rect view, const EdOpts& o) {
        if (o.paper) fillRect(view, C(230, 230, 230));
        layout(view, o);
        if (o.scroll) scrollUpdate(sc, view, view.w, contentH);
        else { sc.y = 0; }
        bool foc = isFocus(this) && !o.readonly;
        if (foc) handleKeys(o);
        clampCur();
        // re-layout if edits happened this frame
        if (layVer != ver) layout(view, o);
        int li = lineOfCursor();
        if (needVis && o.scroll && foc) {
            int top = lines[li].y, bot = top + lines[li].h;
            if (top < sc.y) sc.y = (float)top; else if (bot > sc.y + view.h) sc.y = (float)(bot - view.h + 20);
            float maxY = (float)std::max(0, contentH - view.h); if (sc.y > maxY) sc.y = maxY; if (sc.y < 0) sc.y = 0; sc.vy = 0;
        }
        needVis = false;
        int px0 = view.x + (view.w - pageW) / 2;
        clip(view);
        if (o.paper) { Rect pg{px0, view.y + 8 - (int)sc.y, pageW, std::max(contentH, view.h - 8)}; fillRect(pg, C(255, 255, 255)); frameRect(pg, C(200, 200, 200), 1); }
        for (int i = 0; i < (int)lines.size(); i++) {
            auto& l = lines[i]; int ly = view.y + (o.paper ? 8 : 0) + l.y - (int)sc.y;
            if (ly + l.h < view.y || ly > view.y + view.h) continue;
            Para& p = doc.p[l.para]; int size = pSize(p, o), wt = pWeight(p, o), sty = pSty(p);
            Col col = pCol(p, o);
            if (l.start == 0) {
                int ix = px0 + textX;
                if (p.bullet) circle((float)ix + size * 0.35f, (float)ly + l.h / 2, std::max(3.f, size * 0.12f), col);
                if (p.todo) { int bs = (int)(size * 0.8f); Rect cb{ix, ly + (l.h - bs) / 2, bs, bs}; frameRect(cb, col, 2);
                    if (p.todo == 2) { lineP(cb.x + 3, cb.y + bs / 2, cb.x + bs / 3 + 1, cb.y + bs - 4, col, 2.5f); lineP(cb.x + bs / 3 + 1, cb.y + bs - 4, cb.x + bs - 3, cb.y + 4, col, 2.5f); } }
            }
            std::string sub = p.t.substr(l.start, l.len);
            Col dc = (p.todo == 2) ? alpha(col, 120) : col;
            text(sub, px0 + l.x, ly + (l.h - textH(size)) / 2 + 1, size, dc, wt, 0, sty);
            if (p.todo == 2 && l.w > 0) hline(px0 + l.x, ly + l.h / 2, l.w, dc);
        }
        if (foc && blinkOn()) {
            auto& l = lines[li]; Para& p = cur(); int size = pSize(p, o);
            int cx = px0 + l.x + textW(p.t.substr(l.start, cc - l.start), size, pWeight(p, o), pSty(p));
            int ly = view.y + (o.paper ? 8 : 0) + l.y - (int)sc.y;
            fillRect({cx, ly + 4, 2, l.h - 8}, p.todo == 2 ? C(120, 120, 120) : C(20, 20, 20));
        }
        unclip();
        if (!o.readonly && tapIn(view)) {
            // checkbox tap?
            int ty = in.sy - view.y - (o.paper ? 8 : 0) + (int)sc.y, tx = in.sx - px0;
            int best = (int)lines.size() - 1;
            for (int i = 0; i < (int)lines.size(); i++) if (ty < lines[i].y + lines[i].h) { best = i; break; }
            if (best >= 0 && !lines.empty()) {
                auto& l = lines[best]; Para& p = doc.p[l.para];
                if (p.todo && l.start == 0 && tx >= textX - 6 && tx <= textX + (int)(pSize(p, o) * 1.2f)) { snap(100); p.todo = p.todo == 1 ? 2 : 1; touch(); cp = l.para; cc = std::min(cc, (int)p.t.size()); }
                else { cp = l.para; cc = colFromX(best, tx, o); }
            }
            setFocus(this); needVis = true;
        }
    }

    // ---- serialization
    std::string serialize() {
        std::string out;
        for (auto& p : doc.p) {
            int fl = (p.b ? 1 : 0) | (p.i ? 2 : 0) | (p.u ? 4 : 0) | (p.bullet ? 8 : 0);
            out += std::to_string(p.style) + "|" + std::to_string(fl) + "|" + std::to_string(p.align) + "|" + std::to_string(p.sz) + "|" + std::to_string(p.todo) + "|" + p.t + "\n";
        }
        return out;
    }
    void parse(const std::string& s, bool plain = false) {
        doc.p.clear(); size_t i = 0;
        while (i <= s.size()) {
            size_t e = s.find('\n', i); if (e == std::string::npos) { if (i >= s.size()) break; e = s.size(); }
            std::string ln = s.substr(i, e - i); i = e + 1;
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            Para p;
            if (plain) p.t = ln;
            else {
                int f[5] = {0, 0, 0, 0, 0}; size_t pos = 0; bool ok = true;
                for (int k = 0; k < 5; k++) { size_t b = ln.find('|', pos); if (b == std::string::npos) { ok = false; break; } f[k] = atoi(ln.substr(pos, b - pos).c_str()); pos = b + 1; }
                if (!ok) { p.t = ln; } else { p.style = f[0]; p.b = f[1] & 1; p.i = f[1] & 2; p.u = f[1] & 4; p.bullet = f[1] & 8; p.align = f[2]; p.sz = f[3]; p.todo = f[4]; p.t = ln.substr(pos); }
            }
            doc.p.push_back(p);
        }
        if (doc.p.empty()) doc.p.push_back(Para{});
        cp = 0; cc = 0; undoS.clear(); redoS.clear(); ver++;
    }
};
