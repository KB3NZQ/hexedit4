// TabView.cpp : allows nested (tabbed) views: CHexEditView,CDataFormatView,CAerialView
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//


#include "stdafx.h"
#include "HexEdit.h"
#include "ChildFrm.h"
#include "TabView.h"
#include "HexEditView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CHexTabView

IMPLEMENT_DYNCREATE(CHexTabView, CTabView)

CHexTabView::CHexTabView()
{
	init_ = false;
}

CHexTabView::~CHexTabView()
{
}


BEGIN_MESSAGE_MAP(CHexTabView, CTabView)
	//{{AFX_MSG_MAP(CHexTabView)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
	ON_WM_SIZE()
END_MESSAGE_MAP()

void CHexTabView::OnSize(UINT nType, int cx, int cy)
{
	// This is necessary since we can get resize events before the tab view
	// is ready (before tab window has been created).
	// This caused crash in 2.6 if Window/New Window is used when window maximized.
	if (init_)
		CTabView::OnSize(nType, cx, cy);
}

void CHexTabView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
	if (bActivate)
	{
		// Make sure hex view is active
		SetActiveView(0);

		// Get child frame
		CWnd *pp = GetParent();
		while (pp != NULL && !pp->IsKindOf(RUNTIME_CLASS(CChildFrame)))
			pp = pp->GetParent();
		ASSERT_KINDOF(CChildFrame, pp);

		//((CChildFrame *)pp)->MDIActivate();
		((CChildFrame *)pp)->SetActiveView(GetActiveView());
		//GetActiveView()->SetFocus();
	}
}

/////////////////////////////////////////////////////////////////////////////
// CHexTabView drawing

void CHexTabView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CHexTabView diagnostics

#ifdef _DEBUG
void CHexTabView::AssertValid() const
{
	CTabView::AssertValid();
}

void CHexTabView::Dump(CDumpContext& dc) const
{
	CTabView::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CHexTabView message handlers

int CHexTabView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CTabView::OnCreate(lpCreateStruct) == -1)
		return -1;

	init_ = true;

	// If there is only one tab (ie, hex view only) hide the tabs
	GetTabControl().HideSingleTab();
	GetTabControl().AutoDestroyWindow();        // otherwise windows are not detroyed

	//AddView(RUNTIME_CLASS (CHexEditView), _T("Raw Data (Hex) View"));
	AddView(RUNTIME_CLASS(CHexEditView), _T("Raw Data (Hex) View"), -1, (CCreateContext *)lpCreateStruct->lpCreateParams);

	return 0;
}

BOOL CHexTabView::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}
