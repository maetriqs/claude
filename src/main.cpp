// By The_headphones
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>
#include <fstream>
#include <sstream>
#include <string>
#include <array>
#include <optional>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "mem.hpp"
#include "process.hpp"
#include "ac.hpp"
#include "features.hpp"
#include "aimbot.hpp"

// ── D3D11 globals ────────────────────────────────────────────────────────────

static ID3D11Device*           g_device    = nullptr;
static ID3D11DeviceContext*    g_ctx       = nullptr;
static IDXGISwapChain*         g_swap      = nullptr;
static ID3D11RenderTargetView* g_rtv       = nullptr;

static bool  CreateDeviceD3D(HWND hWnd);
static void  CleanupDeviceD3D();
static void  CreateRenderTarget();
static void  CleanupRenderTarget();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_swap->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── Game state ───────────────────────────────────────────────────────────────

static Process     g_proc;
static uintptr_t   g_base     = 0;
static bool        g_attached = false;
static Features    g_fx;
static AimbotLoop  g_aimbotLoop;
static AimbotConfig g_aimbotCfg;
static std::array<std::optional<Vec3>, 5> g_slots;

// ── Style ────────────────────────────────────────────────────────────────────

static void applyStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c     = s.Colors;

    // Geometry
    s.WindowPadding     = { 12, 10 };
    s.FramePadding      = { 8,  4  };
    s.ItemSpacing       = { 8,  5  };
    s.ItemInnerSpacing  = { 6,  4  };
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;

    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    // Palette  (dark blue-grey, GitHub-ish)
    auto col = [](float r, float g, float b, float a = 1.0f) { return ImVec4(r,g,b,a); };

    c[ImGuiCol_WindowBg]            = col(0.055f, 0.063f, 0.075f);
    c[ImGuiCol_ChildBg]             = col(0.075f, 0.086f, 0.102f);
    c[ImGuiCol_PopupBg]             = col(0.09f,  0.10f,  0.12f);
    c[ImGuiCol_Border]              = col(0.18f,  0.21f,  0.27f);
    c[ImGuiCol_BorderShadow]        = col(0,0,0,0);
    c[ImGuiCol_FrameBg]             = col(0.11f,  0.13f,  0.16f);
    c[ImGuiCol_FrameBgHovered]      = col(0.16f,  0.19f,  0.25f);
    c[ImGuiCol_FrameBgActive]       = col(0.20f,  0.25f,  0.33f);
    c[ImGuiCol_TitleBg]             = col(0.07f,  0.08f,  0.10f);
    c[ImGuiCol_TitleBgActive]       = col(0.07f,  0.13f,  0.24f);
    c[ImGuiCol_TitleBgCollapsed]    = col(0.05f,  0.05f,  0.07f);
    c[ImGuiCol_MenuBarBg]           = col(0.07f,  0.08f,  0.10f);
    c[ImGuiCol_ScrollbarBg]         = col(0.05f,  0.06f,  0.07f);
    c[ImGuiCol_ScrollbarGrab]       = col(0.22f,  0.28f,  0.38f);
    c[ImGuiCol_ScrollbarGrabHovered]= col(0.30f,  0.38f,  0.52f);
    c[ImGuiCol_ScrollbarGrabActive] = col(0.36f,  0.46f,  0.64f);
    c[ImGuiCol_CheckMark]           = col(0.22f,  0.64f,  1.00f);
    c[ImGuiCol_SliderGrab]          = col(0.22f,  0.58f,  0.92f);
    c[ImGuiCol_SliderGrabActive]    = col(0.30f,  0.70f,  1.00f);
    c[ImGuiCol_Button]              = col(0.12f,  0.28f,  0.52f);
    c[ImGuiCol_ButtonHovered]       = col(0.18f,  0.40f,  0.72f);
    c[ImGuiCol_ButtonActive]        = col(0.22f,  0.48f,  0.84f);
    c[ImGuiCol_Header]              = col(0.14f,  0.30f,  0.55f, 0.85f);
    c[ImGuiCol_HeaderHovered]       = col(0.20f,  0.40f,  0.68f);
    c[ImGuiCol_HeaderActive]        = col(0.24f,  0.48f,  0.80f);
    c[ImGuiCol_Separator]           = col(0.18f,  0.21f,  0.27f);
    c[ImGuiCol_SeparatorHovered]    = col(0.26f,  0.50f,  0.80f);
    c[ImGuiCol_SeparatorActive]     = col(0.30f,  0.60f,  1.00f);
    c[ImGuiCol_ResizeGrip]          = col(0.22f,  0.44f,  0.72f, 0.50f);
    c[ImGuiCol_ResizeGripHovered]   = col(0.28f,  0.56f,  0.90f);
    c[ImGuiCol_ResizeGripActive]    = col(0.34f,  0.66f,  1.00f);
    c[ImGuiCol_Tab]                 = col(0.09f,  0.11f,  0.14f);
    c[ImGuiCol_TabHovered]          = col(0.20f,  0.40f,  0.68f);
    c[ImGuiCol_TabActive]           = col(0.12f,  0.28f,  0.52f);
    c[ImGuiCol_TabUnfocused]        = col(0.07f,  0.09f,  0.11f);
    c[ImGuiCol_TabUnfocusedActive]  = col(0.10f,  0.17f,  0.30f);
    c[ImGuiCol_Text]                = col(0.88f,  0.92f,  0.98f);
    c[ImGuiCol_TextDisabled]        = col(0.38f,  0.42f,  0.50f);
    c[ImGuiCol_PlotHistogram]       = col(0.22f,  0.64f,  1.00f);
    c[ImGuiCol_PlotHistogramHovered]= col(0.36f,  0.78f,  1.00f);
}

// ── UI helpers ───────────────────────────────────────────────────────────────

static void hpBar(const char* label, int val, int maxVal,
                  ImVec4 lo = {0.85f,0.22f,0.22f,1}, ImVec4 hi = {0.22f,0.72f,0.36f,1})
{
    float frac = (maxVal > 0) ? static_cast<float>(val) / maxVal : 0.0f;
    frac = ImClamp(frac, 0.0f, 1.0f);
    ImVec4 barCol = ImVec4(
        lo.x + (hi.x - lo.x) * frac,
        lo.y + (hi.y - lo.y) * frac,
        lo.z + (hi.z - lo.z) * frac, 1.0f);
    char overlay[32];
    snprintf(overlay, sizeof(overlay), "%d / %d", val, maxVal);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barCol);
    ImGui::ProgressBar(frac, ImVec2(-1, 14), overlay);
    ImGui::PopStyleColor();
    if (label[0]) { ImGui::SameLine(0, 8); ImGui::TextDisabled("%s", label); }
}

static void badge(const char* text, ImVec4 col)
{
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

static bool toggle(const char* label, bool& v)
{
    ImVec4 on  = {0.16f, 0.56f, 0.28f, 1.0f};
    ImVec4 off = {0.26f, 0.26f, 0.30f, 1.0f};
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        v ? on  : off);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, v ? ImVec4{0.20f,0.70f,0.34f,1} : ImVec4{0.32f,0.32f,0.36f,1});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  v ? on  : off);
    ImGui::PushStyleColor(ImGuiCol_CheckMark,      {1,1,1,1});
    bool changed = ImGui::Checkbox(label, &v);
    ImGui::PopStyleColor(4);
    return changed;
}

// ── Attach / detach ───────────────────────────────────────────────────────────

static void tryAttach()
{
    DWORD pid = findPid("ac_client.exe");
    if (!pid) return;
    if (!g_proc.open(pid)) return;
    g_base = getModuleBase(pid, "ac_client.exe");
    if (!g_base) { g_proc.close(); return; }
    g_attached = true;
}

static void doDetach()
{
    g_fx.stopAll();
    g_aimbotLoop.stop();
    g_proc.close();
    g_base     = 0;
    g_attached = false;
}

// ── Tabs ─────────────────────────────────────────────────────────────────────

static void tabStatus(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();
    if (!lp) { ImGui::TextDisabled("LocalPlayer null — join a match"); return; }

    std::string n = ac.name(lp);
    ImGui::Text("Player:  %s", n.c_str());
    ImGui::Spacing();

    int hp    = ac.health(lp);
    int armor = ac.armor(lp);
    hpBar("Health", hp,    200);
    hpBar("Armor",  armor, 200,
          {0.72f,0.60f,0.22f,1}, {0.38f,0.62f,0.92f,1});

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Position");
    ImGui::Indent();
    ImGui::Text("X  %.2f", ac.posX(lp));
    ImGui::Text("Y  %.2f", ac.posY(lp));
    ImGui::Text("Z  %.2f", ac.posZ(lp));
    ImGui::Unindent();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Ammo");
    ImGui::Indent();
    if (ImGui::BeginTable("ammo", 4, ImGuiTableFlags_SizingFixedFit)) {
        auto row = [&](const char* label, uintptr_t slot) {
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s", label);
            ImGui::TableNextColumn(); ImGui::Text("%d", ac.ammo(lp, slot));
        };
        row("AR",      Ent::AMMO_AR);
        row("SMG",     Ent::AMMO_SMG);
        ImGui::TableNextRow();
        row("Sniper",  Ent::AMMO_SNIPER);
        row("Shotgun", Ent::AMMO_SHOTGUN);
        ImGui::TableNextRow();
        row("Pistol",  Ent::AMMO_PISTOL);
        row("Grenade", Ent::AMMO_GRENADE);
        ImGui::TableNextRow();
        row("Carbine", Ent::AMMO_CARBINE);
        ImGui::EndTable();
    }
    ImGui::Unindent();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Server:  %d player(s)", ac.playerCount());
    ImGui::Text("FOV:     %d", ac.fov());
}

static void tabPlayers(const ACGame& ac)
{
    uintptr_t lp  = ac.localPlayer();
    Vec3 lpPos    = lp ? ac.pos(lp) : Vec3{};
    int  count    = ac.playerCount();

    ImGui::Text("%d player(s)", count);
    ImGui::Spacing();

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit;

    float rowH = ImGui::GetTextLineHeightWithSpacing();
    if (!ImGui::BeginTable("players", 5, flags, {-1, rowH * 10})) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("IDX",  ImGuiTableColumnFlags_WidthFixed, 36);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("HP",   ImGuiTableColumnFlags_WidthFixed, 150);
    ImGui::TableSetupColumn("ARM",  ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("Dist", ImGuiTableColumnFlags_WidthFixed, 54);
    ImGui::TableHeadersRow();

    for (int i = 0; i < count; ++i) {
        uintptr_t ent = ac.entity(i);
        if (!ent) continue;
        int   hp   = ac.health(ent);
        int   arm  = ac.armor(ent);
        Vec3  ep   = ac.pos(ent);
        float d    = dist2d(lpPos, ep);
        std::string nm = ac.name(ent);
        if (nm.empty()) nm = "-";

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("[%d]", i);
        ImGui::TableSetColumnIndex(1);
        if (hp <= 0) ImGui::TextDisabled("%s", nm.c_str());
        else         ImGui::TextUnformatted(nm.c_str());
        ImGui::TableSetColumnIndex(2);
        hpBar("", hp, 200);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", arm);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.0f", d);
    }
    ImGui::EndTable();

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::Text("Kill");
    ImGui::SameLine();
    static int killIdx = 0;
    ImGui::SetNextItemWidth(60);
    ImGui::InputInt("##kidx", &killIdx, 0);
    ImGui::SameLine();
    if (ImGui::Button("Kill##one")) {
        uintptr_t ent = ac.entity(killIdx);
        if (ent) ac.setHealth(ent, 0);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.55f,0.12f,0.12f,1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.72f,0.18f,0.18f,1});
    if (ImGui::Button("Kill All")) {
        for (int i = 0; i < count; ++i) {
            uintptr_t ent = ac.entity(i);
            if (!ent || ent == lp) continue;
            ac.setHealth(ent, 0);
        }
    }
    ImGui::PopStyleColor(2);
}

static void tabRadar(const ACGame& ac)
{
    static float radarScale = 12.0f;
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Scale (units/px)", &radarScale, 4.0f, 40.0f, "%.1f");
    ImGui::Spacing();

    // Compute available space and pick a square
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float  side  = std::min(avail.x, avail.y) - 8.0f;
    float  half  = side * 0.5f;

    ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImVec2 center  = { topLeft.x + half, topLeft.y + half };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background + border
    dl->AddCircleFilled(center, half, IM_COL32(10, 13, 18, 255));
    dl->AddCircle(center, half,        IM_COL32(40, 80, 140, 220), 64, 1.5f);
    dl->AddCircle(center, half * 0.5f, IM_COL32(25, 50,  90, 120), 64, 1.0f);

    // Crosshair
    dl->AddLine({center.x - 12, center.y}, {center.x + 12, center.y},
                IM_COL32(50,110,180,100), 1.0f);
    dl->AddLine({center.x, center.y - 12}, {center.x, center.y + 12},
                IM_COL32(50,110,180,100), 1.0f);

    // Entities
    uintptr_t lp   = ac.localPlayer();
    Vec3 lpPos     = lp ? ac.pos(lp) : Vec3{};
    int  count     = ac.playerCount();

    for (int i = 0; i < count; ++i) {
        uintptr_t ent = ac.entity(i);
        if (!ent || ent == lp) continue;
        Vec3 ep = ac.pos(ent);

        float dx = (ep.x - lpPos.x) / radarScale;
        float dz = (ep.z - lpPos.z) / radarScale;
        float ex = center.x + dx;
        float ey = center.y + dz;

        // Clamp to circle edge
        float cdx = ex - center.x, cdy = ey - center.y;
        float d = std::sqrt(cdx*cdx + cdy*cdy);
        if (d > half - 3.0f) {
            float inv = (half - 3.0f) / d;
            ex = center.x + cdx * inv;
            ey = center.y + cdy * inv;
        }

        int hp = ac.health(ent);
        ImU32 dot = (hp <= 0)
            ? IM_COL32(70, 70, 80, 180)
            : IM_COL32(220, 50, 50, 255);
        dl->AddCircleFilled({ex, ey}, 4.0f, dot);
        dl->AddCircle({ex, ey}, 4.0f, IM_COL32(255, 100, 100, 120));
    }

    // Local player at centre
    dl->AddCircleFilled(center, 5.0f, IM_COL32(60, 220, 100, 255));
    dl->AddCircle(center, 5.0f,       IM_COL32(255,255,255, 160));

    // Legend
    ImGui::Dummy({side, side});
    ImGui::Spacing();
    ImGui::TextDisabled("  Green = you    Red = enemy    Grey = dead");
}

static void tabFeatures(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();

    ImGui::SeparatorText("Loops");
    {
        bool gm = g_fx.godmode.running();
        if (toggle("Godmode  (HP+Armor 200)", gm)) {
            if (gm) g_fx.godmode.start(&g_proc, g_base);
            else    g_fx.godmode.stop();
        }
        bool ia = g_fx.infammo.running();
        if (toggle("Infinite Ammo", ia)) {
            if (ia) g_fx.infammo.start(&g_proc, g_base);
            else    g_fx.infammo.stop();
        }
    }

    ImGui::Spacing(); ImGui::SeparatorText("Aimbot");
    {
        bool abon = g_aimbotCfg.enabled.load();
        if (toggle("Enable Aimbot", abon)) {
            g_aimbotCfg.enabled = abon;
            if (abon && !g_aimbotLoop.running())
                g_aimbotLoop.start(&g_proc, g_base, &g_aimbotCfg);
        }

        int fov = g_aimbotCfg.fovDeg.load();
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderInt("FOV (degrees)", &fov, 5, 180))
            g_aimbotCfg.fovDeg = fov;

        int sm = g_aimbotCfg.smoothPct.load();
        ImGui::SetNextItemWidth(200);
        if (ImGui::SliderInt("Smooth %%", &sm, 0, 95))
            g_aimbotCfg.smoothPct = sm;

        bool ah = g_aimbotCfg.aimHead.load();
        if (ImGui::RadioButton("Aim Head", ah))  g_aimbotCfg.aimHead = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Aim Body", !ah)) g_aimbotCfg.aimHead = false;
    }

    ImGui::Spacing(); ImGui::SeparatorText("Fast Fire");
    if (lp) {
        struct { const char* label; uintptr_t slot; } ff[] = {
            {"AR",      Ent::FF_AR},
            {"Sniper",  Ent::FF_SNIPER},
            {"Shotgun", Ent::FF_SHOTGUN},
        };
        for (auto& f : ff) {
            int cur = 0;
            g_proc.read(lp + f.slot, cur);
            bool on = (cur != 0);
            std::string lbl = std::string("Fast Fire ") + f.label;
            if (toggle(lbl.c_str(), on))
                ac.setFastFire(lp, f.slot, on ? 1 : 0);
        }
        {
            int cur = 0;
            g_proc.read(lp + Ent::AUTO_SHOOT, cur);
            bool on = (cur != 0);
            if (toggle("Auto Shoot", on))
                ac.setAutoShoot(lp, on ? 1 : 0);
        }
    } else {
        ImGui::TextDisabled("(join a match)");
    }

    ImGui::Spacing(); ImGui::SeparatorText("Freeze Entity");
    {
        static int fzIdx = 0;
        ImGui::SetNextItemWidth(70);
        ImGui::InputInt("Index##fz", &fzIdx, 0);
        fzIdx = ImClamp(fzIdx, 0, 7);
        ImGui::SameLine();
        bool fzon = g_fx.freeze[fzIdx].running();
        if (toggle(fzon ? "Frozen##fz" : "Freeze##fz", fzon)) {
            if (fzon && fzIdx < 8) {
                uintptr_t ent = g_base ? ACGame{g_proc, g_base}.entity(fzIdx) : 0;
                if (ent) {
                    Vec3 p = ACGame{g_proc, g_base}.pos(ent);
                    g_fx.freeze[fzIdx].stop();
                    g_fx.freeze[fzIdx].start(&g_proc, g_base, fzIdx, p);
                }
            } else {
                g_fx.freeze[fzIdx].stop();
            }
        }
    }
}

static void tabConfig(const ACGame& ac)
{
    uintptr_t lp = ac.localPlayer();

    ImGui::SeparatorText("Server");
    {
        static int fov = 90;
        static bool fetched = false;
        if (!fetched && g_attached) { fov = ac.fov(); fetched = true; }
        if (!g_attached) fetched = false;
        ImGui::SetNextItemWidth(160);
        if (ImGui::SliderInt("FOV", &fov, 60, 179))
            ac.setFov(fov);
    }

    ImGui::Spacing(); ImGui::SeparatorText("Manual Set (Local Player)");
    if (lp) {
        static int setHp = 100, setArmor = 100;
        ImGui::SetNextItemWidth(100); ImGui::InputInt("HP##s",    &setHp,   0);
        ImGui::SameLine();
        if (ImGui::Button("Set##hp")) ac.setHealth(lp, setHp);

        ImGui::SetNextItemWidth(100); ImGui::InputInt("Armor##s", &setArmor, 0);
        ImGui::SameLine();
        if (ImGui::Button("Set##arm")) ac.setArmor(lp, setArmor);

        static int setAmmo = 999;
        ImGui::SetNextItemWidth(100); ImGui::InputInt("Ammo##s",  &setAmmo,  0);
        ImGui::SameLine();
        if (ImGui::Button("All##ammo")) {
            for (uintptr_t s : {Ent::AMMO_AR, Ent::AMMO_SMG, Ent::AMMO_SNIPER,
                                Ent::AMMO_SHOTGUN, Ent::AMMO_PISTOL,
                                Ent::AMMO_GRENADE, Ent::AMMO_CARBINE})
                ac.setAmmo(lp, s, setAmmo);
        }
    } else {
        ImGui::TextDisabled("(join a match)");
    }

    ImGui::Spacing(); ImGui::SeparatorText("Position Slots");
    if (lp) {
        static int slot = 0;
        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("Slot (0-4)##sl", &slot, 0);
        slot = ImClamp(slot, 0, 4);
        ImGui::SameLine();
        if (ImGui::Button("Save##p")) g_slots[slot] = ac.pos(lp);
        ImGui::SameLine();
        if (ImGui::Button("Load##p") && g_slots[slot])
            ac.setPos(lp, *g_slots[slot]);
    }
    for (int i = 0; i < 5; ++i) {
        ImGui::TextDisabled("  [%d] ", i);
        ImGui::SameLine();
        if (g_slots[i]) ImGui::Text("%.1f, %.1f, %.1f", g_slots[i]->x, g_slots[i]->y, g_slots[i]->z);
        else            ImGui::TextDisabled("(empty)");
    }

    ImGui::Spacing(); ImGui::SeparatorText("Teleport");
    if (lp) {
        static float tx = 0, ty = 0, tz = 0;
        ImGui::SetNextItemWidth(80); ImGui::InputFloat("X##tp", &tx, 0, 0, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80); ImGui::InputFloat("Y##tp", &ty, 0, 0, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80); ImGui::InputFloat("Z##tp", &tz, 0, 0, "%.1f");
        ImGui::SameLine();
        if (ImGui::Button("Go")) ac.setPos(lp, {tx, ty, tz});
        ImGui::SameLine();
        if (ImGui::Button("Copy Current")) {
            Vec3 p = ac.pos(lp);
            tx = p.x; ty = p.y; tz = p.z;
        }
    }

    ImGui::Spacing(); ImGui::SeparatorText("Dump");
    if (ImGui::Button("Dump to ac_dump.txt")) {
        std::ofstream f("ac_dump.txt");
        if (f) {
            f << std::fixed;
            uintptr_t llp = ac.localPlayer();
            if (llp) {
                f << "=== Local Player ===\n"
                  << "Name:  " << ac.name(llp) << "\n"
                  << "HP:    " << ac.health(llp) << "\n"
                  << "Armor: " << ac.armor(llp)  << "\n";
            }
            int cnt = ac.playerCount();
            f << "\n=== All Players ===\n";
            for (int i = 0; i < cnt; ++i) {
                uintptr_t ent = ac.entity(i);
                if (!ent) continue;
                Vec3 ep = ac.pos(ent);
                f << "[" << i << "] " << ac.name(ent)
                  << "  HP=" << ac.health(ent)
                  << "  Armor=" << ac.armor(ent)
                  << "  Pos=" << ep.x << "," << ep.y << "," << ep.z << "\n";
            }
        }
    }
}

// ── Main UI frame ────────────────────────────────────────────────────────────

static void renderFrame()
{
    static float autoRetryTimer = 0.0f;
    autoRetryTimer += ImGui::GetIO().DeltaTime;
    if (!g_attached && autoRetryTimer > 2.0f) {
        tryAttach();
        autoRetryTimer = 0.0f;
    }

    // Full-screen dockable window
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##root", nullptr, wf);

    // ── Header bar ───────────────────────────────────────────────────────────
    if (g_attached) {
        badge("● Attached", {0.22f, 0.78f, 0.38f, 1.0f});
        ImGui::SameLine();
        ImGui::TextDisabled("pid %lu  |  base 0x%llX", g_proc.pid, (unsigned long long)g_base);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::SmallButton("Detach")) doDetach();
    } else {
        badge("○ Detached", {0.70f, 0.30f, 0.30f, 1.0f});
        ImGui::SameLine();
        ImGui::TextDisabled("waiting for ac_client.exe …");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::SmallButton("Attach")) tryAttach();
    }

    // Active loops strip
    auto loopBadge = [](const char* label, bool on) {
        if (!on) return;
        ImGui::SameLine(0, 6);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.22f,0.78f,0.38f,1});
        ImGui::SmallButton(label);
        ImGui::PopStyleColor();
    };
    loopBadge("GODMODE", g_fx.godmode.running());
    loopBadge("INF-AMMO", g_fx.infammo.running());
    loopBadge("AIMBOT", g_aimbotLoop.running() && g_aimbotCfg.enabled.load());
    for (int i = 0; i < 8; ++i)
        if (g_fx.freeze[i].running()) {
            char buf[16]; snprintf(buf, sizeof(buf), "FRZ[%d]", i);
            loopBadge(buf, true);
        }

    ImGui::Separator();

    // ── Tabs ──────────────────────────────────────────────────────────────
    if (!g_attached) {
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x * 0.5f - 100);
        ImGui::TextDisabled("Start ac_client.exe, then click Attach.");
        ImGui::End();
        return;
    }

    ACGame ac { g_proc, g_base };

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Status"))   { tabStatus(ac);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Players"))  { tabPlayers(ac);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Radar"))    { tabRadar(ac);    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Features")) { tabFeatures(ac); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Config"))   { tabConfig(ac);   ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── D3D11 setup ──────────────────────────────────────────────────────────────

static bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount       = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = hWnd;
    sd.SampleDesc.Count  = 1;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed          = TRUE;

    D3D_FEATURE_LEVEL lvl;
    const D3D_FEATURE_LEVEL lvls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, lvls, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &lvl, &g_ctx);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}

static void CreateRenderTarget()
{
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

static void CleanupRenderTarget()
{
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_swap)   { g_swap->Release();   g_swap   = nullptr; }
    if (g_ctx)    { g_ctx->Release();    g_ctx    = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

// ── WinMain ──────────────────────────────────────────────────────────────────

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                       hInstance, nullptr, nullptr, nullptr, nullptr,
                       L"ACTool", nullptr };
    RegisterClassExW(&wc);
    HWND hWnd = CreateWindowW(wc.lpszClassName,
        L"AssaultCube Tool",
        WS_OVERLAPPEDWINDOW, 100, 100, 880, 580,
        nullptr, nullptr, hInstance, nullptr);

    if (!CreateDeviceD3D(hWnd)) { UnregisterClassW(wc.lpszClassName, hInstance); return 1; }

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't write imgui.ini

    applyStyle();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    // Auto-attach on startup
    tryAttach();

    const ImVec4 clearColor = { 0.055f, 0.063f, 0.075f, 1.0f };

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderFrame();

        ImGui::Render();
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv,
            reinterpret_cast<const float*>(&clearColor));
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }

    g_fx.stopAll();
    g_aimbotLoop.stop();
    if (g_attached) doDetach();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hWnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}
