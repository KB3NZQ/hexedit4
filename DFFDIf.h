// DFFDIf.h
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#if !defined(AFX_DFFDIF_H__C56A4BD8_9D5A_41B8_B1AE_948A67916E38__INCLUDED_)
#define AFX_DFFDIF_H__C56A4BD8_9D5A_41B8_B1AE_948A67916E38__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "XMLTree.h"

// DFFDIf.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDFFDIf dialog

class CDFFDIf : public CDialog
{
// Construction
public:
	CDFFDIf(CXmlTree::CElt *pp, signed char parent_type, CWnd* pParent = NULL);   // standard constructor
	virtual ~CDFFDIf();

	bool IsModified() const { return modified_; }
	void SetModified(bool mm = true) { modified_ = mm; }
	void GetPosition(CPoint &pp) const { pp.x = pos_.x; pp.y = pos_.y; }
	void SetPosition(CPoint pp, CPoint oo = CPoint(-1,-1))
	{
		pos_ = pp;
		if (oo == CPoint(-1,-1))
			orig_ = pp, first_ = true;
		else
			orig_ = oo;
	}

// Dialog Data
	//{{AFX_DATA(CDFFDIf)
	enum { IDD = IDD_DFFD_IF };
	CButton ctl_prev_;
	CButton ctl_next_;
	CString comment_;
	CString condition_;
	//}}AFX_DATA
	CEdit ctl_condition_;
	CMFCMenuButton ctl_condition_var_;

	CString if_elt_name_;       // "name" of contained IF elt
	CButton ctl_if_edit_;
	CMFCMenuButton  ctl_if_replace_;

	int else_;                  // 0 = IF has no ELSE, 1 = has ELSE
	CButton ctl_else_;
	CString else_elt_name_;     // "name" of contained ELSE elt
	CEdit ctl_else_elt_name_;
	CButton ctl_else_edit_;
	CMFCMenuButton  ctl_else_replace_;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDFFDIf)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDFFDIf)
	afx_msg void OnNext();
	afx_msg void OnPrev();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnChange();
	virtual void OnCancel();
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	//}}AFX_MSG
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg LRESULT OnKickIdle(WPARAM, LPARAM);
	afx_msg void OnHelp();
	afx_msg void OnElse();
	afx_msg void OnIfEdit();
	afx_msg void OnIfReplace();
	afx_msg void OnElseEdit();
	afx_msg void OnElseReplace();
	afx_msg void OnGetConditionVar();
	DECLARE_MESSAGE_MAP()

	CXmlTree::CElt *pelt_;      // The IF element we are editing
	CXmlTree::CFrag saved_;     // Saved copy of the children (before editing) in case Cancel pressed
	bool saved_mod_flag_;       // If dialog cancelled then we have to restore the modified status
	bool show_prev_next_;       // Show the << and >> buttons (parent is STRUCT)
	CPoint pos_;                // Position of this dialog
	CPoint orig_;               // Position of the first dialog opened
	bool modified_;             // Have any of the values been changed?
	bool valid_if_element_;     // Is there a subelement for the IF?
	bool valid_else_element_;   // Is there a subelement for the ELSE (when else check box is checked)
	CMenu button_menu_;         // We need to keep the menu for the Replace/Insert button in memory
	bool first_;                // Is this the first dialog called (since any DFFD dialog can call any other)
	signed char parent_type_;   // Type of container of this element
	CMenu *pcondition_menu_;    // Menu displaying all availbale vars for "condition"

	void            load_data();
	bool            check_data();
	void            save_data();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DFFDIF_H__C56A4BD8_9D5A_41B8_B1AE_948A67916E38__INCLUDED_)
