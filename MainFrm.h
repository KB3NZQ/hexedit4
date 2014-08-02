// MainFrm.h : interface of the CMainFrame class
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "Control.h"
#include "GenDockablePane.h"
#include "FindDlg.h"
#include "BookmarkDlg.h"
#include "Prop.h"
#include "CalcDlg.h"
#include "Explorer.h"

#include "TipWnd.h"
#include "expr.h"
#include <FreeImage.h>

#define DFFD_RESERVED 1000   // Max number of commands on all template open menus (combined total)
#define NAV_RESERVED  2000   // Max number of commands on nav forward and backward menus (both separate)

// This is used by the hex and decimal address tools to evaluate expressions to jump to.
// (find_symbol is overridden to scan the list of bookmarks for the active file.)
class CJumpExpr : public expr_eval
{
public:
	CJumpExpr::CJumpExpr() : expr_eval(16, true) {}
	virtual value_t find_symbol(const char *sym, value_t parent, size_t index, int *pac,
		__int64 &sym_size, __int64 &sym_address, CString &sym_str);
	bool LoadVars();
	void SaveVars();
	std::vector<CString> GetVarNames(CJumpExpr::type_t typ);
};

class CMainFrame : public CMDIFrameWndEx
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();

// Attributes
public:

// Operations
public:
	void SetSearch(LPCTSTR ss) { m_wndFind.NewSearch(ss); current_search_string_ = ss; }
	void AddSearchHistory(const CString &);
	void LoadSearchHistory(CHexEditApp *aa);
	void SaveSearchHistory(CHexEditApp *aa);
	void AddReplaceHistory(const CString &);

	void LoadJumpHistory(CHexEditApp *aa);
	void SaveJumpHistory(CHexEditApp *aa);
	void AddHexHistory(const CString &);
	void AddDecHistory(const CString &);
	void StatusBarText(const char *mess = NULL);
	void bar_context(CPoint point);
	void SaveFrameOptions();
	BOOL UpdateBGSearchProgress();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL);
	virtual void RecalcLayout(BOOL bNotify = TRUE);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

	// BCG overrides
	virtual BOOL OnShowPopupMenu (CMFCPopupMenu* pMenuPopup);
	virtual void OnClosePopupMenu(CMFCPopupMenu* pMenuPopup);
	virtual BOOL OnEraseMDIClientBackground (CDC* pDC);

// Implementation
public:
	virtual ~CMainFrame();
	virtual void HtmlHelp(DWORD_PTR dwData, UINT nCmd);

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

public:
	// BCG menu bar and tool bar
	CMFCMenuBar m_wndMenuBar;
	CMFCToolBar m_wndStandardBar, m_wndEditBar;   // Tool bar and Edit bar
	CMFCToolBar m_wndFormatBar, m_wndNavBar;   // Format bar and Navigation bar

	// Modeless dialogs in panes
	CFindSheet m_wndFind;               // Find property sheet/bar
	CGenDockablePane m_paneFind;        // Docking pane that holds m_wndFind
	CBookmarkDlg m_wndBookmarks;        // Bookmarks dialog/bar
	CGenDockablePane m_paneBookmarks;   // Docking pane that holds m_wndBookmarks
	CPropSheet m_wndProp;               // Properties dialog/bar
	CGenDockablePane m_paneProp;        // Docking pane that holds m_wndProp
	CCalcDlg m_wndCalc;                 // Calculator dialog/bar
	CGenDockablePane m_paneCalc;        // Docking pane that holds m_wndCalc
	CExplorerWnd m_wndExpl;             // Explorer dialog/bar
	void UpdateExplorer(LPCTSTR ff = NULL) { m_wndExpl.Update(ff); }
	CGenDockablePane m_paneExpl;        // Docking pane that holds m_wndExpl

	CMFCToolBarImages	m_UserImages;
	CString m_strImagesFileName;

	std::vector<CString> hex_hist_;     // All hex jump addresses
	clock_t hex_hist_changed_;
	std::vector<CString> dec_hist_;     // All decimal jump addresses
	clock_t dec_hist_changed_;
	std::vector<CString> search_hist_;  // History list of all search strings
	std::vector<CString> replace_hist_; // History list of replace strings

	CStatBar m_wndStatusBar;

	// Progress (for long foreground processes)
	void Progress(int value);  // pass 0 to 100 or -1 to disable progress

// Generated message map functions
// protected:
public:
	afx_msg void OnInitMenu(CMenu* pMenu);
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);

	afx_msg void OnEditFind();
	afx_msg void OnUpdateEditFind(CCmdUI* pCmdUI);
	afx_msg void OnEditFind2();
	afx_msg void OnCalculator();
	afx_msg void OnCustomize();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnTest();
	//}}AFX_MSG
	afx_msg void OnSysCommand(UINT nID, LONG lParam);
	afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
	// afx_msg void OnEnterIdle(UINT nWhy, CWnd *pWho);

	afx_msg void OnNavigateBackwards();
	afx_msg void OnUpdateNavigateBackwards(CCmdUI* pCmdUI);
	afx_msg void OnNavBack(UINT nID);
	afx_msg void OnNavigateForwards();
	afx_msg void OnUpdateNavigateForwards(CCmdUI* pCmdUI);
	afx_msg void OnNavForw(UINT nID);

	afx_msg void OnEditReplace();
	afx_msg void OnCalcSel();
	//afx_msg void OnOptionsScheme();
	afx_msg void OnBookmarks();
	afx_msg void OnEditGotoDec();
	afx_msg void OnEditGotoHex();
	afx_msg void OnEditGoto(int base_mode = 0);
	afx_msg void OnWindowNew();
	afx_msg BOOL OnMDIWindowCmd(UINT nID);
	afx_msg BOOL OnPaneCheck(UINT nID);
	afx_msg void OnUpdateViewViewbar(CCmdUI* pCmdUI);
	afx_msg void OnViewViewbar();
	afx_msg void OnUpdateViewEditbar(CCmdUI* pCmdUI);
	afx_msg void OnViewEditbar();
	afx_msg void OnUpdateViewFormatbar(CCmdUI* pCmdUI);
	afx_msg void OnViewFormatbar();
	afx_msg void OnUpdateViewNavbar(CCmdUI* pCmdUI);
	afx_msg void OnViewNavbar();

	afx_msg void OnUpdateViewCalculator(CCmdUI* pCmdUI);
	afx_msg void OnViewCalculator();
	afx_msg void OnUpdateViewBookmarks(CCmdUI* pCmdUI);
	afx_msg void OnViewBookmarks();
	afx_msg void OnUpdateViewFind(CCmdUI* pCmdUI);
	afx_msg void OnViewFind();
	afx_msg void OnUpdateViewExpl(CCmdUI* pCmdUI);
	afx_msg void OnViewExpl();
	afx_msg void OnViewRuler();
	afx_msg void OnUpdateViewRuler(CCmdUI* pCmdUI);
	afx_msg void OnViewHighlightCaret();
	afx_msg void OnUpdateViewHighlightCaret(CCmdUI* pCmdUI);
	afx_msg void OnViewHighlightMouse();
	afx_msg void OnUpdateViewHighlightMouse(CCmdUI* pCmdUI);
	afx_msg void OnViewProperties();
	afx_msg void OnUpdateViewProperties(CCmdUI* pCmdUI);

	afx_msg void OnContextHelp();   // for Shift+F1 help
	afx_msg void OnHelpFinder();
	afx_msg void OnHelp();
	afx_msg void OnHelpKeyboardMap();
	afx_msg void OnHelpTute(UINT nID);

	afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM);
	afx_msg void OnChangeAddrHex();
	afx_msg void OnSelAddrHex();
	afx_msg void OnChangeAddrDec();
	afx_msg void OnSelAddrDec();
	afx_msg void OnUpdateCompares(CCmdUI *pCmdUI);
	afx_msg void OnUpdateOccurrences(CCmdUI *pCmdUI);
	afx_msg void OnUpdateValues(CCmdUI *pCmdUI);
	afx_msg void OnUpdateAddrHex(CCmdUI *pCmdUI);
	afx_msg void OnUpdateAddrDec(CCmdUI *pCmdUI);
	afx_msg void OnUpdateFileLength(CCmdUI *pCmdUI);
	afx_msg void OnUpdateBigEndian(CCmdUI *pCmdUI);
	afx_msg void OnUpdateReadonly(CCmdUI *pCmdUI);
	afx_msg void OnUpdateOvr(CCmdUI *pCmdUI);
	afx_msg void OnUpdateRec(CCmdUI *pCmdUI);
	afx_msg LONG OnOpenMsg(UINT wParam, LONG lParam);

//    afx_msg LONG OnFindDlgMess(WPARAM wParam, LPARAM lParam);
//    afx_msg LRESULT OnReturn(WPARAM, LPARAM);

	// BCG stuff
	afx_msg LRESULT OnToolbarReset(WPARAM,LPARAM);
	afx_msg LRESULT OnMenuReset(WPARAM,LPARAM);
	//BOOL OnToolsViewUserToolbar (UINT id);
	//void OnUpdateToolsViewUserToolbar (CCmdUI* pCmdUI);
	//afx_msg LRESULT OnToolbarContextMenu(WPARAM,LPARAM);
	afx_msg LRESULT OnHelpCustomizeToolbars(WPARAM wp, LPARAM lp);
	afx_msg void OnSearchCombo();
	afx_msg void OnUpdateSearchCombo(CCmdUI* pCmdUI);
	afx_msg void OnHexCombo();
	afx_msg void OnUpdateHexCombo(CCmdUI* pCmdUI);
	afx_msg void OnDecCombo();
	afx_msg void OnUpdateDecCombo(CCmdUI* pCmdUI);
	//afx_msg void OnUpdateSchemeCombo(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBookmarksCombo(CCmdUI* pCmdUI);

	afx_msg void OnFindNext();
	afx_msg void OnBookmarkAll();
	afx_msg void OnReplace();
	afx_msg void OnReplaceAll();
	afx_msg void OnSearchForw();
	afx_msg void OnSearchBack();
	afx_msg void OnUpdateSearch(CCmdUI* pCmdUI);
	afx_msg void OnSearchSel();
	afx_msg void OnUpdateSearchSel(CCmdUI* pCmdUI);
	afx_msg void OnDockableToggle();
	afx_msg void OnUpdateDockableToggle(CCmdUI* pCmdUI);

	afx_msg void OnApplicationLook(UINT id);
	afx_msg void OnUpdateApplicationLook(CCmdUI* pCmdUI);

	afx_msg LRESULT OnGetTabToolTip(WPARAM, LPARAM lp);
	afx_msg void OnMacro(UINT nID);

	DECLARE_MESSAGE_MAP()

public:
	BOOL DoFind();

	void show_calc();  // Make sure the calculator is displayed
	void move_bars(const CRect &rct)
	{
		move_dlgbar(m_paneFind, rct);
		move_dlgbar(m_paneBookmarks, rct);
		move_dlgbar(m_paneProp, rct);
		move_dlgbar(m_paneCalc, rct);
		move_dlgbar(m_paneExpl, rct);
	}
	void redraw_background() { m_wndClientArea.Invalidate(); }

	CString not_found_mess(BOOL forward, BOOL icase, int tt, BOOL ww, int aa);
	FILE_ADDRESS search_forw(CHexEditDoc *pdoc, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr,
							 const unsigned char *ss, const unsigned char *mask, size_t length,
							 BOOL icase, int tt, BOOL ww, int aa, int offset, bool align_rel, FILE_ADDRESS base_addr);
	FILE_ADDRESS search_back(CHexEditDoc *pdoc, FILE_ADDRESS start_addr, FILE_ADDRESS end_addr,
							 const unsigned char *ss, const unsigned char *mask, size_t length,
							 BOOL icase, int tt, BOOL ww, int aa, int offset, bool align_rel, FILE_ADDRESS base_addr);
//    CString GetSearchString() const { return current_search_string_; }
	void SetSearchString(CString ss) { current_search_string_ = ss; }
	void SetReplaceString(CString ss) { current_replace_string_ = ss; }

	CJumpExpr expr_;   // Used in calculating addresses using int expressions

//    FILE_ADDRESS GetAddress() const { return current_address_; }
	void SetAddress(FILE_ADDRESS fa) { current_address_ = fa; current_hex_address_="-1"; current_dec_address_="-1"; }
	void SetHexAddress(const char *ss);
	void SetDecAddress(const char *ss);
	FILE_ADDRESS GetHexAddress(CString &expr_str, CString &err_str);
	FILE_ADDRESS GetDecAddress(CString &expr_str, CString &err_str);

	int preview_page_;  // global value which was the only way I could work out how to pass the print preview page to CPreviewView

	BOOL ComboNeedsUpdate(const std::vector<CString> &vs, CComboBox *pp);

	void InitDockWindows();
	void FixPanes();

private:

	CHexEditDoc *GetPrevDoc(CHexEditDoc *pdoc);
#ifdef _DEBUG
	// Use smaller buffers to make it more likely to catch certain types of bugs
	enum { search_buf_len = 128 };
#else
	enum { search_buf_len = 32768 };
#endif
	CString last_mess_;     // Last status bar message received with refresh off

	// This is now just used so that the Find Tool can make sure it is displaying the latest search string
	// But the actual text/bytes to search for is now obtained directly from CFindSheet::GetSearch
	CString current_search_string_, current_replace_string_;
	FILE_ADDRESS current_address_;
	CString current_hex_address_, current_dec_address_;

	void move_dlgbar(CGenDockablePane &bar, const CRect &rct);  // move so it does not intersect with rct

	FIBITMAP *m_dib;
	int m_background_pos;
	CBitmap m_search_image;       // Displayed in background search occurrences (status bar) pane
	COLORREF bg_progress_colour_; // Current search progress bar colour or -1 if no progress bar shown

	// Current status bar pane widths - so we only adjust if necessary (to avoid flicker)
	// Note using CMFCStatusBar::GetPaneWidth won't do as it returns 0 for non-visible panes
	int ComparesWidth, OccurrencesWidth, ValuesWidth, AddrHexWidth, AddrDecWidth, FileLengthWidth;

//    UINT timer_id_;             // Timer id for updating bg search progress in the status bar

	CRect item_rect(CMFCPopupMenu *pm, UINT id);
	void show_tip(UINT id = -1);
	CTipWnd menu_tip_;            // Used for displaying a tip when some menu items are hovered
	std::vector<CMFCPopupMenu*> popup_menu_;    // We are only interested in the last (currently active) menu
	UINT last_id_;                // Last menu item mouse was over
	bool progress_on_;            // Used by Progress() method to say if we are currently displaying progress

	std::vector<CString> GetMacroCommands(); // get cmd IDs of macros for adding to macro sub-menu 
};

/////////////////////////////////////////////////////////////////////////////
