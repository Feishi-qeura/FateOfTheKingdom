#include "GridInfo_Editor.h"

UGridInfo_Editor::UGridInfo_Editor()
{
	bShowNormal = false;
	bSensing = false;
}

bool UGridInfo_Editor::GetVisibility() const
{
	return bShowNormal || bSensing;
}

bool UGridInfo_Editor::GetShowNormal() const
{
	return bShowNormal;
}
void UGridInfo_Editor::SetShowNormal(bool bNewShowNormal)
{
	bShowNormal = bNewShowNormal;

	Dirty();
}
bool UGridInfo_Editor::GetSensing() const
{
	return bSensing;
}
void UGridInfo_Editor::SetSensing(bool bNewSensing)
{
	bSensing = bNewSensing;

	Dirty();
}

