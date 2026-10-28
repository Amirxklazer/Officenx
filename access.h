#pragma once
#include "grid.h"

struct AccessApp : App {
    std::vector<std::string> fields; std::vector<std::vector<std::string>> rows;
    std::string tableName = "Contacts";
    GridView gv; GridCfg cfg; Field search, fieldName; std::vector<int> vis;
    AccessApp() { id = A_ACCESS; reset(); docName = "Database1"; }
    const char* ext() override { return "adb"; }
    const char* baseName() override { return "Database"; }
    std::vector<std::string> openExts() override { return {"adb"}; }
    std::vector<const char*> tabNames() override { return {"Home", "Fields"}; }
    bool wantsFlatBg() override { return true; }
    void reset() override { fields = {"Name", "Phone", "City"}; rows.clear(); gv = GridView(); search = Field(); fieldName = Field(); tableName = "Contacts"; }

    // ---- csv-like storage: first line = table|name, second = field names
    static void csvLine(std::string& out, const std::vector<std::string>& v) {
        for (size_t i = 0; i < v.size(); i++) {
            const std::string& s = v[i]; bool q = s.find_first_of(",\"\n") != std::string::npos;
            if (q) { out += '"'; for (char ch : s) { if (ch == '"') out += '"'; out += ch; } out += '"'; } else out += s;
            if (i + 1 < v.size()) out += ',';
        }
        out += '\n';
    }
    std::string serialize() override {
        gv.flush(cfg); std::string o = "ADB1\n"; csvLine(o, {tableName}); csvLine(o, fields);
        for (auto& r : rows) csvLine(o, r); return o;
    }
    bool deserialize(const std::string& d) override {
        if (d.compare(0, 5, "ADB1\n") != 0) return false;
        std::vector<std::vector<std::string>> recs; std::vector<std::string> cur; std::string f; bool q = false;
        for (size_t i = 5; i < d.size(); i++) {
            char ch = d[i];
            if (q) { if (ch == '"') { if (i + 1 < d.size() && d[i + 1] == '"') { f += '"'; i++; } else q = false; } else f += ch; }
            else if (ch == '"') q = true;
            else if (ch == ',') { cur.push_back(f); f.clear(); }
            else if (ch == '\n') { cur.push_back(f); f.clear(); recs.push_back(cur); cur.clear(); }
            else if (ch != '\r') f += ch;
        }
        if (recs.size() < 2) return false;
        tableName = recs[0].empty() ? "Table1" : recs[0][0]; fields = recs[1]; rows.assign(recs.begin() + 2, recs.end());
        for (auto& r : rows) r.resize(fields.size());
        return true;
    }

    void refreshVis() {
        vis.clear(); std::string q = search.t; for (auto& c : q) c = (char)tolower(c);
        for (int i = 0; i < (int)rows.size(); i++) {
            if (q.empty()) { vis.push_back(i); continue; }
            bool ok = false; for (auto& s : rows[i]) { std::string t = s; for (auto& c : t) c = (char)tolower(c); if (t.find(q) != std::string::npos) ok = true; }
            if (ok) vis.push_back(i);
        }
    }
    void cfgInit() {
        refreshVis();
        cfg.nRows = (int)vis.size() + 1; cfg.nCols = (int)fields.size() + 1; cfg.accent = HX(APPS[id].ui); cfg.numRight = false; cfg.barLabel = "Edit"; cfg.growOnEnter = true;
        cfg.disp = [this](int r, int c) -> std::string {
            if (r >= (int)vis.size()) return c == 0 ? "(New)" : "";
            if (c == 0) return std::to_string(vis[r] + 1);
            return rows[vis[r]][c - 1];
        };
        cfg.raw = [this](int r, int c) -> std::string { if (r >= (int)vis.size() || c == 0) return c == 0 && r < (int)vis.size() ? std::to_string(vis[r] + 1) : ""; return rows[vis[r]][c - 1]; };
        cfg.set = [this](int r, int c, const std::string& s) {
            if (c == 0) return;
            if (r >= (int)vis.size()) { rows.push_back(std::vector<std::string>(fields.size())); rows.back()[c - 1] = s; }
            else rows[vis[r]][c - 1] = s;
            dirty = true;
        };
        cfg.colName = [this](int c) { return c == 0 ? std::string("ID") : fields[c - 1]; };
        cfg.colW = [](int c) { return c == 0 ? 90 : 190; };
    }
    int selRow() { return gv.selR < (int)vis.size() ? vis[gv.selR] : -1; }
    void sortBy(bool asc) {
        gv.flush(cfg); int c = gv.selC - 1; if (c < 0 || rows.size() < 2) return;
        std::stable_sort(rows.begin(), rows.end(), [&](const std::vector<std::string>& a, const std::vector<std::string>& b) {
            bool na = looksNumeric(a[c]), nb = looksNumeric(b[c]);
            if (na && nb) return asc ? atof(a[c].c_str()) < atof(b[c].c_str()) : atof(a[c].c_str()) > atof(b[c].c_str());
            return asc ? a[c] < b[c] : a[c] > b[c];
        });
        dirty = true; gv.load(cfg);
    }
    void ribbon(int t, Rect a) override {
        cfgInit(); Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x;
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 80, "+", "New record", ac, false, 40)) { gv.flush(cfg); rows.push_back(std::vector<std::string>(fields.size())); dirty = true; search.t.clear(); cfgInit(); gv.select(cfg, (int)vis.size() - 1, std::max(1, gv.selC)); } x += 82;
            if (ribBtn(x, y, 80, "X", "Delete", ac, false, 30)) { int r = selRow(); if (r >= 0) { gv.flush(cfg); rows.erase(rows.begin() + r); dirty = true; clearFocus(); gv.lastR = -1; } } x += 82;
            grp("Records");
            if (ribBtn(x, y, 80, "A-Z", "Ascending", ac, false, 24)) sortBy(true); x += 82;
            if (ribBtn(x, y, 80, "Z-A", "Descending", ac, false, 24)) sortBy(false); x += 82;
            grp("Sort");
            text("Search", x + 4, a.y + 10, 15, C(90, 90, 90));
            drawField({x, a.y + 34, 300, 36}, search, "Type to filter records", ac, 20);
            x += 304; grp("Find");
        } else {
            text("Field name", x + 4, a.y + 10, 15, C(90, 90, 90));
            drawField({x, a.y + 34, 300, 36}, fieldName, "Field name", ac, 20); x += 312;
            if (ribBtn(x, y, 90, "+", "Add field", ac, false, 40)) { if (!fieldName.t.empty()) { gv.flush(cfg); fields.push_back(fieldName.t); for (auto& r : rows) r.push_back(""); fieldName.t.clear(); fieldName.cur = 0; dirty = true; gv.lastR = -1; clearFocus(); } else toast("Type a field name first"); } x += 92;
            if (ribBtn(x, y, 90, "Ab", "Rename", ac, false, 28)) { int c = gv.selC - 1; if (c >= 0 && !fieldName.t.empty()) { fields[c] = fieldName.t; dirty = true; clearFocus(); } else toast("Select a field and type a name"); } x += 92;
            if (ribBtn(x, y, 90, "X", "Delete field", ac, false, 30)) { int c = gv.selC - 1; if (c >= 0 && fields.size() > 1) { gv.flush(cfg); fields.erase(fields.begin() + c); for (auto& r : rows) r.erase(r.begin() + c); dirty = true; gv.selC = std::max(1, c); gv.lastR = -1; clearFocus(); } } x += 92;
            grp("Fields");
        }
    }
    void content(Rect a) override {
        cfgInit();
        int vh = H - oskH() - STATUS_H - a.y, pw = 230;
        // navigation pane
        Rect nav{a.x, a.y, pw, vh}; fillRect(nav, C(255, 255, 255)); vline(pw - 1, a.y, vh, C(205, 205, 205));
        Rect nh{a.x, a.y, pw, 34}; fillRect(nh, C(240, 240, 240)); text("All Access Objects", a.x + 12, a.y + 5, 18, HX(APPS[id].ui), BOLD);
        text("Tables", a.x + 12, a.y + 48, 16, C(110, 110, 110));
        Rect it{a.x, a.y + 76, pw - 1, 38}; fillRect(it, alpha(HX(APPS[id].ui), 40));
        frameRect({a.x + 14, a.y + 85, 20, 20}, HX(APPS[id].ui), 2); hline(a.x + 14, a.y + 92, 20, HX(APPS[id].ui)); vline(a.x + 24, a.y + 85, 20, HX(APPS[id].ui));
        text(fitText(tableName, pw - 60, 19), a.x + 44, a.y + 83, 19, C(40, 40, 40));
        // table tab + grid
        int gx = a.x + pw; Rect tab{gx + 8, a.y + 4, 150, 28};
        fillRect({gx, a.y, a.w - pw, 34}, C(238, 238, 238)); fillRect(tab, C(255, 255, 255)); frameRect(tab, C(200, 200, 200)); fillRect({tab.x, tab.y, tab.w, 3}, HX(APPS[id].ui));
        text(fitText(tableName, 126, 17), tab.x + 10, tab.y + 3, 17, C(40, 40, 40));
        Rect view{gx, a.y + 34, a.w - pw, vh - 34 - 30};
        gv.draw(view, cfg);
        if (gv.changed) { dirty = true; gv.changed = false; }
        // record navigator
        Rect nb{gx, view.y + view.h, a.w - pw, 30}; fillRect(nb, C(240, 240, 240)); hline(nb.x, nb.y, nb.w, C(200, 200, 200));
        int cr = std::min(gv.selR, (int)vis.size()) + 1;
        text("Record:", nb.x + 10, nb.y + 4, 16, C(70, 70, 70));
        Rect first{nb.x + 80, nb.y, 30, 30}, prev{nb.x + 112, nb.y, 30, 30}, next{nb.x + 220, nb.y, 30, 30}, last{nb.x + 252, nb.y, 30, 30};
        text("|<", first.x + 15, nb.y + 4, 16, C(70, 70, 70), BOLD, 1); text("<", prev.x + 15, nb.y + 4, 16, C(70, 70, 70), BOLD, 1);
        text(std::to_string(cr) + " of " + std::to_string(vis.size()), nb.x + 181, nb.y + 4, 16, C(40, 40, 40), REG, 1);
        text(">", next.x + 15, nb.y + 4, 16, C(70, 70, 70), BOLD, 1); text(">|", last.x + 15, nb.y + 4, 16, C(70, 70, 70), BOLD, 1);
        if (tapIn(first)) gv.select(cfg, 0, std::max(1, gv.selC)); if (tapIn(prev)) gv.select(cfg, gv.selR - 1, std::max(1, gv.selC));
        if (tapIn(next)) gv.select(cfg, gv.selR + 1, std::max(1, gv.selC)); if (tapIn(last)) gv.select(cfg, (int)vis.size() - 1, std::max(1, gv.selC));
        text(search.t.empty() ? "No Filter" : "Filtered", nb.x + 310, nb.y + 4, 16, C(70, 70, 70));
    }
    std::string status() override { return "Datasheet View     " + std::to_string(rows.size()) + " records, " + std::to_string(fields.size()) + " fields"; }
};
