/**
 * @file SyncStateIcons.cpp
 * @brief Implementation of icon management for OneDrive sync states
 * @author Haiku OneDrive Team
 * @version 1.0.0
 * @date 2025-08-15
 */

#include "SyncStateIcons.h"

#include <Autolock.h>
#include "../shared/ColorConstants.h"
#include "../shared/FileSystemConstants.h"

using namespace OneDrive;
using namespace OneDrive::Colors;
using namespace OneDrive::FileSystem;
#include <Bitmap.h>
#include <Entry.h>
#include <File.h>
#include <IconUtils.h>
#include <InterfaceDefs.h>
#include <Node.h>
#include <NodeInfo.h>
#include <View.h>
#include <fs_attr.h>

#include <stdio.h>
#include <string.h>

// Icon attribute names
static const char* kVectorIconAttr = "BEOS:ICON";
static const char* kLargeIconAttr = "BEOS:L:ICON";
static const char* kMiniIconAttr = "BEOS:M:ICON";

// Using colors from ColorConstants.h

// Singleton instance
SyncStateIcons* SyncStateIcons::sInstance = NULL;

/**
 * @brief Get the singleton instance
 */
SyncStateIcons&
SyncStateIcons::Instance()
{
    if (!sInstance) {
        sInstance = new SyncStateIcons();
    }
    return *sInstance;
}

/**
 * @brief Private constructor
 */
SyncStateIcons::SyncStateIcons()
    : fInitialized(false)
{
}

/**
 * @brief Destructor
 */
SyncStateIcons::~SyncStateIcons()
{
    ClearCache();
}

/**
 * @brief Initialize the icon system
 */
status_t
SyncStateIcons::Initialize()
{
    if (fInitialized) {
        return B_OK;
    }
    
    // Pre-create commonly used overlays
    _CreateOverlayBitmap(kOverlaySynced, B_MINI_ICON);
    _CreateOverlayBitmap(kOverlaySynced, B_LARGE_ICON);
    _CreateOverlayBitmap(kOverlaySyncing, B_MINI_ICON);
    _CreateOverlayBitmap(kOverlaySyncing, B_LARGE_ICON);
    
    fInitialized = true;
    return B_OK;
}

/**
 * @brief Get icon overlay for a sync state
 */
IconOverlayType
SyncStateIcons::GetOverlayForState(OneDriveSyncState state) const
{
    switch (state) {
        case kSyncStateSynced:
            return kOverlaySynced;
        case kSyncStateSyncing:
            return kOverlaySyncing;
        case kSyncStateError:
            return kOverlayError;
        case kSyncStatePending:
            return kOverlayPending;
        case kSyncStateConflict:
            return kOverlayConflict;
        case kSyncStateOffline:
            return kOverlayOffline;
        case kSyncStateOnlineOnly:
            return kOverlayOnlineOnly;
        default:
            return kOverlayNone;
    }
}

/**
 * @brief Apply overlay to an icon
 */
status_t
SyncStateIcons::ApplyOverlay(const BBitmap* baseIcon, IconOverlayType overlay,
                             BBitmap** result)
{
    if (!baseIcon || !result || overlay == kOverlayNone) {
        return B_BAD_VALUE;
    }
    
    // Get overlay bitmap
    icon_size size = (baseIcon->Bounds().Width() > 16) ? B_LARGE_ICON : B_MINI_ICON;
    BBitmap* overlayBitmap = GetCachedOverlay(overlay, size);
    if (!overlayBitmap) {
        overlayBitmap = _CreateOverlayBitmap(overlay, size);
        if (!overlayBitmap) {
            return B_NO_MEMORY;
        }
    }
    
    // Composite the icons
    *result = _CompositeIcons(baseIcon, overlayBitmap);
    return (*result) ? B_OK : B_NO_MEMORY;
}

/**
 * @brief Update file/folder icon with sync state
 */
status_t
SyncStateIcons::UpdateFileIcon(const entry_ref& ref, OneDriveSyncState state)
{
    BNode node(&ref);
    status_t result = node.InitCheck();
    if (result != B_OK) {
        return result;
    }
    
    // Get the current icon
    BNodeInfo nodeInfo(&node);
    BBitmap* icon = new BBitmap(BRect(0, 0, 31, 31), B_RGBA32);
    result = nodeInfo.GetIcon(icon, B_LARGE_ICON);
    
    if (result != B_OK) {
        // Try to get vector icon
        result = BIconUtils::GetVectorIcon(&node, kVectorIconAttr, icon);
        if (result != B_OK) {
            delete icon;
            return result;
        }
    }
    
    // Apply overlay
    IconOverlayType overlay = GetOverlayForState(state);
    if (overlay != kOverlayNone) {
        BBitmap* newIcon = NULL;
        result = ApplyOverlay(icon, overlay, &newIcon);
        if (result == B_OK && newIcon) {
            // Set the new icon
            nodeInfo.SetIcon(newIcon, B_LARGE_ICON);
            delete newIcon;
        }
    }
    
    delete icon;
    return B_OK;
}

/**
 * @brief Create vector icon data for overlay
 */
ssize_t
SyncStateIcons::CreateOverlayData(IconOverlayType overlay, uint8* data,
                                  size_t size)
{
    // In production, this would generate actual HVIF data
    // For now, return a placeholder
    if (!data || size < 10) {
        return B_BAD_VALUE;
    }
    
    // Placeholder HVIF header
    data[0] = 'n';  // HVIF magic
    data[1] = 'c';
    data[2] = 'i';
    data[3] = 'f';
    data[4] = 1;    // Version
    
    return 5;
}

/**
 * @brief Get cached overlay bitmap
 */
BBitmap*
SyncStateIcons::GetCachedOverlay(IconOverlayType overlay, icon_size size)
{
    BAutolock lock(fCacheLock);
    
    // Simple cache lookup (in production, use proper cache structure)
    int32 index = (overlay * 2) + (size == B_LARGE_ICON ? 1 : 0);
    if (index < fOverlayCache.CountItems()) {
        return static_cast<BBitmap*>(fOverlayCache.ItemAt(index));
    }
    
    return NULL;
}

/**
 * @brief Clear icon cache
 */
void
SyncStateIcons::ClearCache()
{
    BAutolock lock(fCacheLock);
    
    for (int32 i = 0; i < fOverlayCache.CountItems(); i++) {
        BBitmap* bitmap = static_cast<BBitmap*>(fOverlayCache.ItemAt(i));
        delete bitmap;
    }
    fOverlayCache.MakeEmpty();
}

/**
 * @brief Create overlay bitmap
 */
BBitmap*
SyncStateIcons::_CreateOverlayBitmap(IconOverlayType overlay, icon_size size)
{
    int32 dimension = (size == B_LARGE_ICON) ? 32 : 16;
    int32 overlaySize = dimension * kOverlaySizeRatio;
    
    BBitmap* bitmap = new BBitmap(BRect(0, 0, overlaySize - 1, overlaySize - 1),
                                   B_RGBA32, true);
    if (!bitmap || bitmap->InitCheck() != B_OK) {
        delete bitmap;
        return NULL;
    }
    
    // Create a view to draw into the bitmap
    BView* view = new BView(bitmap->Bounds(), "overlay", B_FOLLOW_NONE, 0);
    bitmap->AddChild(view);
    bitmap->Lock();
    
    // Set up drawing environment
    view->SetHighColor(B_TRANSPARENT_COLOR);
    view->FillRect(view->Bounds());
    
    // Draw the appropriate overlay
    switch (overlay) {
        case kOverlaySynced:
            _DrawCheckmark(bitmap, SyncState::kSynced);
            break;
        case kOverlaySyncing:
            _DrawSyncArrows(bitmap, SyncState::kSyncing);
            break;
        case kOverlayError:
            _DrawErrorX(bitmap, SyncState::kError);
            break;
        case kOverlayPending:
            _DrawClock(bitmap, SyncState::kPending);
            break;
        case kOverlayConflict:
            _DrawWarning(bitmap, SyncState::kConflict);
            break;
        case kOverlayOffline:
            _DrawPin(bitmap, SyncState::kOffline);
            break;
        case kOverlayOnlineOnly:
            _DrawCloud(bitmap, SyncState::kOnlineOnly);
            break;
        default:
            break;
    }
    
    view->Sync();
    bitmap->Unlock();
    bitmap->RemoveChild(view);
    delete view;
    
    // Cache the bitmap
    BAutolock lock(fCacheLock);
    fOverlayCache.AddItem(bitmap);
    
    return bitmap;
}

/**
 * @brief Draw checkmark overlay
 */
void
SyncStateIcons::_DrawCheckmark(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw circle background
    view->SetHighColor(UI::kBackground);
    view->FillEllipse(bounds);
    
    // Draw checkmark
    view->SetHighColor(color);
    view->SetPenSize(size * IconMetrics::kPenSizeThick);
    view->StrokeArc(bounds, 45, 180);
    
    // Simple checkmark path
    BPoint start(size * IconMetrics::kCheckmarkStart, size * 0.5);
    BPoint mid(size * IconMetrics::kCheckmarkMid, size * 0.7);
    BPoint end(size * IconMetrics::kCheckmarkEnd, size * 0.3);
    
    view->StrokeLine(start, mid);
    view->StrokeLine(mid, end);
}

/**
 * @brief Draw sync arrows overlay
 */
void
SyncStateIcons::_DrawSyncArrows(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw circle background
    view->SetHighColor(UI::kBackground);
    view->FillEllipse(bounds);
    
    // Draw sync arrows (simplified circular arrows)
    view->SetHighColor(color);
    view->SetPenSize(size * IconMetrics::kPenSizeMedium);
    
    // Top arc
    view->StrokeArc(bounds.InsetByCopy(size * IconMetrics::kInsetMedium, size * IconMetrics::kInsetMedium), 0, 180);
    
    // Arrow tips (simplified)
    view->SetPenSize(size * IconMetrics::kPenSizeThin);
    view->StrokeLine(BPoint(size * 0.8, size * 0.5),
                     BPoint(size * 0.7, size * 0.4));
    view->StrokeLine(BPoint(size * 0.2, size * 0.5),
                     BPoint(size * 0.3, size * 0.6));
}

/**
 * @brief Draw error X overlay
 */
void
SyncStateIcons::_DrawErrorX(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw circle background
    view->SetHighColor(UI::kBackground);
    view->FillEllipse(bounds);
    
    // Draw X
    view->SetHighColor(color);
    view->SetPenSize(size * IconMetrics::kPenSizeThick);
    
    float inset = size * IconMetrics::kInsetLarge;
    view->StrokeLine(BPoint(inset, inset),
                     BPoint(size - inset, size - inset));
    view->StrokeLine(BPoint(size - inset, inset),
                     BPoint(inset, size - inset));
}

/**
 * @brief Draw clock overlay
 */
void
SyncStateIcons::_DrawClock(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw circle background
    view->SetHighColor(UI::kBackground);
    view->FillEllipse(bounds);
    
    // Draw clock outline
    view->SetHighColor(color);
    view->SetPenSize(size * IconMetrics::kPenSizeThin);
    view->StrokeEllipse(bounds.InsetByCopy(1, 1));
    
    // Draw clock hands
    BPoint center(size / 2, size / 2);
    view->StrokeLine(center, BPoint(size / 2, size * 0.3));
    view->StrokeLine(center, BPoint(size * 0.7, size / 2));
}

/**
 * @brief Draw warning triangle overlay
 */
void
SyncStateIcons::_DrawWarning(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw white background
    view->SetHighColor(UI::kBackground);
    view->FillRect(bounds);
    
    // Draw triangle
    view->SetHighColor(color);
    BPoint top(size / 2, size * IconMetrics::kInsetSmall);
    BPoint left(size * IconMetrics::kInsetSmall, size * 0.9);
    BPoint right(size * 0.9, size * 0.9);
    
    BPoint triangle[3] = {top, left, right};
    view->FillPolygon(triangle, 3);
    
    // Draw exclamation mark
    view->SetHighColor(UI::kBackground);
    view->SetPenSize(size * IconMetrics::kPenSizeMedium);
    view->StrokeLine(BPoint(size / 2, size * 0.35),
                     BPoint(size / 2, size * 0.65));
    view->FillEllipse(BRect(size / 2 - size * 0.05, size * 0.75,
                            size / 2 + size * 0.05, size * 0.85));
}

/**
 * @brief Draw pin overlay
 */
void
SyncStateIcons::_DrawPin(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw circle background
    view->SetHighColor(UI::kBackground);
    view->FillEllipse(bounds);
    
    // Draw pin (simplified)
    view->SetHighColor(color);
    view->FillEllipse(BRect(size * 0.3, size * 0.2,
                            size * 0.7, size * 0.5));
    view->SetPenSize(size * IconMetrics::kPenSizeMedium);
    view->StrokeLine(BPoint(size / 2, size * 0.5),
                     BPoint(size / 2, size * 0.8));
}

/**
 * @brief Draw cloud overlay
 */
void
SyncStateIcons::_DrawCloud(BBitmap* bitmap, rgb_color color)
{
    BView* view = bitmap->ChildAt(0);
    if (!view) return;
    
    BRect bounds = bitmap->Bounds();
    float size = bounds.Width();
    
    // Draw white background
    view->SetHighColor(UI::kBackground);
    view->FillRect(bounds);
    
    // Draw simplified cloud shape
    view->SetHighColor(color);
    
    // Main cloud body
    view->FillEllipse(BRect(size * 0.2, size * 0.4,
                            size * 0.8, size * 0.7));
    // Left bump
    view->FillEllipse(BRect(size * 0.1, size * 0.45,
                            size * 0.4, size * 0.65));
    // Right bump
    view->FillEllipse(BRect(size * 0.6, size * 0.45,
                            size * 0.9, size * 0.65));
    // Top bump
    view->FillEllipse(BRect(size * 0.35, size * 0.3,
                            size * 0.65, size * 0.5));
}

/**
 * @brief Composite overlay onto base icon
 */
BBitmap*
SyncStateIcons::_CompositeIcons(const BBitmap* base, const BBitmap* overlay,
                                int32 position)
{
    if (!base || !overlay) {
        return NULL;
    }
    
    // Create result bitmap
    BBitmap* result = new BBitmap(base->Bounds(), B_RGBA32, true);
    if (!result || result->InitCheck() != B_OK) {
        delete result;
        return NULL;
    }
    
    // Create view for drawing
    BView* view = new BView(result->Bounds(), "composite", B_FOLLOW_NONE, 0);
    result->AddChild(view);
    result->Lock();
    
    // Draw base icon
    view->DrawBitmap(base, BPoint(0, 0));
    
    // Calculate overlay position (bottom-right corner by default)
    BRect baseBounds = base->Bounds();
    BRect overlayBounds = overlay->Bounds();
    BPoint overlayPos;
    
    switch (position) {
        case 0: // Top-left
            overlayPos.Set(0, 0);
            break;
        case 1: // Top-right
            overlayPos.Set(baseBounds.right - overlayBounds.Width(), 0);
            break;
        case 2: // Bottom-left
            overlayPos.Set(0, baseBounds.bottom - overlayBounds.Height());
            break;
        case static_cast<int32>(kOverlayPositionBottomRight): // Bottom-right (default)
        default:
            overlayPos.Set(baseBounds.right - overlayBounds.Width(),
                          baseBounds.bottom - overlayBounds.Height());
            break;
    }
    
    // Draw overlay with transparency
    view->SetDrawingMode(B_OP_ALPHA);
    view->DrawBitmap(overlay, overlayPos);
    
    view->Sync();
    result->Unlock();
    result->RemoveChild(view);
    delete view;
    
    return result;
}