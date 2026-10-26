// Source Repository: https://github.com/waverso/waversomapstudio

#pragma once

#include <cstdint>

struct tagPOINT;
using POINT = tagPOINT;

void GuiDeleteSceneSelection();
void GuiClearSceneSelection();
void GuiOnItemsAppended(int firstIdx, int endIdx);
void GuiRequestOutlinerScrollTo(int itemId);
void GuiSelectAllOutliner();
bool GuiCanSelectParentOutliner();
void GuiSelectParentOutliner();
void GuiCloneUnifiedSelection();
void GuiShowToast(const char* msg, bool error);
void GuiSyncMousePosition(const POINT& screen);
bool GuiIsAnyModalOpen();
void GuiRequestBookmarkFocus(std::uint64_t id);
void GuiResetProjectState();
void GuiOnSceneRestored(int oldCount) noexcept;
bool GuiFinishPropertyEdit();
bool GuiCanCloneUnifiedSelection();
bool GuiIsTransformEditing(int itemIndex);
bool GuiIsAreaEditing(int itemIndex);
int GuiGetBoxAreaEditingFace(int itemIndex);
