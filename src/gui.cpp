#include <cstdarg>
#include <cstdio>
#include <cfloat>
#include <string>
#include <vector>
#include <memory>

#include "includes/htmodloader.h"
#include "imgui.h"
#include "TextEditor.h"
#include "aliases.h"
#include "skylua.h"

i08 gUseLocalEngine = 0;

static HTHandle hKeyMenuToggle
  , hKeyExecuteScript;
static bool gShowMenu = false;

// ---------------------------------------------------------------------------
// Editor tabs
// ---------------------------------------------------------------------------

struct EditorTab {
  std::unique_ptr<ImTextEditor> editor;
  std::string relPath;        // Relative to <ModFolder>/scripts, empty if new.
  std::string title;          // Name shown on the tab.
  int id = 0;                 // Stable ImGui ID.
  bool modified = false;
  bool selectRequested = false;
};

static std::vector<std::unique_ptr<EditorTab>> gTabs;
static int gNextTabId = 1;
static int gNextUntitled = 1;
static int gActiveTab = -1;

struct ScriptEntry {
  std::string relPath;
};

static std::vector<ScriptEntry> gScriptFiles;
static bool gNeedFileRefresh = true;
static std::string gSelectedFile;

static char gStatus[256] = "";

static char gSaveName[260] = "";
static bool gOpenSavePopup = false;
static EditorTab *gSaveTarget = nullptr;

enum PendingAction {
  ACT_NONE,
  ACT_SAVE,
  ACT_SAVE_AS,
  ACT_RUN,
  ACT_CLOSE
};

static PendingAction gPendingAction = ACT_NONE;

static void setStatus(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vsnprintf(gStatus, sizeof(gStatus), fmt, args);
  va_end(args);
}

static std::string fileNameOf(const std::string &path) {
  size_t pos = path.find_last_of("/\\");

  if (pos == std::string::npos)
    return path;
  return path.substr(pos + 1);
}

static bool hasLuaExtension(const std::string &name) {
  if (name.size() < 4)
    return false;

  std::string ext = name.substr(name.size() - 4);
  for (char &c : ext) {
    if (c >= 'A' && c <= 'Z')
      c = (char)(c - 'A' + 'a');
  }

  return ext == ".lua";
}

// Compare script paths like Windows does, ignoring ASCII case.
static bool sameScriptPath(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;

  for (size_t i = 0; i < a.size(); ++i) {
    char ca = a[i];
    char cb = b[i];

    if (ca >= 'A' && ca <= 'Z')
      ca = (char)(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z')
      cb = (char)(cb - 'A' + 'a');

    if (ca != cb)
      return false;
  }

  return true;
}

// Create an editor with the Sky-flavoured Lua language definition.
static std::unique_ptr<ImTextEditor> createEditor(const char *text) {
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

  auto editor = std::make_unique<ImTextEditor>();
  ImTextEditor::LanguageDefinition lang = ImTextEditor::LanguageDefinition::Lua();

  for (u32 i = 0; i < sizeof(skyGameDefs) / sizeof(skyGameDefs[0]); ++i) {
    ImTextEditor::Identifier id;
    id.mDeclaration = skyGameDefDesc[i];
    lang.mIdentifiers[skyGameDefs[i]] = id;
  }

  editor->SetLanguageDefinition(lang);

  // Match the background of the host window.
  ImTextEditor::Palette palette = ImTextEditor::GetDarkPalette();
  palette[(int)ImTextEditor::PaletteIndex::Background] =
    ImGui::GetColorU32(ImGuiCol_WindowBg);
  editor->SetPalette(palette);

  editor->SetTabSize(2);
  editor->SetFontSize(26);

  if (text && text[0])
    editor->SetText(text);

  return editor;
}

static EditorTab *createTab(const std::string &text) {
  auto tab = std::make_unique<EditorTab>();

  tab->editor = createEditor(text.c_str());
  tab->id = gNextTabId++;

  gTabs.push_back(std::move(tab));

  return gTabs.back().get();
}

static int findTabByPath(const std::string &relPath) {
  for (int i = 0; i < (int)gTabs.size(); ++i) {
    if (!gTabs[i]->relPath.empty()
        && sameScriptPath(gTabs[i]->relPath, relPath))
      return i;
  }

  return -1;
}

static void closeTab(int index) {
  if (index < 0 || index >= (int)gTabs.size())
    return;

  if (gTabs[index].get() == gSaveTarget)
    gSaveTarget = nullptr;

  gTabs.erase(gTabs.begin() + index);

  if (gActiveTab == index)
    gActiveTab = gTabs.empty() ? -1 : (index > 0 ? index - 1 : 0);
  else if (gActiveTab > index)
    --gActiveTab;
}

static void newFile() {
  char title[64];
  EditorTab *tab;

  snprintf(title, sizeof(title), "untitled-%d.lua", gNextUntitled++);

  tab = createTab("");
  tab->relPath.clear();
  tab->title = title;
  tab->selectRequested = true;

  setStatus("New script created.");
}

static void openFile(const std::string &relPath) {
  int existing;
  char *data = nullptr;
  EditorTab *tab;

  existing = findTabByPath(relPath);
  if (existing >= 0) {
    gTabs[existing]->selectRequested = true;
    gSelectedFile = relPath;
    return;
  }

  if (!sleReadScriptFile(relPath.c_str(), &data, nullptr)) {
    setStatus("Failed to open %s", relPath.c_str());
    return;
  }

  tab = createTab(data ? data : "");
  free(data);

  tab->relPath = relPath;
  tab->title = fileNameOf(relPath);
  tab->selectRequested = true;
  gSelectedFile = relPath;

  setStatus("Opened %s", relPath.c_str());
}

static bool saveTab(EditorTab *tab, const char *name) {
  std::string rel = name;
  std::string content;

  if (!tab || !name || !name[0])
    return false;

  // Normalize separators and add the Lua extension if necessary.
  for (char &c : rel) {
    if (c == '\\')
      c = '/';
  }

  if (!hasLuaExtension(rel))
    rel += ".lua";

  // Do not let two tabs edit the same file at once.
  for (const auto &other : gTabs) {
    if (other.get() != tab && sameScriptPath(other->relPath, rel)) {
      setStatus("File %s is already open in another tab.", rel.c_str());
      return false;
    }
  }

  content = tab->editor->GetText();

  if (!sleWriteScriptFile(rel.c_str(), content.c_str(), (u64)content.size())) {
    setStatus("Failed to save %s", rel.c_str());
    return false;
  }

  tab->relPath = rel;
  tab->title = fileNameOf(rel);
  tab->modified = false;
  gNeedFileRefresh = true;
  gSelectedFile = rel;

  setStatus("Saved %s", rel.c_str());

  return true;
}

static void requestSave(EditorTab *tab, bool saveAs) {
  if (!tab)
    return;

  if (!saveAs && !tab->relPath.empty()) {
    saveTab(tab, tab->relPath.c_str());
    return;
  }

  gSaveTarget = tab;
  snprintf(
    gSaveName, sizeof(gSaveName), "%s",
    !tab->relPath.empty() ? tab->relPath.c_str() : tab->title.c_str());
  gOpenSavePopup = true;
}

static void runTab(EditorTab *tab) {
  std::string text;

  if (!tab)
    return;

  text = tab->editor->GetText();

  if (!sleQueueEval(text.c_str())) {
    setStatus("Failed to queue %s.", tab->title.c_str());
    return;
  }

  if (!gGameLuaState)
    setStatus("Queued %s (waiting for the game Lua state).", tab->title.c_str());
  else
    setStatus("Queued %s", tab->title.c_str());
}

static void refreshScriptFiles() {
  char **names = nullptr;
  u32 count = sleListScriptFiles(&names);
  bool selectedStillExists = false;

  gScriptFiles.clear();

  for (u32 i = 0; i < count; ++i) {
    ScriptEntry entry;

    entry.relPath = names && names[i] ? names[i] : "";
    gScriptFiles.push_back(std::move(entry));
  }

  sleFreeScriptFiles(names, count);
  gNeedFileRefresh = false;

  if (gSelectedFile.empty())
    return;

  for (const ScriptEntry &entry : gScriptFiles) {
    if (sameScriptPath(entry.relPath, gSelectedFile)) {
      selectedStillExists = true;
      break;
    }
  }

  if (!selectedStillExists)
    gSelectedFile.clear();
}

static void processPendingAction() {
  PendingAction action = gPendingAction;
  EditorTab *tab;

  gPendingAction = ACT_NONE;

  if (action == ACT_NONE)
    return;
  if (gActiveTab < 0 || gActiveTab >= (int)gTabs.size())
    return;

  tab = gTabs[gActiveTab].get();

  switch (action) {
    case ACT_SAVE:
      requestSave(tab, false);
      break;
    case ACT_SAVE_AS:
      requestSave(tab, true);
      break;
    case ACT_RUN:
      runTab(tab);
      break;
    case ACT_CLOSE:
      closeTab(gActiveTab);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Hotkeys
// ---------------------------------------------------------------------------

static void toggleMenu(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    gShowMenu = !gShowMenu;
}

static void executeScript(HTKeyEvent *e) {
  if ((e->flags & HTKeyEventFlags_Mask) != HTKeyEventFlags_Down)
    return;
  if (gActiveTab < 0 || gActiveTab >= (int)gTabs.size())
    return;

  runTab(gTabs[gActiveTab].get());
}

// ---------------------------------------------------------------------------
// Editor page
// ---------------------------------------------------------------------------

static void renderFilePanel() {
  if (gNeedFileRefresh)
    refreshScriptFiles();

  ImGui::TextUnformatted("Scripts");

  if (ImGui::IsItemHovered()) {
    const char *path = sleGetScriptsPathUtf8();
    if (path && path[0])
      ImGui::SetTooltip("%s", path);
  }

  ImGui::Separator();

  if (ImGui::Button("New", ImVec2(-FLT_MIN, 0)))
    newFile();

  ImGui::Spacing();

  ImGui::BeginChild("##fileList", ImVec2(0, 0), ImGuiChildFlags_None);

  for (const ScriptEntry &entry : gScriptFiles) {
    bool selected = sameScriptPath(entry.relPath, gSelectedFile);

    if (ImGui::Selectable(entry.relPath.c_str(), selected)) {
      gSelectedFile = entry.relPath;
      openFile(entry.relPath);
    }
  }

  if (gScriptFiles.empty())
    ImGui::TextDisabled("No .lua files");

  ImGui::EndChild();
}

static void renderEditorTabs() {
  ImVec2 size = ImGui::GetContentRegionAvail();
  int closeIndex = -1;

  ImGui::BeginChild("##editors", size, ImGuiChildFlags_None);

  if (ImGui::BeginTabBar(
      "##EditorTabBar",
      ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
    for (int i = 0; i < (int)gTabs.size(); ++i) {
      EditorTab *tab = gTabs[i].get();
      char label[512];
      bool open = true;
      ImGuiTabItemFlags flags = 0;

      snprintf(
        label, sizeof(label), "%s%s###tab%d",
        tab->title.c_str(),
        tab->modified ? " *" : "",
        tab->id);

      if (tab->selectRequested)
        flags |= ImGuiTabItemFlags_SetSelected;

      if (ImGui::BeginTabItem(label, &open, flags)) {
        tab->selectRequested = false;
        gActiveTab = i;

        ImGui::PushID(tab->id);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        tab->editor->Render("##editor", ImGui::GetContentRegionAvail());
        ImGui::PopStyleVar();
        ImGui::PopID();

        // Escape releases keyboard focus from the editor so that hotkeys and
        // other panels can be used again without clicking elsewhere first.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_Escape))
          ImGui::SetWindowFocus(nullptr);

        if (tab->editor->IsTextChanged())
          tab->modified = true;

        ImGui::EndTabItem();
      }

      if (!open)
        closeIndex = i;
    }

    ImGui::EndTabBar();
  }

  if (gTabs.empty()) {
    ImGui::TextDisabled(
      "Create a new script or open one from the list on the left.");
  }

  ImGui::EndChild();

  if (closeIndex >= 0)
    closeTab(closeIndex);
}

static void renderEditorPage() {
  if (ImGui::Button("New"))
    newFile();
  ImGui::SameLine();
  if (ImGui::Button("Save"))
    gPendingAction = ACT_SAVE;
  ImGui::SameLine();
  if (ImGui::Button("Run"))
    gPendingAction = ACT_RUN;
  ImGui::SameLine();
  ImGui::Checkbox("Use local engine", (bool *)&gUseLocalEngine);
  ImGui::SameLine();
  if (ImGui::Button("Refresh"))
    gNeedFileRefresh = true;

  ImGui::Separator();

  ImGui::BeginChild(
    "##filePanel",
    ImVec2(220.0f, 0.0f),
    ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  renderFilePanel();
  ImGui::EndChild();

  ImGui::SameLine();
  renderEditorTabs();
}

// ---------------------------------------------------------------------------
// REPL page
// ---------------------------------------------------------------------------

static char gReplInput[4096] = "";
static std::vector<std::string> gReplHistory;
static int gReplHistoryIndex = -1;
static u64 gReplSeenVersion = 0;
static bool gReplKeepFocus = false;

static int replInputCallback(ImGuiInputTextCallbackData *data) {
  int count = (int)gReplHistory.size();

  if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory || count == 0)
    return 0;

  if (data->EventKey == ImGuiKey_UpArrow) {
    if (gReplHistoryIndex < 0)
      gReplHistoryIndex = count - 1;
    else if (gReplHistoryIndex > 0)
      --gReplHistoryIndex;
  } else if (data->EventKey == ImGuiKey_DownArrow) {
    if (gReplHistoryIndex >= 0) {
      ++gReplHistoryIndex;
      if (gReplHistoryIndex >= count)
        gReplHistoryIndex = -1;
    }
  } else {
    return 0;
  }

  data->DeleteChars(0, data->BufTextLen);

  if (gReplHistoryIndex >= 0 && gReplHistoryIndex < count)
    data->InsertChars(0, gReplHistory[gReplHistoryIndex].c_str());

  return 0;
}

static void submitRepl() {
  std::string code;

  if (!gReplInput[0])
    return;

  code = gReplInput;
  gReplInput[0] = 0;

  gReplHistory.push_back(code);
  gReplHistoryIndex = -1;

  sleReplLog("> %s", code.c_str());

  if (!sleQueueRepl(code.c_str()))
    sleReplLog("[error] The game Lua state is not ready.");

  gReplKeepFocus = true;
}

static void renderReplPage() {
  float inputHeight;
  ImVec2 avail;
  float outputHeight;
  const char *log;

  if (ImGui::Button("Clear")) {
    sleReplClear();
    gReplSeenVersion = sleReplGetVersion();
  }

  ImGui::SameLine();
  ImGui::TextDisabled("print() and sle.log() output appears here.");

  ImGui::SameLine();
  ImGui::TextDisabled(
    "| Lua state: %s", gGameLuaState ? "ready" : "waiting");

  ImGui::Separator();

  inputHeight = ImGui::GetFrameHeightWithSpacing();
  avail = ImGui::GetContentRegionAvail();
  outputHeight = avail.y - inputHeight - ImGui::GetStyle().ItemSpacing.y;
  if (outputHeight < 60.0f)
    outputHeight = 60.0f;

  ImGui::BeginChild(
    "##replOutput", ImVec2(0.0f, outputHeight), ImGuiChildFlags_Borders);

  log = sleReplGetText();
  if (log && log[0])
    ImGui::TextUnformatted(log);
  else
    ImGui::TextDisabled("(no output yet)");

  if (sleReplGetVersion() != gReplSeenVersion) {
    gReplSeenVersion = sleReplGetVersion();
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();

  ImGui::TextUnformatted(">");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-FLT_MIN);

  if (ImGui::InputText(
      "##replInput",
      gReplInput,
      sizeof(gReplInput),
      ImGuiInputTextFlags_EnterReturnsTrue
        | ImGuiInputTextFlags_CallbackHistory,
      replInputCallback))
    submitRepl();

  if (gReplKeepFocus) {
    gReplKeepFocus = false;
    ImGui::SetKeyboardFocusHere(-1);
  }
}

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------

static void showMenuBar() {
  if (!ImGui::BeginMenuBar())
    return;

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("New", "Ctrl+N"))
      newFile();
    if (ImGui::MenuItem("Save", "Ctrl+S"))
      gPendingAction = ACT_SAVE;
    if (ImGui::MenuItem("Save As..."))
      gPendingAction = ACT_SAVE_AS;
    ImGui::Separator();
    if (ImGui::MenuItem("Close Tab"))
      gPendingAction = ACT_CLOSE;
    if (ImGui::MenuItem("Refresh Scripts"))
      gNeedFileRefresh = true;
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Script")) {
    if (ImGui::MenuItem("Run", "F9"))
      gPendingAction = ACT_RUN;
    ImGui::MenuItem(
      "Use Local Engine", nullptr, (bool *)&gUseLocalEngine);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("REPL")) {
    if (ImGui::MenuItem("Clear Output"))
      sleReplClear();
    ImGui::EndMenu();
  }

  // Status message on the right side of the menu bar.
  if (gStatus[0]) {
    float textWidth = ImGui::CalcTextSize(gStatus).x;
    float maxX = ImGui::GetWindowContentRegionMax().x;

    if (textWidth + 16.0f < maxX) {
      ImGui::SameLine(maxX - textWidth);
      ImGui::TextDisabled("%s", gStatus);
    }
  }

  ImGui::EndMenuBar();
}

static void renderSavePopup() {
  if (gOpenSavePopup) {
    ImGui::OpenPopup("Save Script");
    gOpenSavePopup = false;
  }

  if (!ImGui::BeginPopupModal(
      "Save Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;

  ImGui::TextUnformatted("File name inside the scripts folder:");
  ImGui::SetNextItemWidth(360.0f);

  {
    bool submitted = ImGui::InputText(
      "##saveName",
      gSaveName,
      sizeof(gSaveName),
      ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::TextDisabled("The .lua extension is added automatically.");

    if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)) || submitted) {
      if (gSaveTarget && saveTab(gSaveTarget, gSaveName)) {
        gSaveTarget = nullptr;
        ImGui::CloseCurrentPopup();
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
    gSaveTarget = nullptr;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

i32 sleInitGui() {
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

void sleRenderGui() {
  bool windowFocused;

  if (!gShowMenu)
    return;

  ImGui::PushID("sky-lua");

  ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
    ImVec2(640.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));

  if (ImGui::Begin(
      "Sky Lua Engine", &gShowMenu, ImGuiWindowFlags_MenuBar)) {
    windowFocused = ImGui::IsWindowFocused(
      ImGuiFocusedFlags_RootAndChildWindows);

    if (windowFocused && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N))
      newFile();
    if (windowFocused && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
      gPendingAction = ACT_SAVE;

    showMenuBar();
    processPendingAction();

    if (ImGui::BeginTabBar("##MainTabs", ImGuiTabBarFlags_None)) {
      if (ImGui::BeginTabItem("Editor")) {
        renderEditorPage();
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("REPL")) {
        renderReplPage();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    renderSavePopup();
  }

  ImGui::End();
  ImGui::PopID();
}
