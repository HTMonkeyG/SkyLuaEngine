#include "includes/htmod.h"
#include "imgui.h"
#include "aliases.h"
#include "skylua.h"

i08 gUseLocalEngine = 0;

static HTHandle hKeyMenuToggle
  , hKeyExecuteScript;
static bool gShowMenu = false;
static char gInputBuffer[100000] = {0};

static void toggleMenu(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    gShowMenu = !gShowMenu;
}

static void executeScript(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    queueEval(gInputBuffer);
}

__declspec(dllexport) void HTMLAPI HTModRenderGui(
  float timeElapesed,
  void *reserved
) {
  if (!gShowMenu)
    return;
  
  ImGui::PushID("sky-lua");

  if (!ImGui::Begin("Sky Lua Engine", &gShowMenu))
    return ImGui::End(), ImGui::PopID();
  
  ImGui::Checkbox("Use local engine", (bool *)&gUseLocalEngine);

  if (ImGui::Button("Run", ImVec2(-FLT_MIN, 0)))
    queueEval(gInputBuffer);

  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputTextMultiline(
    "##Input",
    gInputBuffer,
    sizeof(gInputBuffer));

  ImGui::End();
  ImGui::PopID();
}

i32 initGui() {
  hKeyMenuToggle = HTHotkeyRegister(
    hModuleDll,
    "Toggle menu",
    HTKey_F8);
  hKeyExecuteScript = HTHotkeyRegister(
    hModuleDll,
    "Execute script",
    HTKey_F9);

  HTHotkeyListen(hKeyMenuToggle, toggleMenu);
  HTHotkeyListen(hKeyExecuteScript, executeScript);

  return 1;
}
