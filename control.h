#ifndef CONTROL_INCLUDED_
#define CONTROL_INCLUDED_  1

// Control.h : header file
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "misc.h"

/////////////////////////////////////////////////////////////////////////////
// CHexEdit control allows entry of hex digits

class CHexEdit : public CEdit
{
	DECLARE_DYNAMIC(CHexEdit)

public:
	CHexEdit() : group_by_(4), right_align_(false), allow_qmark_(false) { }

	void add_spaces();                              // add spaces for grouping
	void set_group_by(int gb) { group_by_ = gb; }   // set grouping
	void set_right_align(bool ra = true) { right_align_ = ra; }
	void set_allow_qmark(bool aq = true) { allow_qmark_ = aq; }

protected:
	int group_by_;                      // how many hex digits in a group
	bool right_align_;                  // create groups aligned with right digit
	bool allow_qmark_;                  // allow question mark character in the string

	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CDecEdit control allows entry of a decimal number

class CDecEdit : public CEdit
{
	DECLARE_DYNAMIC(CDecEdit)

public:
	CDecEdit();

	void add_commas();                              // add commas for thousands separation
	void set_allow_neg(bool an = true) { allow_neg_ = an; }

protected:
	bool allow_neg_;                    // Allow negative numbers
	int group_by_;                      // how many digits in a group (usually 3)
	char sep_char_;                     // separator char (usually comma)

	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CHexEditControl window

class CHexEditControl : public CMFCToolBarComboBoxEdit
{
	DECLARE_DYNAMIC(CHexEditControl)

// Construction
public:
	CHexEditControl(CMFCToolBarComboBoxButton & combo);

// Attributes
public:

// Operations
public:
	void Redisplay();            // Make sure hex digits case OK etc
	void add_spaces();
	static void BeginJump();
	static void RedisplayAll();  // Find all instances and redisplay

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHexEditControl)
	//}}AFX_VIRTUAL

// Implementation
public:

	// Generated message map functions
protected:
	//{{AFX_MSG(CHexEditControl)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	//}}AFX_MSG
	afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM);

	DECLARE_MESSAGE_MAP()

private:
	CBrush m_brush;
};

/////////////////////////////////////////////////////////////////////////////
// CHexComboBox

class CHexComboBox : public CComboBox
{
	DECLARE_DYNCREATE(CHexComboBox)

// Implementation
public:

	// Generated message map functions
protected:
	afx_msg void OnSelendok();
	afx_msg void OnSelchange();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CHexComboButton

class CHexComboButton : public CMFCToolBarComboBoxButton
{
	DECLARE_SERIAL(CHexComboButton)

// Construction
public:
	CHexComboButton() :
		CMFCToolBarComboBoxButton(ID_JUMP_HEX_COMBO, 
			afxCommandManager->GetCmdImage(ID_JUMP_HEX), //CImageHash::GetImageOfCommand(ID_JUMP_HEX),
			CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP,
			120)
	{
	}

// Attributes
public:

// Operations
public:

// Overrides
protected:

	virtual CComboBox *CreateCombo(CWnd* pWndParent, const CRect& rect);
	virtual CHexEditControl * CreateEdit(CWnd* pWndParent, const CRect& rect, DWORD dwEditStyle);
	virtual BOOL NotifyCommand(int iNotifyCode);

// Implementation
public:

	// Generated message map functions
protected:

private:

};

/////////////////////////////////////////////////////////////////////////////
// CDecEditControl window

class CDecEditControl : public CMFCToolBarComboBoxEdit
{
	DECLARE_DYNAMIC(CDecEditControl)

// Construction
public:
	CDecEditControl(CMFCToolBarComboBoxButton & combo);

// Attributes
public:

// Operations
public:
	void add_commas();
	char sep_char_;     // Char to use as "thousands" separator
	int group_;         // Number of digits in "thousands" group
	bool allow_neg_;    // Leading minus sign allowed

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDecEditControl)
	//}}AFX_VIRTUAL

// Implementation
public:

	// Generated message map functions
protected:
	//{{AFX_MSG(CDecEditControl)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	//}}AFX_MSG
	afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM);

	DECLARE_MESSAGE_MAP()

private:
	CBrush m_brush;
};

/////////////////////////////////////////////////////////////////////////////
// CDecComboBox

class CDecComboBox : public CComboBox
{
	DECLARE_DYNCREATE(CDecComboBox)

// Implementation
public:

	// Generated message map functions
protected:
	afx_msg void OnSelendok();
	afx_msg void OnSelchange();

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CDecComboButton

class CDecComboButton : public CMFCToolBarComboBoxButton
{
	DECLARE_SERIAL(CDecComboButton)

// Construction
public:
	CDecComboButton() :
		CMFCToolBarComboBoxButton(ID_JUMP_DEC_COMBO, 
			-1 /*CImageHash::GetImageOfCommand(ID_JUMP_DEC, FALSE)*/,
			CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP,
			142)
	{
	}

// Overrides
protected:
	virtual CComboBox *CreateCombo(CWnd* pWndParent, const CRect& rect);
	virtual CDecEditControl * CreateEdit(CWnd* pWndParent, const CRect& rect, DWORD dwEditStyle);
	virtual BOOL NotifyCommand(int iNotifyCode);

// Implementation
public:

	// Generated message map functions
protected:

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CSearchEditControl window

class CSearchEditControl : public CMFCToolBarComboBoxEdit
{
	DECLARE_DYNAMIC(CSearchEditControl)

// Construction
public:
	CSearchEditControl(CMFCToolBarComboBoxButton & combo);

// Attributes
public:
	// The characters used for an ASCII search + ASCII search ignoring case
	enum { sflag_char = '=', iflag_char = '~' };
	enum mode_t { mode_none, mode_hex, mode_char, mode_icase };
	void SetMode(enum mode_t m) { mode_ = m; }
	mode_t GetMode() { return mode_; }
	void Redisplay();            // Make sure hex digits case OK etc
//    BOOL in_edit_;

// Operations
public:
	static void BeginSearch(enum mode_t);
	static void RedisplayAll();

// Implementation
public:

	// Generated message map functions
protected:
	//{{AFX_MSG(CSearchEditControl)
	afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
	afx_msg LRESULT OnCommandHelp(WPARAM, LPARAM);

	DECLARE_MESSAGE_MAP()

private:
//    CString saved_;
	enum mode_t mode_;

	BOOL process_char(UINT nChar);
	void convert2hex();
	void convert2chars(char c1 = sflag_char);
	void normalize_hex();
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CFindComboBox

class CFindComboBox : public CComboBox
{
	DECLARE_DYNCREATE(CFindComboBox)

// Operations
public:
	void SetSearchString();

	// Generated message map functions
protected:
	//{{AFX_MSG(CFindComboBox)
	afx_msg void OnSelchange();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CFindComboButton

class CFindComboButton : public CMFCToolBarComboBoxButton
{
	DECLARE_SERIAL(CFindComboButton)

// Construction
public:
	CFindComboButton() :
		CMFCToolBarComboBoxButton(ID_SEARCH_COMBO, 
			-1 /*CImageHash::GetImageOfCommand(ID_SEARCH, FALSE)*/,
			CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP,
			200)
	{
	}

// Overrides
protected:
	virtual CComboBox *CreateCombo(CWnd* pWndParent, const CRect& rect);
	virtual CSearchEditControl * CreateEdit(CWnd* pWndParent, const CRect& rect, DWORD dwEditStyle);
	virtual BOOL NotifyCommand(int iNotifyCode);
};

/*
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CSchemeComboBox window

class CSchemeComboBox : public CComboBox
{
	DECLARE_DYNCREATE(CSchemeComboBox)

// Construction
public:
	CSchemeComboBox();

// Attributes
public:

// Operations
public:
	void SetScheme();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSchemeComboBox)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CSchemeComboBox();

	// Generated message map functions
protected:
	//{{AFX_MSG(CSchemeComboBox)
	afx_msg void OnSelchange();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CSchemeComboButton

class CSchemeComboButton : public CMFCToolBarComboBoxButton
{
	DECLARE_SERIAL(CSchemeComboButton)

// Construction
public:
	CSchemeComboButton() :
	CMFCToolBarComboBoxButton(::IsUs() ? ID_SCHEME_COMBO_US : ID_SCHEME_COMBO, 
			-1,
			CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP,
			120)
	{
		TRACE1("Is US %d\n", ::IsUs());
	}

// Attributes
public:

// Operations
public:

// Overrides
protected:

	virtual CComboBox *CreateCombo(CWnd* pWndParent, const CRect& rect);
//	virtual BOOL NotifyCommand(int iNotifyCode);

// Implementation
public:
	virtual ~CSchemeComboButton()
	{
	}

	// Generated message map functions
protected:

};
*/

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CBookmarksComboBox window

class CBookmarksComboBox : public CComboBox
{
	DECLARE_DYNCREATE(CBookmarksComboBox)

// Implementation
public:

	// Generated message map functions
protected:
	afx_msg void OnSelchange();
//	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CBookmarksComboButton

class CBookmarksComboButton : public CMFCToolBarComboBoxButton
{
	DECLARE_SERIAL(CBookmarksComboButton)

// Construction
public:
	CBookmarksComboButton() :
		CMFCToolBarComboBoxButton(ID_BOOKMARKS_COMBO, 
			-1 /*CImageHash::GetImageOfCommand(ID_BOOKMARKS, FALSE)*/,
			CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | CBS_SORT | WS_VSCROLL | WS_TABSTOP,
			150)
	{
	}

// Overrides
protected:

	virtual CComboBox *CreateCombo(CWnd* pWndParent, const CRect& rect);
//	virtual BOOL NotifyCommand(int iNotifyCode);

// Implementation
public:

	// Generated message map functions
protected:

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CStatBar window

class CStatBar : public CMFCStatusBar
{
// Construction
public:
	void SetToolTips();

// Attributes
public:

// Operations
public:

// Overrides

// Implementation
public:

	//CToolTipCtrl ttip_;

	// Generated message map functions
protected:
	//{{AFX_MSG(CStatBar)
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	//void add_tooltip(UINT id, const char *ss);
	//void move_tooltip(UINT id);
};

/////////////////////////////////////////////////////////////////////////////
#endif
