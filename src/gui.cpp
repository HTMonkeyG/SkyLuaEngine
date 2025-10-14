#include "includes/htmod.h"
#include "imgui.h"
#include "TextEditor.h"
#include "aliases.h"
#include "skylua.h"

i08 gUseLocalEngine = 0;

static HTHandle hKeyMenuToggle
  , hKeyExecuteScript;
static bool gShowMenu = false
  , gEditorInit = false;
static ImTextEditor gEditor;

static void toggleMenu(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    gShowMenu = !gShowMenu;
}

static void executeScript(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    queueEval(gEditor.GetText().c_str());
}

static void initTextEditor() {
  auto lang = ImTextEditor::LanguageDefinition::Lua();
  static const char *skyGameDefs[] = {
    "game", "LuaLog", "LoadLevel", "Kill", "HurtLegL", "sle"
  };
  static const char *skyGameDefDesc[] = {
    "Game status",
    "Print the log to Sky.log",
    "Load specified level",
    "Terminate the game process",
    "Reduce cape energy",
    "Namespace for SkyLuaEngine"
  };
  ImTextEditor::Palette palette = ImTextEditor::GetDarkPalette();

  // Set game-related definitions.
  for (u32 i = 0; i < sizeof(skyGameDefs) / sizeof(skyGameDefs[0]); ++i) {
	  ImTextEditor::Identifier id;
	  id.mDeclaration = skyGameDefDesc[i];
	  lang.mIdentifiers.insert(std::make_pair(std::string(skyGameDefs[i]), id));
  }
  gEditor.SetLanguageDefinition(lang);

  // Set the same colors as HTML.
  palette[(int)ImTextEditor::PaletteIndex::Background] = ImGui::GetColorU32(ImGuiCol_WindowBg);
  gEditor.SetPalette(palette);

  gEditor.SetTabSize(2);
  gEditor.SetFontSize(26);
}

__declspec(dllexport) void HTMLAPI HTModRenderGui(
  float timeElapesed,
  void *reserved
) {
  if (!gShowMenu)
    return;
  if (!gEditorInit) {
    // We should not put this initialization in HTModOnInit() when using HTML
    // SDK version >= 1.4.0, because the ImGui isn't initialized when the mod
    // is loaded.
    // For a better compatibility, we put this in HTModRenderGui().
    gEditorInit = true;
    initTextEditor();
  }

  ImGui::PushID("sky-lua");

  if (!ImGui::Begin("Sky Lua Engine", &gShowMenu))
    return ImGui::End(), ImGui::PopID();

  ImGui::Checkbox("Use local engine", (bool *)&gUseLocalEngine);

  if (ImGui::Button("Run", ImVec2(-FLT_MIN, 0)))
    queueEval(gEditor.GetText().c_str());

  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
  gEditor.Render("ImTextEditor");
  ImGui::PopStyleVar();

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
