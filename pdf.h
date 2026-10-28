#pragma once
#include "app.h"
#include <zlib.h>

// Minimal PDF text extractor (text-based PDFs; no images, no custom font encodings).
static bool pdfInflate(const unsigned char* src, size_t n, std::string& out) {
    z_stream z; memset(&z, 0, sizeof z); if (inflateInit(&z) != Z_OK) return false;
    z.next_in = (Bytef*)src; z.avail_in = (uInt)n; unsigned char buf[16384]; int r;
    do { z.next_out = buf; z.avail_out = sizeof buf; r = inflate(&z, Z_NO_FLUSH); if (r != Z_OK && r != Z_STREAM_END) break; out.append((char*)buf, sizeof buf - z.avail_out); if (out.size() > 8 * 1024 * 1024) break; } while (r != Z_STREAM_END);
    inflateEnd(&z); return !out.empty();
}
static void pdfAppendByte(std::string& o, unsigned char c) {
    if (c < 0x80) o += (char)c; else { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
}
static std::string pdfContentText(const std::string& s) {
    std::string out; size_t i = 0, n = s.size(); std::vector<std::string> strs; std::vector<double> nums; double lastY = 0; bool haveY = false;
    auto nl = [&]() { if (!out.empty() && out.back() != '\n') out += '\n'; };
    while (i < n) {
        char c = s[i];
        if (c == '(') {
            std::string t; int depth = 1; i++;
            while (i < n && depth > 0) {
                char d = s[i++];
                if (d == '\\' && i < n) {
                    char e = s[i++];
                    if (e == 'n') t += '\n'; else if (e == 'r') {} else if (e == 't') t += ' '; else if (e == 'b' || e == 'f') {}
                    else if (e >= '0' && e <= '7') { int v = e - '0'; for (int k = 0; k < 2 && i < n && s[i] >= '0' && s[i] <= '7'; k++) v = v * 8 + (s[i++] - '0'); pdfAppendByte(t, (unsigned char)v); }
                    else if (e == '\n' || e == '\r') {} else t += e;
                } else if (d == '(') { depth++; t += d; } else if (d == ')') { depth--; if (depth > 0) t += d; }
                else if ((unsigned char)d >= 0x80) pdfAppendByte(t, (unsigned char)d); else t += d;
            }
            strs.push_back(t);
        } else if (c == '<' && i + 1 < n && s[i + 1] != '<') {
            size_t e = s.find('>', i); if (e == std::string::npos) break; std::string h, t;
            for (size_t k = i + 1; k < e; k++) if (isxdigit((unsigned char)s[k])) h += s[k];
            if (h.size() % 4 == 0 && h.size() >= 4) { bool ok = true; std::string tmp; for (size_t k = 0; k + 3 < h.size(); k += 4) { int hi = (int)strtol(h.substr(k, 2).c_str(), 0, 16), lo = (int)strtol(h.substr(k + 2, 2).c_str(), 0, 16); if (hi != 0) { ok = false; break; } if (lo >= 32 && lo < 127) tmp += (char)lo; } if (ok) t = tmp; }
            else if (h.size() % 2 == 0) { for (size_t k = 0; k + 1 < h.size(); k += 2) { int v = (int)strtol(h.substr(k, 2).c_str(), 0, 16); if (v >= 32 && v < 127) t += (char)v; } }
            strs.push_back(t); i = e + 1;
        } else if (c == '[' || c == ']') { i++; }
        else if (c == '%') { while (i < n && s[i] != '\n') i++; }
        else if (c == '/' ) { i++; while (i < n && !isspace((unsigned char)s[i]) && !strchr("/[]()<>", s[i])) i++; }
        else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') { char* e; double v = strtod(s.c_str() + i, &e); if (e == s.c_str() + i) { i++; } else { nums.push_back(v); i = e - s.c_str(); if (strs.size() && v < -250) strs.push_back(" "); } }
        else if (isalpha((unsigned char)c) || c == '\'' || c == '"' || c == '*') {
            size_t j = i; while (j < n && (isalpha((unsigned char)s[j]) || s[j] == '*' || s[j] == '\'' || s[j] == '"')) j++;
            std::string op = s.substr(i, j - i); i = j;
            if (op == "Tj" || op == "TJ" || op == "'" || op == "\"") { if (op == "'" || op == "\"") nl(); for (auto& t : strs) out += t; strs.clear(); }
            else if (op == "Td" || op == "TD") { if (nums.size() >= 2 && fabs(nums[nums.size() - 1]) > 0.5) nl(); else if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' '; }
            else if (op == "Tm") { if (nums.size() >= 6) { double y = nums[nums.size() - 1]; if (!haveY || fabs(y - lastY) > 1) nl(); else if (!out.empty() && out.back() != ' ') out += ' '; lastY = y; haveY = true; } }
            else if (op == "T*") nl();
            else if (op == "ET") nl();
            strs.clear(); nums.clear();
        } else i++;
    }
    return out;
}
static std::string pdfExtract(const std::string& d) {
    std::string all; size_t pos = 0;
    while (true) {
        size_t sp = d.find("stream", pos); if (sp == std::string::npos) break;
        if (sp >= 3 && d.compare(sp - 3, 3, "end") == 0) { pos = sp + 6; continue; }
        size_t ds = d.rfind("obj", sp); std::string dict = ds == std::string::npos ? "" : d.substr(ds, sp - ds);
        size_t b = sp + 6; if (b < d.size() && d[b] == '\r') b++; if (b < d.size() && d[b] == '\n') b++;
        size_t e = d.find("endstream", b); if (e == std::string::npos) break;
        pos = e + 9;
        if (dict.find("/Image") != std::string::npos || dict.find("/FontFile") != std::string::npos) continue;
        std::string content;
        if (dict.find("/FlateDecode") != std::string::npos) { if (!pdfInflate((const unsigned char*)d.data() + b, e - b, content)) continue; }
        else content = d.substr(b, e - b);
        if (content.find("BT") == std::string::npos || content.find("ET") == std::string::npos) continue;
        std::string t = pdfContentText(content); if (!t.empty()) { all += t; if (all.back() != '\n') all += '\n'; all += '\n'; }
    }
    return all;
}

struct PdfApp : App {
    std::string raw, text_, titleStr; std::vector<std::string> lines; int layW = -1; float layZ = 0;
    float zoom = 1.f; bool night = false; Scroll sc; int contentH = 0, viewH = 1;
    struct L { std::string s; bool gap; }; std::vector<L> wl;
    PdfApp() { id = A_PDF; reset(); docName = "Welcome"; }
    const char* ext() override { return "pdf"; }
    const char* baseName() override { return "Document"; }
    std::vector<std::string> openExts() override { return {"pdf"}; }
    std::vector<const char*> tabNames() override { return {"Home", "View"}; }
    bool wantsFlatBg() override { return false; }
    void reset() override {
        raw.clear(); layW = -1; sc = Scroll(); zoom = 1.f;
        text_ = "Welcome to Office\n\nThis is the PDF Reader. Open a PDF from File > Open. PDF files are read from the Documents folder on your SD card (switch/Office/Documents).\n\n"
                "What it can do\n\nShows the text of text-based PDFs, with zoom and a night mode for reading in the dark. Drag the page to scroll.\n\n"
                "Limits\n\nThis reader extracts text only. Pictures are not shown, and scanned PDFs or PDFs with special embedded fonts may show little or no text.\n\n"
                "Credits\n\nCredits to Microsoft and all of its developers and designers. See File > Info for details.";
    }
    bool canSave() override { return !raw.empty(); }
    std::string serialize() override { return raw; }   // never rewrites a PDF with different data
    bool deserialize(const std::string& d) override {
        if (d.compare(0, 4, "%PDF") != 0) return false;
        raw = d; text_ = pdfExtract(d); layW = -1; sc = Scroll(); zoom = 1.f;
        if (text_.size() < 8) text_ = "No readable text was found in this PDF.\n\nIt may be a scanned document, or it may use embedded fonts that this simple reader cannot decode. This reader shows text only (no pictures).";
        return true;
    }
    std::string status() override { int pages = std::max(1, (contentH + viewH - 1) / std::max(1, viewH)); int pg = std::min(pages, (int)(sc.y / std::max(1, viewH)) + 1); return "Page " + std::to_string(pg) + " of " + std::to_string(pages) + "     " + std::to_string(text_.size()) + " characters"; }
    void statusExtra(int x1, int y) override { statusZoom(x1, y, zoom, 0.7f, 2.0f); }
    void ribbon(int t, Rect a) override {
        Col ac = HX(APPS[id].ui); int y = a.y + 6, x = 10, g0 = x;
        auto grp = [&](const char* n) { ribGroupEnd(g0, x, n, a); x += 14; g0 = x; };
        if (t == 0) {
            if (ribBtn(x, y, 84, "-", "Zoom out", ac, false, 40)) zoom = std::max(0.7f, zoom - 0.15f); x += 86;
            if (ribBtn(x, y, 84, "100%", "Fit", ac, false, 24)) zoom = 1.f; x += 86;
            if (ribBtn(x, y, 84, "+", "Zoom in", ac, false, 40)) zoom = std::min(2.0f, zoom + 0.15f); x += 86;
            grp("Zoom");
            if (ribBtn(x, y, 90, "Top", "Go to top", ac, false, 24)) { sc.y = 0; sc.vy = 0; } x += 92;
            grp("Navigate");
        } else {
            if (ribBtn(x, y, 100, "Aa", "Day", ac, !night, 28)) night = false; x += 102;
            if (ribBtn(x, y, 100, "Aa", "Night", ac, night, 28)) night = true; x += 102;
            grp("Reading mode");
        }
    }
    void content(Rect a) override {
        if (raw.empty()) docName = "Welcome to Office";
        Rect view{a.x, a.y, a.w, H - oskH() - STATUS_H - a.y}; viewH = view.h;
        Col bg = night ? C(30, 30, 30) : C(230, 230, 230), pgc = night ? C(18, 18, 18) : C(255, 255, 255), tc = night ? C(225, 225, 225) : C(30, 30, 30);
        fillRect(view, bg);
        int pw = std::min(view.w - 24, (int)(820 * zoom)), tx = 50, fs = (int)(22 * zoom + 0.5f), lh = (int)(fs * 1.5f);
        int tw = pw - 2 * tx;
        if (layW != tw || layZ != zoom) {
            layW = tw; layZ = zoom; wl.clear();
            for (auto& ln : wrapText(text_, tw, fs)) wl.push_back({ln, ln.empty()});
            contentH = 60 + (int)wl.size() * lh + 60;
        }
        scrollUpdate(sc, view, view.w, contentH);
        int px0 = view.x + (view.w - pw) / 2;
        clip(view);
        fillRect({px0, view.y + 8 - (int)sc.y, pw, std::max(contentH, view.h)}, pgc);
        for (int i = 0; i < (int)wl.size(); i++) {
            int y = view.y + 8 + 40 + i * lh - (int)sc.y; if (y + lh < view.y || y > view.y + view.h) continue;
            bool head = i + 1 < (int)wl.size() && wl[i + 1].gap && !wl[i].s.empty() && wl[i].s.size() < 40 && (i == 0 || wl[i - 1].gap);
            if (head) text(wl[i].s, px0 + tx, y, (int)(fs * 1.25f), night ? C(255, 150, 140) : HX(APPS[id].ui), LIGHT);
            else text(wl[i].s, px0 + tx, y, fs, tc);
        }
        unclip();
    }
};
