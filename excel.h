#pragma once
#include "grid.h"

struct Val { double n = 0; std::string s; bool str = false; std::string err; };

struct ExcelApp : App {
    static const int NR = 200, NC = 26;
    std::vector<std::vector<std::string>> cells;
    GridView gv; GridCfg cfg; std::string clipboard;
    ExcelApp() { id = A_EXCEL; reset(); docName = "Book1"; }
    const char* ext() override { return "csv"; }
    const char* baseName() override { return "Book"; }
    std::vector<std::string> openExts() override { return {"csv"}; }
    std::vector<const char*> tabNames() override { return {"Home", "Formulas", "Data"}; }
    bool wantsFlatBg() override { return true; }
    void reset() override { cells.assign(NR, std::vector<std::string>(NC)); gv = GridView(); clipboard.clear(); }

    // ---------------------------------------------------------------- formula engine
    static std::string colLetter(int c) { return std::string(1, (char)('A' + c)); }
    Val num(double n) { Val v; v.n = n; return v; }
    Val errv(const char* e) { Val v; v.err = e; return v; }
    Val cellVal(int r, int c, int depth) {
        if (r < 0 || r >= NR || c < 0 || c >= NC) return errv("#REF!");
        const std::string& s = cells[r][c];
        if (s.empty()) return num(0);
        if (s[0] == '=') return evalFormula(s.substr(1), depth + 1);
        if (looksNumeric(s)) return num(atof(s.c_str()));
        Val v; v.str = true; v.s = s; return v;
    }
    struct P { ExcelApp* x; const std::string& s; size_t i; int depth; };
    static void ws(P& p) { while (p.i < p.s.size() && p.s[p.i] == ' ') p.i++; }
    static bool isRef(const std::string& s, size_t i, int& col, int& row, size_t& end) {
        if (i >= s.size() || !isalpha((unsigned char)s[i])) return false;
        size_t j = i; while (j < s.size() && isalpha((unsigned char)s[j])) j++;
        if (j - i != 1 || j >= s.size() || !isdigit((unsigned char)s[j])) return false;
        col = toupper(s[i]) - 'A'; size_t k = j; row = 0; while (k < s.size() && isdigit((unsigned char)s[k])) { row = row * 10 + (s[k] - '0'); k++; }
        end = k; row -= 1; return true;
    }
    Val evalFormula(const std::string& f, int depth) {
        if (depth > 40) return errv("#CIRC!");
        P p{this, f, 0, depth}; Val v = parseCmp(p); ws(p);
        if (p.i < f.size() && v.err.empty()) return errv("#VALUE!");
        return v;
    }
    double asNum(const Val& v) { return v.str ? atof(v.s.c_str()) : v.n; }
    Val parseCmp(P& p) {
        Val a = parseAdd(p); ws(p);
        while (p.i < p.s.size()) {
            std::string op; char c = p.s[p.i];
            if (c == '<' || c == '>' || c == '=') { op = c; if (p.i + 1 < p.s.size() && (p.s[p.i + 1] == '=' || (c == '<' && p.s[p.i + 1] == '>'))) op += p.s[p.i + 1]; } else break;
            p.i += op.size(); Val b = parseAdd(p); ws(p);
            if (!a.err.empty()) continue; if (!b.err.empty()) { a = b; continue; }
            double x = asNum(a), y = asNum(b); bool r = false;
            if (op == "<") r = x < y; else if (op == ">") r = x > y; else if (op == "<=") r = x <= y; else if (op == ">=") r = x >= y; else if (op == "=") r = (a.str && b.str) ? a.s == b.s : x == y; else r = x != y;
            a = num(r ? 1 : 0);
        }
        return a;
    }
    Val parseAdd(P& p) {
        Val a = parseMul(p); ws(p);
        while (p.i < p.s.size() && (p.s[p.i] == '+' || p.s[p.i] == '-')) {
            char op = p.s[p.i++]; Val b = parseMul(p); ws(p);
            if (!a.err.empty()) continue; if (!b.err.empty()) { a = b; continue; }
            a = num(op == '+' ? asNum(a) + asNum(b) : asNum(a) - asNum(b));
        }
        return a;
    }
    Val parseMul(P& p) {
        Val a = parsePow(p); ws(p);
        while (p.i < p.s.size() && (p.s[p.i] == '*' || p.s[p.i] == '/')) {
            char op = p.s[p.i++]; Val b = parsePow(p); ws(p);
            if (!a.err.empty()) continue; if (!b.err.empty()) { a = b; continue; }
            if (op == '/' && asNum(b) == 0) { a = errv("#DIV/0!"); continue; }
            a = num(op == '*' ? asNum(a) * asNum(b) : asNum(a) / asNum(b));
        }
        return a;
    }
    Val parsePow(P& p) {
        Val a = parseUnary(p); ws(p);
        if (p.i < p.s.size() && p.s[p.i] == '^') { p.i++; Val b = parsePow(p); if (!a.err.empty()) return a; if (!b.err.empty()) return b; return num(pow(asNum(a), asNum(b))); }
        return a;
    }
    Val parseUnary(P& p) {
        ws(p);
        if (p.i < p.s.size() && p.s[p.i] == '-') { p.i++; Val v = parseUnary(p); if (v.err.empty()) v = num(-asNum(v)); return v; }
        if (p.i < p.s.size() && p.s[p.i] == '+') { p.i++; return parseUnary(p); }
        return parsePrimary(p);
    }
    Val parsePrimary(P& p) {
        ws(p); if (p.i >= p.s.size()) return errv("#VALUE!");
        char c = p.s[p.i];
        if (c == '(') { p.i++; Val v = parseCmp(p); ws(p); if (p.i < p.s.size() && p.s[p.i] == ')') p.i++; else return errv("#VALUE!"); return v; }
        if (c == '"') { size_t e = p.s.find('"', p.i + 1); if (e == std::string::npos) return errv("#VALUE!"); Val v; v.str = true; v.s = p.s.substr(p.i + 1, e - p.i - 1); p.i = e + 1; return v; }
        if (isdigit((unsigned char)c) || c == '.') { char* e; double d = strtod(p.s.c_str() + p.i, &e); p.i = e - p.s.c_str(); return num(d); }
        if (isalpha((unsigned char)c)) {
            int col, row; size_t end;
            if (isRef(p.s, p.i, col, row, end)) { p.i = end; return cellVal(row, col, p.depth); }
            size_t j = p.i; while (j < p.s.size() && isalpha((unsigned char)p.s[j])) j++;
            std::string fn = p.s.substr(p.i, j - p.i); for (auto& ch : fn) ch = (char)toupper(ch);
            if (j < p.s.size() && p.s[j] == '(') { p.i = j + 1; return callFn(p, fn); }
            return errv("#NAME?");
        }
        return errv("#VALUE!");
    }
    Val callFn(P& p, const std::string& fn) {
        std::vector<Val> args; std::vector<double> flat; std::string firstErr;
        ws(p);
        if (p.i < p.s.size() && p.s[p.i] == ')') p.i++;
        else for (;;) {
            ws(p); int c1, r1, c2, r2; size_t e1, e2;
            if (isRef(p.s, p.i, c1, r1, e1) && e1 < p.s.size() && p.s[e1] == ':' && isRef(p.s, e1 + 1, c2, r2, e2)) {
                p.i = e2;
                for (int r = std::min(r1, r2); r <= std::max(r1, r2); r++) for (int c = std::min(c1, c2); c <= std::max(c1, c2); c++) {
                    if (r < 0 || r >= NR || c < 0 || c >= NC) { if (firstErr.empty()) firstErr = "#REF!"; continue; }
                    const std::string& s = cells[r][c]; if (s.empty()) continue;
                    Val v = cellVal(r, c, p.depth); if (!v.err.empty()) { if (firstErr.empty()) firstErr = v.err; continue; }
                    if (!v.str) flat.push_back(v.n);
                }
            } else {
                Val v = parseCmp(p); args.push_back(v);
                if (!v.err.empty() && firstErr.empty() && fn != "IF") firstErr = v.err;
                if (v.err.empty() && !v.str) flat.push_back(v.n);
            }
            ws(p);
            if (p.i < p.s.size() && p.s[p.i] == ',') { p.i++; continue; }
            if (p.i < p.s.size() && p.s[p.i] == ')') { p.i++; break; }
            return errv("#VALUE!");
        }
        if (fn == "IF") {
            if (args.size() < 2) return errv("#VALUE!");
            if (!args[0].err.empty()) return args[0];
            bool t = args[0].str ? !args[0].s.empty() : args[0].n != 0;
            if (t) return args[1]; return args.size() > 2 ? args[2] : num(0);
        }
        if (!firstErr.empty()) return errv(firstErr.c_str());
        if (fn == "SUM") { double s = 0; for (double d : flat) s += d; return num(s); }
        if (fn == "AVERAGE") { if (flat.empty()) return errv("#DIV/0!"); double s = 0; for (double d : flat) s += d; return num(s / flat.size()); }
        if (fn == "MIN") { if (flat.empty()) return num(0); return num(*std::min_element(flat.begin(), flat.end())); }
        if (fn == "MAX") { if (flat.empty()) return num(0); return num(*std::max_element(flat.begin(), flat.end())); }
        if (fn == "COUNT") return num((double)flat.size());
        if (fn == "PRODUCT") { double s = 1; for (double d : flat) s *= d; return num(s); }
        if (flat.empty()) return errv("#VALUE!");
        if (fn == "ABS") return num(fabs(flat[0]));
        if (fn == "SQRT") { if (flat[0] < 0) return errv("#NUM!"); return num(sqrt(flat[0])); }
        if (fn == "ROUND") { double m = pow(10.0, flat.size() > 1 ? flat[1] : 0); return num(round(flat[0] * m) / m); }
        if (fn == "POWER" && flat.size() > 1) return num(pow(flat[0], flat[1]));
        return errv("#NAME?");
    }
    static std::string fmtNum(double d) {
        char b[64]; if (fabs(d) < 1e15 && d == floor(d)) snprintf(b, sizeof b, "%.0f", d); else snprintf(b, sizeof b, "%.10g", d);
        return b;
    }
    std::string display(int r, int c) {
        const std::string& s = cells[r][c];
        if (s.empty() || s[0] != '=') return s;
        Val v = evalFormula(s.substr(1), 0);
        if (!v.err.empty()) return v.err;
        return v.str ? v.s : fmtNum(v.n);
    }

    // ---------------------------------------------------------------- csv
    std::string serialize() override {
        gv.flush(cfg);
        int lr = -1, lc = -1; for (int r = 0; r < NR; r++) for (int c = 0; c < NC; c++) if (!cells[r][c].empty()) { lr = std::max(lr, r); lc = std::max(lc, c); }
        std::string out;
        for (int r = 0; r <= lr; r++) {
            for (int c = 0; c <= lc; c++) {
                const std::string& s = cells[r][c]; bool q = s.find_first_of(",\"\n") != std::string::npos;
                if (q) { out += '"'; for (char ch : s) { if (ch == '"') out += '"'; out += ch; } out += '"'; } else out += s;
                if (c < lc) out += ',';
            }
            out += '\n';
        }
        return out;
    }
    bool deserialize(const std::string& d) override {
        size_t i = 0; int r = 0, c = 0; std::string f; bool q = false;
        auto endField = [&]() { if (r < NR && c < NC) cells[r][c] = f; f.clear(); c++; };
        while (i < d.size()) {
            char ch = d[i++];
            if (q) { if (ch == '"') { if (i < d.size() && d[i] == '"') { f += '"'; i++; } else q = false; } else f += ch; }
            else if (ch == '"') q = true;
            else if (ch == ',') endField();
            else if (ch == '\n') { endField(); r++; c = 0; }
            else if (ch != '\r') f += ch;
        }
        if (!f.empty() || c > 0) endField();
        return true;
    }

    // ---------------------------------------------------------------- ribbon
    void cfgInit() {
        cfg.nRows = NR; cfg.nCols = NC; cfg.accent = HX(APPS[id].ui);
        cfg.disp = [this](int r, int c) { return display(r, c); };
        cfg.raw = [this](int r, int c) { return cells[r][c]; };
        cfg.set = [this](int r, int c, const std::string& s) { cells[r][c] = s; dirty = true; };
        cfg.colName = [](int c) { return colLetter(c); };
        cfg.colW = [](int) { return 120; };
    }
    void putCell(const std::string& s) { gv.flush(cfg); cells[gv.selR][gv.selC] = s; dirty = true; gv.load(cfg); }
    std::string autoRange(const char* fn) {
        int r = gv.selR, c = gv.selC; int r0 = r;
        while (r0 - 1 >= 0 && looksNumeric(display(r0 - 1, c)) && !cells[r0 - 1][c].empty()) r0--;
        if (r0 < r) return std::string("=") + fn + "(" + colLetter(c) + std::to_string(r0 + 1) + ":" + colLetter(c) + std::to_string(r) + ")";
        int c0 = c; while (c0 - 1 >= 0 && looksNumeric(display(r, c0 - 1)) && !cells[r][c0 - 1].empty()) c0--;
        if (c0 < c) return std::string("=") + fn + "(" + colLetter(c0) + std::to_string(r + 1) + ":" + colLetter(c - 1) + std::to_string(r + 1) + ")";
        return std::string("=") + fn + "(";
    }
    void startFn(const char* fn) { std::string s = autoRange(fn); gv.selR = gv.selR; gv.beginEdit(cfg, s); }
    void sortCol(bool asc) {
        gv.flush(cfg); int lr = -1; for (int r = 0; r < NR; r++) for (int c = 0; c < NC; c++) if (!cells[r][c].empty()) lr = std::max(lr, r);
        if (lr < 1) return; int col = gv.selC;
        std::vector<std::vector<std::string>> rows(cells.begin(), cells.begin() + lr + 1);
        std::stable_sort(rows.begin(), rows.end(), [&](const std::vector<std::string>& a, const std::vector<std::string>& b) {
            std::string x = a[col], y = b[col];
            bool nx = looksNumeric(x), ny = looksNumeric(y);
            bool less; if (nx && ny) less = atof(x.c_str()) < atof(y.c_str()); else if (nx != ny) less = nx; else less = x < y;
            bool gt; if (nx && ny) gt = atof(x.c_str()) > atof(y.c_str()); else if (nx != ny) gt = !nx; else gt = x > y;
            return asc ? less : gt;
        });
        for (int r = 0; r <= lr; r++) cells[r] = rows[r];
        dirty = true; gv.load(cfg);
    }
    void ribbon(int t, Rect a) override {
        Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x;
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 66, "Cp", "Copy", ac, false, 26)) { clipboard = cells[gv.selR][gv.selC]; toast("Copied"); } x += 68;
            if (ribBtn(x, y, 66, "Ps", "Paste", ac, false, 26)) putCell(clipboard); x += 68;
            if (ribBtn(x, y, 66, "X", "Clear", ac, false, 26)) putCell(""); x += 68;
            grp("Clipboard");
            if (ribBtn(x, y, 80, "Sum", "AutoSum", ac, false, 24)) startFn("SUM"); x += 82;
            if (ribBtn(x, y, 80, "Avg", "Average", ac, false, 24)) startFn("AVERAGE"); x += 82;
            if (ribBtn(x, y, 80, "Dn", "Fill down", ac, false, 24)) { gv.flush(cfg); int r = gv.selR; if (r > 0) { cells[r][gv.selC] = cells[r - 1][gv.selC]; dirty = true; gv.load(cfg); } } x += 82;
            grp("Editing");
            if (ribBtn(x, y, 80, "+Row", "Insert row", ac, false, 22)) { gv.flush(cfg); cells.insert(cells.begin() + gv.selR, std::vector<std::string>(NC)); cells.pop_back(); dirty = true; gv.load(cfg); } x += 82;
            if (ribBtn(x, y, 80, "-Row", "Delete row", ac, false, 22)) { gv.flush(cfg); cells.erase(cells.begin() + gv.selR); cells.push_back(std::vector<std::string>(NC)); dirty = true; gv.load(cfg); } x += 82;
            grp("Cells");
        } else if (t == 1) {
            const char* fn[] = {"SUM", "AVERAGE", "MIN", "MAX", "COUNT", "ROUND", "IF", "SQRT"};
            for (int i = 0; i < 8; i++) {
                std::string lbl = fn[i];
                if (ribBtn(x, y, 96, "fx", lbl.c_str(), ac, false, 26)) { if (i < 5) startFn(fn[i]); else gv.beginEdit(cfg, std::string("=") + fn[i] + "("); }
                x += 98;
            }
            grp("Function library");
        } else {
            if (ribBtn(x, y, 100, "A-Z", "Sort A to Z", ac, false, 24)) sortCol(true); x += 102;
            if (ribBtn(x, y, 100, "Z-A", "Sort Z to A", ac, false, 24)) sortCol(false); x += 102;
            grp("Sort");
        }
    }
    void content(Rect a) override {
        cfgInit();
        Rect view{a.x, a.y, a.w, H - oskH() - STATUS_H - a.y - 30};
        gv.draw(view, cfg);
        Rect tb{a.x, view.y + view.h, a.w, 30}; fillRect(tb, C(240, 240, 240)); hline(tb.x, tb.y, tb.w, C(200, 200, 200));
        Rect st{a.x + 40, tb.y, 110, 30}; fillRect(st, C(255, 255, 255)); fillRect({st.x, st.y + 27, st.w, 3}, HX(APPS[id].ui)); vline(st.x, tb.y, 30, C(200, 200, 200)); vline(st.x + st.w, tb.y, 30, C(200, 200, 200));
        text("Sheet1", st.x + st.w / 2, tb.y + 4, 17, HX(APPS[id].ui), BOLD, 1);
        circle((float)a.x + 192, (float)tb.y + 15, 11, C(150, 150, 150)); hline(a.x + 186, tb.y + 15, 12, C(255, 255, 255)); vline(a.x + 192, tb.y + 9, 12, C(255, 255, 255));
        if (gv.changed) { dirty = true; gv.changed = false; }
    }
    std::string status() override {
        int n = 0; for (auto& r : cells) for (auto& s : r) if (!s.empty()) n++;
        return "Ready     Cell " + gv.cellName(cfg) + "     Cells used: " + std::to_string(n);
    }
};
