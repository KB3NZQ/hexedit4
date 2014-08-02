// ChildFrm.cpp : implementation of the CChildFrame class
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "HexEdit.h"

#include "MainFrm.h"
#include "HexEditView.h"
#include "DataFormatView.h"
#include "AerialView.h"

#include "ChildFrm.h"
#include <afxpriv.h>            // for WM_HELPHITTEST

extern CHexEditApp theApp;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChildFrame

IMPLEMENT_DYNCREATE(CChildFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(CChildFrame, CMDIChildWndEx)
		//{{AFX_MSG_MAP(CChildFrame)
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
		ON_WM_SYSCOMMAND()
		ON_MESSAGE(WM_HELPHITTEST, OnHelpHitTest)
		ON_WM_SETFOCUS()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CChildFrame construction/destruction

CChildFrame::CChildFrame()
{
}

CChildFrame::~CChildFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// CChildFrame diagnostics

#ifdef _DEBUG
void CChildFrame::AssertValid() const
{
		CMDIChildWndEx::AssertValid();
}

void CChildFrame::Dump(CDumpContext& dc) const
{
		CMDIChildWndEx::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CChildFrame message handlers

BOOL CChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	ASSERT(pContext != NULL && pContext->m_pNewViewClass != NULL);

	if (!splitter_.CreateStatic(this, 1, 4))
	{
		AfxMessageBox("Failed to create splitter.");
		return FALSE;
	}

	// We create with 4 columns then delete all except one, which means only one (for
	// hex view) is shown but more can be added (template, aerial views).
	splitter_.DelColumn(3);
	splitter_.DelColumn(2);
	splitter_.DelColumn(1);

	if (!splitter_.CreateView(0, 0, RUNTIME_CLASS(CHexTabView), CSize(0, 0), pContext))
	{
		AfxMessageBox("Failed to create splitter view.");
		return FALSE;
	}
	ptv_ = (CHexTabView *)splitter_.GetPane(0, 0);
	ASSERT_KINDOF(CHexTabView, ptv_);

	return TRUE;
}

// All child frame's will have exactly one CHexEditView (unless in print preview?).  This returns it.
CHexEditView *CChildFrame::GetHexEditView() const
{
	CView *pv = GetActiveView();
	if (pv != NULL)                         // May be NULL if print preview
	{
		if (pv->IsKindOf(RUNTIME_CLASS(CHexEditView)))
			return (CHexEditView *)pv;
		else if (pv->IsKindOf(RUNTIME_CLASS(CDataFormatView)))
			return ((CDataFormatView *)pv)->phev_;
		else if (pv->IsKindOf(RUNTIME_CLASS(CAerialView)))
			return ((CAerialView *)pv)->phev_;
		else if (pv->IsKindOf(RUNTIME_CLASS(CHexTabView)))
		{
			// Find the hex view (left-most tab)
			CHexTabView *ptv = (CHexTabView *)pv;
			ptv->SetActiveView(0);  // hex view is always left-most (index 0)
			ASSERT_KINDOF(CHexEditView, ptv->GetActiveView());
			return (CHexEditView *)ptv->GetActiveView();
		}
	}
	return NULL;
}

BOOL CChildFrame::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) 
{
	// Bypass calling CMDIChildWndEx::LoadFrame which loads an icon
	BOOL bRtn = CMDIChildWndEx::LoadFrame( nIDResource, dwDefaultStyle, pParentWnd, pContext );

	return bRtn;
}

BOOL CChildFrame::DestroyWindow() 
{
	return CMDIChildWndEx::DestroyWindow();
}

// Handles control menu commands and system buttons (Minimize etc)
void CChildFrame::OnSysCommand(UINT nID, LONG lParam)
{
	CMDIChildWndEx::OnSysCommand(nID, lParam);

	CHexEditApp *aa = dynamic_cast<CHexEditApp *>(AfxGetApp());
	nID &= 0xFFF0;
	if (nID == SC_MINIMIZE || nID == SC_RESTORE || nID == SC_MAXIMIZE ||
		nID == SC_NEXTWINDOW || nID == SC_PREVWINDOW || nID == SC_CLOSE)
	{
		if ((nID == SC_NEXTWINDOW || nID == SC_PREVWINDOW || nID == SC_CLOSE) &&
			aa->recording_ && aa->mac_.size() > 0 && (aa->mac_.back()).ktype == km_focus)
		{
			// Next win, prev. win, close win cause focus change which causes a km_focus
			// for a particular window to be stored.  On replay, we don't want to
			// change to this window before executing this command.
			aa->mac_.pop_back();
		}
		aa->SaveToMacro(km_childsys, nID);
	}
}

// Handles Shift-F1 help when clicked within the window (lParam contains
// point in high and low words).  At the moment nothing is done, so all
// help goes to the same place (HIDR_HEXEDTYPE).
LRESULT CChildFrame::OnHelpHitTest(WPARAM wParam, LPARAM lParam)
{
	// DEBUG: This is just here so we can intercept in debugger
	LRESULT retval = CMDIChildWndEx::OnHelpHitTest(wParam, lParam);
	return retval;
}

void CChildFrame::OnClose() 
{
	CMDIChildWndEx::OnClose();
}

void CChildFrame::OnSetFocus(CWnd* pOldWnd) 
{
	CMDIChildWndEx::OnSetFocus(pOldWnd);

	CHexEditView *pv = GetHexEditView();
	if (pv != NULL)
	{
		pv->show_prop();
		pv->show_calc();
		pv->show_pos();
	}
}
