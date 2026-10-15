/*
 * View_pat.h
 * ----------
 * Purpose: Pattern tab, lower panel.
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Globals.h"
#include "PatternCursor.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "PatternEditorDialogs.h"
#include "PatternClipboard.h"
#include "PatternAutomation.h"
#include "TrackHeader.h"
#include "UpdateHints.h"

OPENMPT_NAMESPACE_BEGIN

enum CommandID : int;

class CModDoc;
class CEditCommand;
class CEffectVis;
class CInputHandler;

// Drag & Drop info
class DragItem
{
	uint32 v = 0;

	enum : uint32
	{
		ValueMask = 0x00FFFFFF,
		TypeMask = 0xFF000000,
	};

public:
	enum DragType : uint32
	{
		ChannelHeader = 0x01000000,
		PatternHeader = 0x02000000,
		PluginName = 0x04000000,
		ChannelVolume = 0x08000000,
		ChannelPan = 0x10000000,
		ChannelMute = 0x20000000,
		ChannelSolo = 0x40000000,
		PanLaneToggle = 0x50000000,
		VolumeLaneToggle = 0x60000000,
		PanLane = 0x70000000,
		VolumeLane = 0x80000000,
		ChannelRecord = 0x81000000,
		ChannelInstrument = 0x82000000,
		ChannelRouting = 0x83000000,
		ChannelResize = 0x84000000,
	};

	DragItem() = default;
	DragItem(DragType type, uint32 value)
	    : v(type | value) {}

	DragType Type() const { return static_cast<DragType>(v & TypeMask); }
	uint32 Value() const { return v & ValueMask; }
	INT_PTR ToIntPtr() const { return v; }
	bool IsValid() const { return v != 0; }

	bool operator==(const DragItem other) const { return v == other.v; }
	bool operator!=(const DragItem other) const { return v != other.v; }
};


// Edit Step aka Row Spacing
inline constexpr ROWINDEX MAX_SPACING = MAX_PATTERN_ROWS;


struct PatternEditPos
{
	ROWINDEX row = ROWINDEX_INVALID;
	ORDERINDEX order = ORDERINDEX_INVALID;
	PATTERNINDEX pattern = PATTERNINDEX_INVALID;
	CHANNELINDEX channel = CHANNELINDEX_INVALID;
};


// Pattern editing class


class CViewPattern final : public CModScrollView
{
public:
	// Pattern status flags
	enum PatternStatus
	{
		psMouseDragSelect    = 0x01,     // Creating a selection using the mouse
		psFocussed           = 0x04,     // Is the pattern editor focussed
		psFollowSong         = 0x08,     // Does the cursor follow playback
		psRecordingEnabled   = 0x10,     // Recording enabled
		psDragVScroll        = 0x40,     // Indicates that the vertical scrollbar is being dragged
		psShowVUMeters       = 0x80,     // Display channel VU meters
		psChordPlaying       = 0x100,    // Is a chord playing? (pretty much unused)
		psDragnDropEdit      = 0x200,    // Drag & Drop editing (?)
		psDragnDropping      = 0x400,    // Dragging a selection around
		psShiftSelect        = 0x800,    // User has made at least one selection using Shift-Click since the Shift key has been pressed.
		psCtrlDragSelect     = 0x1000,   // Creating a selection using Ctrl
		psShowPluginNames    = 0x2000,   // Show plugin names in channel headers
		psRowSelection       = 0x4000,   // Selecting a whole pattern row by clicking the row numbers
		psChannelSelection   = 0x8000,   // Double-clicked pattern to select a whole channel
		psDragging           = 0x10000,  // Drag&Drop: Dragging an item around
		psShiftDragging      = 0x20000,  // Drag&Drop: Dragging an item around and holding shift

		// All possible drag flags, to test if user is dragging a selection or a scrollbar
		psDragActive = psDragVScroll | psMouseDragSelect | psRowSelection | psChannelSelection,
	};

protected:
	enum class WrapMode : uint8
	{
		IgnoreInvalidRow,   // If the row is outside the current pattern's bounds, nothing happens.
		WrapAround,         // Wrap to next / previous pattern(s) if row is out of bounds
		LimitAtPatternEnd,  // If the row is outside the current pattern's bounds, it is automatically corrected to the pattern's first / last row.
	};

	CFastBitmap m_Dib;
	CDC m_offScreenDC, m_vuMeterDC;
	CBitmap m_offScreenBitmap, m_vuMeterBitmap;
	CEditCommand *m_pEditWnd = nullptr;
	CSize m_szHeader, m_szPluginHeader, m_szCell;
	CRect m_oldClient;
	uint32 m_nMidRow, m_nSpacing, m_nLastPlayedRow, m_nLastPlayedOrder;
	FlagSet<PatternStatus> m_Status;
	ROWINDEX m_nPlayRow, m_nNextPlayRow;
	uint32 m_nPlayTick, m_nTicksOnRow;
	PATTERNINDEX m_nPattern = 0, m_nPlayPat = 0;
	ORDERINDEX m_nOrder = 0;
	static int32 m_nTransposeAmount;

	int m_nXScroll = 0, m_nYScroll = 0;
	int m_ledWidth = 0, m_ledHeight = 0;
	std::bitset<PatternCursor::numColumns> m_visibleColumns;

	// Cursor and selection positions
	PatternCursor m_Cursor;               // Current cursor position in pattern.
	PatternCursor m_StartSel, m_DragPos;  // Point where selection was started.
	PatternCursor m_MenuCursor;           // Position at which context menu was opened.
	PatternRect m_Selection;              // Upper-left / Lower-right corners of selection.

	// Drag&Drop
	DragItem m_nDragItem;  // Currently dragged item
	DragItem m_nDropItem;  // Currently hovered item during dragondrop
	RECT m_rcDragItem, m_rcDropItem;
	bool m_bInItemRect = false;

	static constexpr int TrackNameHeight = 20;
	static constexpr int TrackControlHeight = 18;
	TrackHeaderSelection m_trackSelection;
	std::vector<bool> m_trackSoloTargets;
	bool m_trackHeaderClick = false;
	int m_defaultTrackWidth = 0;
	CHANNELINDEX m_resizingTrack = CHANNELINDEX_INVALID;
	int m_resizeStartX = 0, m_resizeStartWidth = 0;
	DragItem m_trackSlider;
	bool m_trackSliderUndo = false;
	bool m_automationPresenceDirty = true;
	bool m_automationLinePending = false, m_automationContext = false, m_automationClick = false;
	AutomationNode m_lineStart, m_lineEnd;
	bool m_automationDragging = false, m_automationErasing = false, m_automationUndo = false;
	AutomationLane m_automationLane = AutomationLane::Pan;
	CHANNELINDEX m_automationChannel = CHANNELINDEX_INVALID;
	PATTERNINDEX m_automationPattern = PATTERNINDEX_INVALID;
	int m_automationLastRow = -1, m_automationLastValue = 0;

	// Drag-select record group
	std::vector<RecordGroup> m_initialDragRecordStatus;

	ModCommand::INSTR m_fallbackInstrument = 0;

	// Chord auto-detect interval
	DWORD m_autoChordStartTime = 0;
	ROWINDEX m_autoChordStartRow = ROWINDEX_INVALID;
	ORDERINDEX m_autoChordStartOrder = ORDERINDEX_INVALID;

	bool m_bContinueSearch : 1, m_bWholePatternFitsOnScreen : 1;

	ModCommand m_PCNoteEditMemory;  // PC Note edit memory
	static ModCommand m_cmdOld;     // Quick cursor copy/paste data

	QuickChannelProperties m_quickChannelProperties;

	// Chord preview
	CHANNELINDEX m_chordPatternChannels[MPTChord::notesPerChord];
	ModCommand::NOTE m_prevChordNote, m_prevChordBaseNote;

	// Note-off event buffer for MIDI sustain pedal
	std::array<std::vector<uint32>, 16> m_midiSustainBuffer;
	std::bitset<16> m_midiSustainActive;

	struct ChannelState
	{
		uint16 vuMeter = 0;
		uint16 vuMeterOld = 0;
		std::pair<PLUGINDEX, PlugParamIndex> previousPCevent = {PLUGINDEX_INVALID, 0};
		ModCommand::NOTE previousNote = NOTE_NONE;
		uint8 selectedCols = 0;
		bool hasPanAutomation = false, hasVolumeAutomation = false;
		bool panExpanded = false, volumeExpanded = false;
		int widthDip = 0;
	};

	std::vector<ChannelState> m_chnState;
	std::bitset<128> m_baPlayingNote;
	CModDoc::NoteToChannelMap m_noteChannel;  // Note -> Preview channel assignment
	std::array<ModCommand::NOTE, (NOTE_MAX - NOTE_MIN + 12) / 12> m_octaveKeyMemory;
	std::array<uint8, NOTE_MAX + NOTE_MIN> m_activeNoteChannel;
	std::array<uint8, NOTE_MAX + NOTE_MIN> m_splitActiveNoteChannel;
	static constexpr uint8 NOTE_CHANNEL_MAP_INVALID = 0xFF;
	static_assert(MAX_BASECHANNELS - 1 <= std::numeric_limits<decltype(m_activeNoteChannel)::value_type>::max());
	static_assert(MAX_BASECHANNELS - 1 < NOTE_CHANNEL_MAP_INVALID);

public:
	std::unique_ptr<CEffectVis> m_pEffectVis;

	CViewPattern();
	~CViewPattern();
	DECLARE_SERIAL(CViewPattern)

public:
	const CSoundFile *GetSoundFile() const;
	CSoundFile *GetSoundFile();

	const ModSequence &Order() const;
	ModSequence &Order();

	void SetModified(bool updateAllViews = true);

	bool IsSelectionPressed() const;

	bool UpdateSizes();
	void UpdateScrollSize();
	void UpdateScrollPos();
	void UpdateIndicator(bool updateAccessibility = true);
	void UpdateXInfoText();
	void UpdateColors();
	void UpdateVisibileColumns(std::bitset<PatternCursor::numColumns> visibleColumns);

	CString GetCursorDescription() const;

	int GetXScrollPos() const { return m_nXScroll; }
	int GetYScrollPos() const { return m_nYScroll; }
	int GetChannelWidth() const { return m_szCell.cx; }
	int GetRowHeight() const { return m_szCell.cy; }
	int GetSmoothScrollOffset() const;

	PATTERNINDEX GetCurrentPattern() const { return m_nPattern; }
	ROWINDEX GetCurrentRow() const { return m_Cursor.GetRow(); }
	CHANNELINDEX GetCurrentChannel() const { return m_Cursor.GetChannel(); }
	ORDERINDEX GetCurrentOrder() const { return m_nOrder; }
	void SetCurrentOrder(ORDERINDEX ord);
	// Get ModCommand at the pattern cursor position.
	ModCommand &GetCursorCommand() { return GetModCommand(m_Cursor); };
	const ModCommand& GetCursorCommand() const { return const_cast<CViewPattern *>(this)->GetModCommand(m_Cursor); };
	void SanitizeCursor();

	UINT GetColumnOffset(PatternCursor::Columns column) const;
	POINT GetPointFromPosition(PatternCursor cursor) const;
	PatternCursor GetPositionFromPoint(POINT pt) const;

	DragItem GetDragItem(CPoint point, RECT &rect) const;
	CRect GetTrackControlRect(CHANNELINDEX channel, DragItem::DragType type) const;
	CRect GetTrackSliderRect(CHANNELINDEX channel, DragItem::DragType type) const;
	bool CanEditTrackControl(DragItem::DragType type) const;
	bool IsTrackSolo(CHANNELINDEX channel) const;
	int GetTrackHeaderHeight() const { return TrackHeaderLayout(m_dpi).ControlsBottom(); }
	void SelectTrackHeader(CHANNELINDEX channel, UINT flags);
	void OnMuteTrackSelection(CHANNELINDEX channel);
	void OnSoloTrackSelection(CHANNELINDEX channel);
	bool IsTrackSoloMaskActive() const;
	void SoloTrackMask(const std::vector<bool> &selected, bool toggle = true);
	void DrawTrackControls(HDC dc, CHANNELINDEX channel, int lineWidth);
	void UpdateTrackSlider(CPoint point);
	void SetTrackSliderValue(int value);
	int GetNoteTrackWidth(CHANNELINDEX channel) const;
	void ResizeTracks(int delta, bool allTracks);
	void ShowTrackInstrumentMenu(CHANNELINDEX channel);
	CString GetTrackInstrumentLabel(CHANNELINDEX channel) const;
	bool CanGroupTrack(CHANNELINDEX channel, uint16 instrument = 0) const;
	void ShowTrackRoutingMenu(CHANNELINDEX channel);
	void CreateGroupBus();
	int GetTrackWidth(CHANNELINDEX channel) const;
	int GetTrackScrollPixel(CHANNELINDEX channel) const;
	CHANNELINDEX GetTrackAtScrollPixel(int pixel) const;
	int GetTrackLeft(CHANNELINDEX channel) const;
	int GetAutomationWidth(CHANNELINDEX channel, AutomationLane lane) const;
	CRect GetAutomationRect(CHANNELINDEX channel, AutomationLane lane) const;
	CHANNELINDEX FirstTrackToFit(CHANNELINDEX channel, int width) const;
	AutomationFormat GetAutomationFormat() const;
	void RefreshAutomationPresence();
	void ToggleAutomationLane(CHANNELINDEX channel, AutomationLane lane);
	void UpdateAutomationStroke(CPoint point);
	void EndAutomationStroke();
	int GetAutomationInitialValue(CHANNELINDEX channel, AutomationLane lane) const;
	std::vector<AutomationNode> GetAutomationNodes(CHANNELINDEX channel, AutomationLane lane) const;
	std::optional<AutomationNode> AutomationPointFromMouse(CHANNELINDEX channel, AutomationLane lane, CPoint point) const;
	CPoint AutomationNodePosition(CHANNELINDEX channel, AutomationLane lane, AutomationNode point) const;
	int FindAutomationNode(CHANNELINDEX channel, AutomationLane lane, CPoint point) const;
	void AutomationLineClick(CHANNELINDEX channel, AutomationLane lane, CPoint point);
	void WriteAutomationSegment(CHANNELINDEX channel, AutomationLane lane, const std::vector<AutomationNode> &nodes, int eraseFrom = -1, int eraseTo = -1);
	void EditAutomationNode(CHANNELINDEX channel, AutomationLane lane, CPoint point, bool reset);
	void CancelAutomationLine();
	void DrawAutomationLanes(HDC dc, bool headers, int lineWidth);

	
	void StartRecordGroupDragging(const DragItem source);
	void ResetRecordGroupDragging() { m_initialDragRecordStatus.clear(); }
	bool IsDraggingRecordGroup() const { return !m_initialDragRecordStatus.empty(); }

	ROWINDEX GetRowsPerBeat() const;
	ROWINDEX GetRowsPerMeasure() const;

	// Invalidate functions (for redrawing areas of the pattern)
	void InvalidatePattern(bool invalidateChannelHeaders = false, bool invalidateRowHeaders = false);
	void InvalidateRow(ROWINDEX n = ROWINDEX_INVALID);
	void InvalidateArea(const PatternRect &rect) { InvalidateArea(rect.GetUpperLeft(), rect.GetLowerRight()); };
	void InvalidateArea(PatternCursor begin, PatternCursor end);
	void InvalidateSelection() { InvalidateArea(m_Selection); }
	void InvalidateCell(PatternCursor cursor);
	void InvalidateChannelsHeaders(CHANNELINDEX chn = CHANNELINDEX_INVALID);

	// Selection functions
	void SetCurSel(const PatternRect &rect) { SetCurSel(rect.GetUpperLeft(), rect.GetLowerRight()); };
	void SetCurSel(const PatternCursor &point) { SetCurSel(point, point); };
	void SetCurSel(PatternCursor beginSel, PatternCursor endSel);
	void SetSelToCursor() { SetCurSel(m_Cursor); };

	bool SetCurrentPattern(PATTERNINDEX pat, ROWINDEX row = ROWINDEX_INVALID);
	ROWINDEX SetCurrentRow(ROWINDEX row, WrapMode wrapMode = WrapMode::IgnoreInvalidRow, bool updateHorizontalScrollbar = true);
	bool SetCurrentColumn(const PatternCursor &cursor) { return SetCurrentColumn(cursor.GetChannel(), cursor.GetColumnType()); };
	bool SetCurrentColumn(CHANNELINDEX channel, PatternCursor::Columns column = PatternCursor::firstColumn);
	// This should be used instead of consecutive calls to SetCurrentRow() then SetCurrentColumn()
	bool SetCursorPosition(const PatternCursor &cursor, WrapMode wrapMode = WrapMode::IgnoreInvalidRow);
	bool DragToSel(const PatternCursor &cursor, bool scrollHorizontal, bool scrollVertical, bool noMove = false);
	bool SetPlayCursor(PATTERNINDEX pat, ROWINDEX row, uint32 tick);
	bool UpdateScrollbarPositions(bool updateHorizontalScrollbar = true);
	bool ShowEditWindow();
	UINT GetCurrentInstrument() const;
	void SelectBeatOrMeasure(bool selectBeat);
	// Move pattern cursor to left or right, respecting invisible columns.
	void MoveCursor(bool moveRight);

	bool TransposeSelection(int transp);
	bool DataEntry(bool up, bool coarse);

	bool PrepareUndo(const PatternRect &selection, const char *description) { return PrepareUndo(selection.GetUpperLeft(), selection.GetLowerRight(), description); };
	bool PrepareUndo(const PatternCursor &beginSel, const PatternCursor &endSel, const char *description);
	void UndoRedo(bool undo);

	bool InsertOrDeleteRows(CHANNELINDEX firstChn, CHANNELINDEX lastChn, bool globalEdit, bool deleteRows);
	void DeleteRows(CHANNELINDEX firstChn, CHANNELINDEX lastChn, bool globalEdit = false);
	void InsertRows(CHANNELINDEX firstChn, CHANNELINDEX lastChn, bool globalEdit = false);

	void OnDropSelection();

public:
	void DrawPatternData(HDC hdc, const int lineWidth, PATTERNINDEX nPattern, bool selEnable, bool isPlaying, ROWINDEX startRow, ROWINDEX numRows, CHANNELINDEX startChan, CRect &rcClient, int *pypaint);
	void DrawLetter(int x, int y, char letter, int sizex = 10, int ofsx = 0);
	void DrawLetter(int x, int y, wchar_t letter, int sizex = 10, int ofsx = 0);
#if MPT_CXX_AT_LEAST(20)
	void DrawLetter(int x, int y, char8_t letter, int sizex = 10, int ofsx = 0);
#endif
	void DrawNote(int x, int y, UINT note, CTuning *pTuning = nullptr);
	void DrawInstrument(int x, int y, UINT instr);
	void DrawVolumeCommand(int x, int y, const ModCommand &mc, std::optional<int> defaultVolume, bool hex);
	void DrawChannelVUMeter(HDC hdc, int x, int y, UINT nChn);
	void UpdateAllVUMeters(Notification *pnotify);
	void DrawDragSel(HDC hdc);
	void OnDrawDragSel();
	// Returns result of GetDefaultVolume if default volume should be drawn, std::nullopt otherwise
	std::optional<int> DrawDefaultVolume(const ModCommand &m) const;

	void CursorJump(int distance, bool snap);

	void TempEnterNote(ModCommand::NOTE n, int vol = -1, bool fromMidi = false);
	void TempStopNote(ModCommand::NOTE note, const bool fromMidi = false, bool chordMode = false);
	void TempEnterChord(ModCommand::NOTE n);
	void TempStopChord(ModCommand::NOTE note) { TempStopNote(note, false, true); }
	void TempEnterIns(int val);
	void TempEnterOctave(int val);
	void TempStopOctave(int val);
	void TempEnterVol(CommandID cmd);
	void TempEnterFX(ModCommand::COMMAND c, int v = -1);
	void TempEnterFXparam(int v);
	void EnterAftertouch(ModCommand::NOTE note, int atValue);

	std::optional<int> GetDefaultVolume(const ModCommand &m, ModCommand::INSTR lastInstr = 0) const;
	int GetBaseNote() const;
	ModCommand::NOTE GetNoteWithBaseOctave(int note) const;

	// Construct a chord from the chord presets. Returns number of notes in chord.
	int ConstructChord(int note, ModCommand::NOTE (&outNotes)[MPTChord::notesPerChord], ModCommand::NOTE baseNote);

	void QuantizeRow(PATTERNINDEX &pat, ROWINDEX &row) const;
	PATTERNINDEX GetPrevPattern() const;
	PATTERNINDEX GetNextPattern() const;

	void SetSpacing(uint32 n);
	void OnClearField(const std::bitset<PatternCursor::numColumns> mask, bool step, bool ITStyle = false);
	void SetSelectionInstrument(const INSTRUMENTINDEX instr, bool setEmptyInstrument);

	void FindInstrument();
	void JumpToPrevOrNextEntry(bool nextEntry, bool select);

	void GotoPreviousOrder(std::optional<OrderTransitionMode> transitionMode = std::nullopt);
	void GotoNextOrder(std::optional<OrderTransitionMode> transitionMode = std::nullopt);
	void QueuePattern(ORDERINDEX order, OrderTransitionMode transitionMode);

	void TogglePluginEditor(int chan);

	void ExecutePaste(PatternClipboard::PasteModes mode);

	// Reset all channel variables
	void ResetChannel(CHANNELINDEX chn);

public:
	//{{AFX_VIRTUAL(CViewPattern)
	void OnDraw(CDC *) override;
	void OnInitialUpdate() override;
	void OnDPIChanged() override;
	BOOL OnScrollBy(CSize sizeScroll, BOOL bDoScroll = TRUE) override;
	BOOL PreTranslateMessage(MSG *pMsg) override;
	void UpdateView(UpdateHint hint, CObject *pObj = nullptr) override;
	LRESULT OnModViewMsg(WPARAM, LPARAM) override;
	LRESULT OnPlayerNotify(Notification *) override;
	INT_PTR OnToolHitTest(CPoint point, TOOLINFO *pTI) const override;
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CViewPattern)
	afx_msg BOOL OnEraseBkgnd(CDC *) { return TRUE; }
	afx_msg void OnSize(UINT nType, int cx, int cy);
	// cppcheck-suppress duplInheritedMember
	afx_msg void OnDestroy();
	// cppcheck-suppress duplInheritedMember
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnXButtonUp(UINT nFlags, UINT nButton, CPoint point);
	afx_msg void OnMouseMove(UINT, CPoint);
	afx_msg void OnCaptureChanged(CWnd *wnd);
	afx_msg void OnLButtonUp(UINT, CPoint);
	afx_msg void OnLButtonDown(UINT, CPoint);
	afx_msg void OnLButtonDblClk(UINT, CPoint);
	afx_msg void OnRButtonDown(UINT, CPoint);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	// cppcheck-suppress duplInheritedMember
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnKillFocus(CWnd *pNewWnd);
	afx_msg void OnEditCut();
	afx_msg void OnEditCopy();

	afx_msg void OnEditPaste() { ExecutePaste(PatternClipboard::pmOverwrite); };
	afx_msg void OnEditMixPaste() { ExecutePaste(PatternClipboard::pmMixPaste); };
	afx_msg void OnEditMixPasteITStyle() { ExecutePaste(PatternClipboard::pmMixPasteIT); };
	afx_msg void OnEditPasteFlood() { ExecutePaste(PatternClipboard::pmPasteFlood); };
	afx_msg void OnEditPushForwardPaste() { ExecutePaste(PatternClipboard::pmPushForward); };

	afx_msg void OnClearSelection(bool ITStyle = false, std::bitset<PatternCursor::numColumns> sb = std::bitset<PatternCursor::numColumns>{}.set());
	afx_msg void OnGrowSelection();
	afx_msg void OnShrinkSelection();
	afx_msg void OnEditSelectAll();
	afx_msg void OnEditSelectChannel();
	afx_msg void OnSelectCurrentChannel();
	afx_msg void OnSelectCurrentColumn();
	afx_msg void OnEditFind();
	afx_msg void OnEditGoto();
	afx_msg void OnEditFindNext();
	afx_msg void OnEditUndo();
	afx_msg void OnEditRedo();
	afx_msg void OnChannelReset();
	afx_msg void OnMuteFromClick();
	afx_msg void OnSoloFromClick();
	afx_msg void OnPanAutomation() { ToggleAutomationLane(GetCurrentChannel(), AutomationLane::Pan); }
	afx_msg void OnVolumeAutomation() { ToggleAutomationLane(GetCurrentChannel(), AutomationLane::Volume); }
	afx_msg void OnTracksWider() { ResizeTracks(12, true); }
	afx_msg void OnTracksNarrower() { ResizeTracks(-12, true); }
	afx_msg void OnTrackWider() { ResizeTracks(12, false); }
	afx_msg void OnTrackNarrower() { ResizeTracks(-12, false); }
	afx_msg void OnTrackInstrument() { ShowTrackInstrumentMenu(GetCurrentChannel()); }
	afx_msg void OnTrackGroupOutput() { ShowTrackRoutingMenu(GetCurrentChannel()); }
	afx_msg void OnTogglePendingMuteFromClick();
	afx_msg void OnPendingSoloChnFromClick();
	afx_msg void OnPendingUnmuteAllChnFromClick();
	afx_msg void OnSoloChannel(CHANNELINDEX first, CHANNELINDEX last);
	afx_msg void OnMuteChannel(CHANNELINDEX chn);
	afx_msg void OnUnmuteAll();
	afx_msg void OnRecordSelect();
	afx_msg void OnSplitRecordSelect();
	afx_msg void OnDeleteRow();
	afx_msg void OnDeleteWholeRow();
	afx_msg void OnDeleteRowGlobal();
	afx_msg void OnDeleteWholeRowGlobal();
	afx_msg void OnInsertRow();
	afx_msg void OnInsertWholeRow();
	afx_msg void OnInsertRowGlobal();
	afx_msg void OnInsertWholeRowGlobal();
	afx_msg void OnSplitPattern();
	afx_msg void OnPatternStep();
	afx_msg void OnSwitchToOrderList();
	afx_msg void OnPrevInstrument();
	afx_msg void OnNextInstrument();
	afx_msg void OnPatternRecord();
	afx_msg void OnInterpolateVolume() { Interpolate(PatternCursor::volumeColumn); }
	afx_msg void OnInterpolateEffect() { Interpolate(PatternCursor::effectColumn); }
	afx_msg void OnInterpolateNote() { Interpolate(PatternCursor::noteColumn); }
	afx_msg void OnInterpolateInstr() { Interpolate(PatternCursor::instrColumn); }
	afx_msg void OnVisualizeEffect();
	afx_msg void OnTransposeUp() { TransposeSelection(1); }
	afx_msg void OnTransposeDown() { TransposeSelection(-1); }
	afx_msg void OnTransposeOctUp() { TransposeSelection(12000); }
	afx_msg void OnTransposeOctDown() { TransposeSelection(-12000); }
	afx_msg void OnTransposeCustom();
	afx_msg void OnTransposeCustomQuick();
	afx_msg void OnSetSelInstrument();
	afx_msg void OnAddChannelFront() { AddChannel(m_MenuCursor.GetChannel(), false); }
	afx_msg void OnAddChannelAfter() { AddChannel(m_MenuCursor.GetChannel(), true); };
	afx_msg void OnDuplicateChannel();
	afx_msg void OnResetChannelColors();
	afx_msg void OnChannelSettings();
	afx_msg void OnTransposeChannel();
	afx_msg void OnRemoveChannel();
	afx_msg void OnRemoveChannelDialog();
	afx_msg void OnPatternProperties() { ShowPatternProperties(PATTERNINDEX_INVALID); }
	void ShowPatternProperties(PATTERNINDEX pat);
	void OnCursorCopy();
	void OnCursorPaste();
	afx_msg void OnPatternAmplify();
	afx_msg void OnUpdateUndo(CCmdUI *pCmdUI);
	afx_msg void OnUpdateRedo(CCmdUI *pCmdUI);
	afx_msg void OnSelectPlugin(UINT nID);
	afx_msg LRESULT OnMidiMsg(WPARAM, LPARAM);
	afx_msg LRESULT OnRecordPlugParamChange(WPARAM, LPARAM);
	afx_msg LRESULT OnCustomKeyMsg(WPARAM, LPARAM);
	afx_msg void OnClearSelectionFromMenu();
	afx_msg void OnSelectInstrument(UINT nid);
	afx_msg void OnSelectPCNoteParam(UINT nid);
	afx_msg void OnRunScript();
	afx_msg void OnShowTimeAtRow();
	afx_msg void OnTogglePCNotePluginEditor();
	afx_msg void OnSetQuantize();
	afx_msg void OnLockPatternRows();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	// Copy&Paste
	bool CopyPattern(PATTERNINDEX nPattern, const PatternRect &selection);
	bool PastePattern(PATTERNINDEX nPattern, const PatternCursor &pastePos, PatternClipboard::PasteModes mode);

	void SetSplitKeyboardSettings();
	bool HandleSplit(ModCommand &m, int note);
	bool IsNoteSplit(int note) const;

	CHANNELINDEX FindGroupRecordChannel(RecordGroup recordGroup, bool forceFreeChannel, CHANNELINDEX startChannel = 0) const;

	bool BuildChannelControlCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildPluginCtxMenu(HMENU hMenu, UINT nChn, const CSoundFile &sndFile) const;
	bool BuildRecordCtxMenu(HMENU hMenu, CInputHandler *ih, CHANNELINDEX nChn) const;
	bool BuildSoloMuteCtxMenu(HMENU hMenu, CInputHandler *ih, UINT nChn, const CSoundFile &sndFile) const;
	bool BuildRowInsDelCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildMiscCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildSelectionCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildGrowShrinkCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildInterpolationCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildInterpolationCtxMenu(HMENU hMenu, PatternCursor::Columns colType, CString label, UINT command) const;
	bool BuildEditCtxMenu(HMENU hMenu, CInputHandler *ih, CModDoc *pModDoc) const;
	bool BuildVisFXCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildTransposeCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildSetInstCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildAmplifyCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildPCNoteCtxMenu(HMENU hMenu, CInputHandler *ih) const;
	bool BuildTogglePlugEditorCtxMenu(HMENU hMenu, CInputHandler *ih) const;

	// Returns an ordered list of all channels in which a given column type is selected.
	CHANNELINDEX ListChansWhereColSelected(PatternCursor::Columns colType, std::vector<CHANNELINDEX> &chans) const;
	// Check if a column type is selected on any channel in the current selection.
	bool IsColumnSelected(PatternCursor::Columns colType) const;

	bool IsInterpolationPossible(PatternCursor::Columns colType) const;
	bool IsInterpolationPossible(ROWINDEX startRow, ROWINDEX endRow, CHANNELINDEX chan, PatternCursor::Columns colType) const;
	void Interpolate(PatternCursor::Columns type);
	PatternRect SweepPattern(bool (*startCond)(const ModCommand &), bool (*endCond)(const ModCommand &, const ModCommand &)) const;

	// Return true if recording live (i.e. editing while following playback).
	bool IsLiveRecord() const;

	// Returns edit position.
	PatternEditPos GetEditPos(const CSoundFile &sndFile, const bool liveRecord) const;

	// Returns pointer to modcommand at given position.
	// If the position is not valid, a pointer to a dummy command is returned.
	ModCommand &GetModCommand(PatternCursor cursor);
	ModCommand &GetModCommand(CSoundFile &sndFile, const PatternEditPos &pos);

	// Returns true if pattern editing is enabled.
	bool IsEditingEnabled() const { return m_Status[psRecordingEnabled]; }

	// Like IsEditingEnabled(), but shows some notification when editing is not enabled.
	bool IsEditingEnabled_bmsg();

	CHANNELINDEX GetRecordChannelForPCEvent(PLUGINDEX plugSlot, PlugParamIndex paramIndex) const;

	// Play one pattern row and stop ("step mode")
	void PatternStep(ROWINDEX row = ROWINDEX_INVALID);

	// Add a channel.
	void AddChannel(CHANNELINDEX parent, bool afterCurrent);

	void DragChannel(CHANNELINDEX source, CHANNELINDEX target, CHANNELINDEX numChannels, bool duplicate);

private:
	void TogglePendingMute(CHANNELINDEX nChn);
	void PendingSoloChn(CHANNELINDEX first, CHANNELINDEX last);

	template <typename Func>
	void ApplyToSelection(Func func);

	void PlayNote(ModCommand::NOTE note, ModCommand::INSTR instr, int volume, CHANNELINDEX channel);
	void PreviewNote(ROWINDEX row, CHANNELINDEX channel);
	void StopPreview(ROWINDEX row, CHANNELINDEX channel);

	void CreateVUMeterBitmap();

	PatternCursor::Columns LastVisibleColumn() const noexcept;

public:
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

	HRESULT get_accName(VARIANT varChild, BSTR *pszName) override;
};

DECLARE_FLAGSET(CViewPattern::PatternStatus);

void getXParam(ModCommand::COMMAND command, PATTERNINDEX nPat, ROWINDEX nRow, CHANNELINDEX nChannel, const CSoundFile &sndFile, UINT &xparam, UINT &multiplier);

OPENMPT_NAMESPACE_END
