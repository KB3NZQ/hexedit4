// Algorithm.cpp : implements dialog that gets encryption algorithm
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include "stdafx.h"
#include "HexEdit.h"
#include "Algorithm.h"
#include "resource.hm"
#include "HelpID.hm"            // User defined help IDs
#include <HtmlHelp.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAlgorithm dialog


CAlgorithm::CAlgorithm(CWnd* pParent /*=NULL*/)
	: CDialog(CAlgorithm::IDD, pParent)
{
	//{{AFX_DATA_INIT(CAlgorithm)
	m_note = _T("");
	m_blocklen = _T("");
	m_keylen = _T("");
	m_alg = -1;
	//}}AFX_DATA_INIT

	m_note = "Notes:\n"
			 "1. All algorithms except the first (the HexEdit internal algorithm) are provided by 3rd party\n"
			 "    Crypto API CSPs (Cryptographic Service Providers) installed on this computer.\n"
			 "2. To decrypt on a different computer you must ensure the same algorithm is present.\n"
			 "    Microsoft Base Cryptographic Provider v1.0 RC2 && RC4 algorithms are always present.\n"
			 "3. The HexEdit internal algorithm requires the selection to be a multiple of 8 bytes.\n"
			 "4. CSP block ciphers typically increase the encrypted length, usually by one block.\n"
			 "    You must remember the encrypted length in order to decrypt.\n"
			 "5. Stream ciphers do not have a block size, and do not change the encrypted length.\n"
//             "6. The greater the key length the harder the encryption is to break."
			 ;
}

void CAlgorithm::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAlgorithm)
	DDX_Control(pDX, IDC_ALG, m_control);
	DDX_Text(pDX, IDC_ALG_NOTE, m_note);
	DDX_Text(pDX, IDC_ALG_BLOCKLEN, m_blocklen);
	DDX_Text(pDX, IDC_ALG_KEYLEN, m_keylen);
	DDX_CBIndex(pDX, IDC_ALG, m_alg);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAlgorithm, CDialog)
	//{{AFX_MSG_MAP(CAlgorithm)
	ON_CBN_SELCHANGE(IDC_ALG, OnSelchangeAlg)
	ON_WM_HELPINFO()
	ON_BN_CLICKED(IDC_ALGORITHM_HELP, OnAlgorithmHelp)
	//}}AFX_MSG_MAP
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAlgorithm message handlers

BOOL CAlgorithm::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CWaitCursor wait;
	CHexEditApp *aa = dynamic_cast<CHexEditApp *>(AfxGetApp());

	// Put all the algorithms into the drop list
	for (int ii = 0; ii < aa->crypto_.GetNum(); ++ii)
		m_control.AddString(aa->crypto_.GetName(ii));

	// Select the current algorithm
	ASSERT(m_alg == aa->algorithm_);
	ASSERT(m_alg < aa->crypto_.GetNum());
	if (aa->crypto_.GetBlockLength(m_alg) == 0)
		m_blocklen = "Stream cipher";
	else
		m_blocklen.Format("%ld bytes (%ld bits)", long(aa->crypto_.GetBlockLength(m_alg)/8),
												  long(aa->crypto_.GetBlockLength(m_alg)));
	if (aa->crypto_.GetKeyLength(m_alg) == -1)
		m_keylen = "Unknown";
	else
		m_keylen.Format("%ld bits", long(aa->crypto_.GetKeyLength(m_alg)));

	UpdateData(FALSE);

	return TRUE;
}

void CAlgorithm::OnOK() 
{
	CDialog::OnOK();
}

void CAlgorithm::OnSelchangeAlg() 
{
	CHexEditApp *aa = dynamic_cast<CHexEditApp *>(AfxGetApp());

	UpdateData();

	if (aa->crypto_.GetBlockLength(m_alg) == 0)
		m_blocklen = "Stream cipher";
	else
		m_blocklen.Format("%ld bytes (%ld bits)", long(aa->crypto_.GetBlockLength(m_alg)/8),
												  long(aa->crypto_.GetBlockLength(m_alg)));
	if (aa->crypto_.GetKeyLength(m_alg) == -1)
		m_keylen = "Unknown";
	else
		m_keylen.Format("%ld bits", long(aa->crypto_.GetKeyLength(m_alg)));

	UpdateData(FALSE);
}

void CAlgorithm::OnAlgorithmHelp() 
{
	// Display help for the dialog
	if (!::HtmlHelp(AfxGetMainWnd()->m_hWnd, theApp.htmlhelp_file_, HH_HELP_CONTEXT, HIDD_ALGORITHM_HELP))
		AfxMessageBox(AFX_IDP_FAILED_TO_LAUNCH_HELP);
}

static DWORD id_pairs[] = { 
	IDC_ALG, HIDC_ALG,
	IDC_ALG_BLOCKLEN, HIDC_ALG_BLOCKLEN,
	IDC_ALG_KEYLEN, HIDC_ALG_KEYLEN,
	0,0 
};

BOOL CAlgorithm::OnHelpInfo(HELPINFO* pHelpInfo) 
{
	theApp.HtmlHelpWmHelp((HWND)pHelpInfo->hItemHandle, id_pairs);
	return TRUE;
}

void CAlgorithm::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	theApp.HtmlHelpContextMenu(pWnd, id_pairs);
}
