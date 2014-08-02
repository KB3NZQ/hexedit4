// BCGMisc.h - for misc classes derived from MFC (previously BCG) classes
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

//#include <BCGCB.h>

// We just need to access some protected members
class CHexEditFontCombo : public CMFCToolBarFontComboBox
{
	DECLARE_SERIAL(CHexEditFontCombo)
protected:
	static BYTE saved_charset;
	CHexEditFontCombo() : CMFCToolBarFontComboBox() {}
public:
#if 0 // This causes problems due to unavoidable call to RebuildFonts which includes printer fonts (can be very slow)
	CHexEditFontCombo(UINT uiID, int iImage,
						int nFontType = DEVICE_FONTTYPE | RASTER_FONTTYPE | TRUETYPE_FONTTYPE,
						BYTE nCharSet = DEFAULT_CHARSET,
						DWORD dwStyle = CBS_DROPDOWN, int iWidth = 0,
						BYTE nPitchAndFamily = DEFAULT_PITCH) :
		CMFCToolBarFontComboBox(uiID, iImage, nFontType, nCharSet, dwStyle, iWidth, nPitchAndFamily)
	{
	}
#else
	CHexEditFontCombo(UINT uiID, int iImage,
						int nFontType = DEVICE_FONTTYPE | RASTER_FONTTYPE | TRUETYPE_FONTTYPE,
						BYTE nCharSet = DEFAULT_CHARSET,
						DWORD dwStyle = CBS_DROPDOWN, int iWidth = 0,
						BYTE nPitchAndFamily = DEFAULT_PITCH) :
		CMFCToolBarFontComboBox()
	{
		// The defaults for nFontType, nCharSet, nPitchAndFamily are OK so do nothing with those
		m_nID = uiID;
		SetImage(iImage);
		m_dwStyle = dwStyle | WS_CHILD | WS_VISIBLE | WS_VSCROLL;
		m_iWidth = iWidth;
		RebuildFonts();
		SetContext();
	}
#endif

#if 0  // This is not necessary as the first time OnUpdateFont is called the charset will be fixed
	// This is needed when the base class changes the charset (eg after deserializing) and we have to keep
	// our saved copy up to date
	static void SaveCharSet()
	{
		CObList listButtons;
		POSITION posCombo;

		if (CMFCToolBar::GetCommandButtons(ID_FONTNAME, listButtons) > 0 &&
			(posCombo = listButtons.GetHeadPosition()) != NULL)
		{
			CHexEditFontCombo* pCombo = 
				DYNAMIC_DOWNCAST(CHexEditFontCombo, listButtons.GetNext(posCombo));
			saved_charset = pCombo->m_nCharSet;
		}
	}
#endif
	// We use this instead of the base class CMFCToolBarFontComboBox::RebuildFonts
	// which can be very slow due to enumerating printer fonts
	void RebuildFonts()
	{
		CObList& lstFonts = m_pLstFontsExternal != NULL ? *m_pLstFontsExternal : m_lstFonts;
		ClearFonts();

		// First, take the screen fonts:
		CWindowDC dc(NULL);

		LOGFONT lf;
		memset(&lf, 0, sizeof(LOGFONT));
		lf.lfCharSet = m_nCharSet;

		::EnumFontFamiliesEx(dc.GetSafeHdc(), &lf, (FONTENUMPROC) EnumFamScreenCallBackEx, (LPARAM) this, NULL);
	}

	// Is this control consistent with the static font list
	bool OKToRead()
	{
		return m_nCharSet == saved_charset;
	}

	// Required when we change between ANSI and OEM fonts
	void FixFontList(BYTE nCharSet)
	{
		bool fix_list = m_nCharSet != nCharSet;

		// Since the font list is static we only want to rebuild it once when charset changes
		if (saved_charset != nCharSet)
		{
			ClearFonts();
			m_nCharSet = nCharSet;
			saved_charset = nCharSet;
			RebuildFonts();     // note that this can be slow especially in debug builds and you have lots of fonts
			fix_list = true;
		}
		if (fix_list)
		{
			m_nCharSet = nCharSet;
			RemoveAllItems();
			SetContext();
		}
	}
#ifdef _DEBUG
	void Check()
	{
		POSITION pos;
		TRACE("***** %p %p\n", &m_lstItemData, &m_lstItems);
		TRACE("***** %d %d\n", m_lstItemData.GetCount(), m_lstItems.GetCount());

		for (pos = m_lstItems.GetHeadPosition(); pos != NULL;)
		{
			CString strItem = m_lstItems.GetNext (pos);
			TRACE("*** NAME %s\n", strItem);
		}
		for (pos = m_lstItemData.GetHeadPosition(); pos != NULL;)
		{
			CMFCFontInfo* pDesc = (CMFCFontInfo*) m_lstItemData.GetNext (pos);
			TRACE("*** name %s\n", pDesc->m_strName);
		}
	}
#endif
};

// We just need this to store the last font name used
class CHexEditFontSizeCombo : public CMFCToolBarFontSizeComboBox
{
	DECLARE_SERIAL(CHexEditFontSizeCombo)
protected:
	CHexEditFontSizeCombo() : CMFCToolBarFontSizeComboBox() {}
public:
	CHexEditFontSizeCombo(UINT uiID, int iImage, DWORD dwStyle = CBS_DROPDOWN, int iWidth = 0) :
		CMFCToolBarFontSizeComboBox(uiID, iImage, dwStyle, iWidth)
	{
	}

	CString font_name_;                  // last font we got sizes for
};
