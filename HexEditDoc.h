// HexEditDoc.h : interface of the CHexEditDoc class
//
// For the implementation of the CHexEditDoc class see: HexEditDoc.cpp,
// DocData.cpp, BGSearch.cpp, BGAerial.cpp, Template.cpp
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#ifndef HEXEDITDOC_INCLUDED
#define HEXEDITDOC_INCLUDED  1

/////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <deque>
#include <list>
#include <set>
#include <algorithm>
#include <afxmt.h>              // For MFC IPC (CEvent etc)

#include "CFile64.h"
#include <FreeImage.h>
#include "xmltree.h"
#include "expr.h"
#include "timer.h"

using namespace std;

// This enum is for the different modification types that can be made
// to the document.  It is used for keeping track of changes made in the
// undo array and for passing info about changes made to views.
enum mod_type
{
	mod_unknown = '0',          // Helps bug detection
	mod_insert  = 'I',          // Bytes inserted
	mod_replace = 'R',          // Bytes replaced (overtyped)
	mod_delforw = 'D',          // Bytes deleted (using DEL)
	mod_delback = 'B',          // Bytes deleted (using back space)
	mod_repback = '<',          // Replace back (BS in overtype mode)
	mod_insert_file = 'F',      // Bytes inserted - requires index into data_file_[]
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used by the view to tell what parts of its display
// (if any) need to be updated.
class CHexHint : public CObject
{
public:
	enum mod_type utype;        // Have bytes been inserted, deleted or replaced?
	FILE_ADDRESS len;           // How many bytes were "         "    "    "
	FILE_ADDRESS address;       // Address in the file that change started

	CView *pview;               // The view that caused the change
	int index;                  // Index into the document undo array
	BOOL is_undo;               // True if undoing
	BOOL ptoo;                  // Merge with previous op on undo stack (if not undoing)

	// Default and proper constructor
	CHexHint() { utype = mod_unknown; }
	CHexHint(enum mod_type u, FILE_ADDRESS n, FILE_ADDRESS a, CView *v, int i, BOOL f = FALSE, BOOL p = FALSE)
	{
		ASSERT(u == mod_insert  || u == mod_insert_file || u == mod_replace ||
			   u == mod_delforw || u == mod_delback     || u == mod_repback);
		utype = u; len = n; address = a; pview = v; index = i; is_undo = f; ptoo = p;
	}

protected:
	DECLARE_DYNAMIC(CHexHint)   // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to inform the view to undo all view changes
// (moves etc) back to the last doc undo.
class CUndoHint : public CObject
{
public:
	CView *pview;               // The view that caused the change
	int index;                  // Index into the document undo array

	// Default and proper constructor
	CUndoHint() { index = -1; }
	CUndoHint(CView *v, int ii)
	{
		pview = v; index = ii;
	}

protected:
	DECLARE_DYNAMIC(CUndoHint)  // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to inform the view to remove all undo info up to
// the last doc undo, probably because the file has been saved.
class CRemoveHint : public CObject
{
public:
	int index;                          // Index of last doc undo
	CRemoveHint(int ii = -1) { index = ii; }

protected:
	DECLARE_DYNAMIC(CRemoveHint)        // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to tell the view to update its found string
// display.  It is called when a background search is started (to turn
// off display of found strings) and finished (to display the new strings).
class CBGSearchHint : public CObject
{
public:
	BOOL finished_;                     // FALSE = clear all search string occurrences
										// TRUE = get new occurrences from doc and fix current display
										// -1 = remove range of address in start_:end_ but keep rest
	FILE_ADDRESS start_, end_;          // range of string occurrence addresses that need to be removed
	CBGSearchHint(BOOL bb = TRUE) { finished_ = bb; start_ = end_ = -1; }
	CBGSearchHint(FILE_ADDRESS ss, FILE_ADDRESS ee) { finished_ = -1; start_ = ss; end_ = ee; }

protected:
	DECLARE_DYNAMIC(CBGSearchHint)        // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint parameter
// to say that the bg scan has finished.  CAerialView uses it to refresh its display.
class CBGAerialHint : public CObject
{
public:

protected:
	DECLARE_DYNAMIC(CBGAerialHint)        // Required for MFC run-time type info.
};

//// This object is passed to view OnUpdate() functions as the (3rd) hint
//// parameter.  It is used to tell the tree views that the document has changed
//class CRefreshHint : public CObject
//{
//public:
//
//protected:
//    DECLARE_DYNAMIC(CRefreshHint)          // Required for MFC run-time type info.
//};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to tell the tree views of the document that
// the data format files descripiton (XML file) has changed.
class CDFFDHint : public CObject
{
public:

protected:
	DECLARE_DYNAMIC(CDFFDHint)          // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to tell the tree views to save their tree state.
class CSaveStateHint : public CObject
{
public:

protected:
	DECLARE_DYNAMIC(CSaveStateHint)          // Required for MFC run-time type info.
};

// This object is passed to view OnUpdate() functions as the (3rd) hint
// parameter.  It is used to tell the tree views to restore their tree state.
class CRestoreStateHint : public CObject
{
public:

protected:
	DECLARE_DYNAMIC(CRestoreStateHint)          // Required for MFC run-time type info.
};

// Passed to view OnUpdate() functions when a bookmark is turned on/off.
class CBookmarkHint : public CObject
{
public:
	CBookmarkHint(FILE_ADDRESS aa) : addr_(aa) { }
	FILE_ADDRESS addr_;                 // The location of the new/deleted bookmark

protected:
	DECLARE_DYNAMIC(CBookmarkHint)      // Required for MFC run-time type info.
};

class CTrackHint : public CObject
{
public:
	FILE_ADDRESS start_, end_;          // range that needs redrawing
	CTrackHint(FILE_ADDRESS ss, FILE_ADDRESS ee) { start_ = ss; end_ = ee; }

protected:
	DECLARE_DYNAMIC(CTrackHint)        // Required for MFC run-time type info.
};


// These structures used to be declared within class CHexEditDoc but with
// VC++ 5 you get errors when used in <vector> and <list>

// These structures are used to easily access the current doc data
struct doc_loc
{
	static const FILE_ADDRESS mask;      // masks off the top 2 bits of dlen
	static const FILE_ADDRESS fmask;     // masks off the top bits of fileaddr
	static const int max_data_files;     // must match fmask (eg if fmask removes 3 bits should be <= 8)

	// Location type is now stored in the top 2 bits of dlen (0=unknown, 1=orig file, 2=memory, 3=other file)
	unsigned __int64 dlen;               // Data block len - needs to be as big as a file can be
	union
	{
		// If location type == 3 then the top bits store the file number (index into data_file_ etc)
		FILE_ADDRESS fileaddr;  // File location (if file) + top 2 bits = data_file_ idx if location == 3
		unsigned char *memaddr; // Ptr to data (if mem)
	};
	doc_loc(FILE_ADDRESS f, unsigned __int64 n)
	{
//        location = loc_file;
		ASSERT(FILE_ADDRESS(n) <= mask);
		dlen = n | (unsigned __int64(1) << 62);  // 1 = orig file
		fileaddr = f;
	}
	doc_loc(unsigned char *m, unsigned __int64 n)
	{
//        location = loc_mem;
		ASSERT(FILE_ADDRESS(n) <= mask);
		dlen = n | (unsigned __int64(2) << 62); // 2 = memory
		memaddr = m;
	}
	doc_loc(FILE_ADDRESS f, unsigned __int64 n, int idx)
	{
		ASSERT(FILE_ADDRESS(n) <= mask);
		ASSERT(FILE_ADDRESS(f) <= fmask);
		ASSERT(idx >= 0 && idx < max_data_files);
		dlen = n | (unsigned __int64(3) << 62);  // 3 = data file
		fileaddr = f | (FILE_ADDRESS(idx) << 62);  // Note: must change when fmask/max_data_files change
	}

private:
	doc_loc();                          // Default constructor (not used)
};

// These structures are used to keep track of all changes made to the doc
struct doc_undo
{
	// static const size_t limit; // replaced with theApp.undo_limit_
	enum mod_type utype;                // Type of modification made to file
	union
	{
		unsigned char *ptr;             // NULL if utype is del else new data
		int idx;						// date_file_[] index if utype == mod_insert_file
	};
	FILE_ADDRESS address;               // Address in file of start of mod
	FILE_ADDRESS len;                   // Length of mod

	// Normal constructor
	doc_undo(mod_type u, FILE_ADDRESS a, FILE_ADDRESS n, unsigned char *p = NULL, int i = -1)
	{
		ASSERT(u == mod_insert  || u == mod_insert_file || u == mod_replace ||
			   u == mod_delforw || u == mod_delback     || u == mod_repback);

		utype = u; len = n; address = a;
		if (utype == mod_insert_file)
		{
			ASSERT(i >= 0 && i < doc_loc::max_data_files);
			idx = i;
		}
		else if (p != NULL)
		{
			ASSERT(len < 0x100000000);
			ptr = new unsigned char[max(theApp.undo_limit_, int(len))];
			memcpy(ptr, p, size_t(len));
		}
		else
		{
			ASSERT(utype == mod_delforw || utype == mod_delback);
			ptr = NULL;
		}
	}
	// Copy constructor
	doc_undo(const doc_undo &from)
	{
		ASSERT(from.utype != mod_unknown);
		utype = from.utype;
		len = from.len;
		address = from.address;
		if (utype == mod_insert_file)
		{
			ASSERT(from.idx >= 0 && from.idx < doc_loc::max_data_files);
			idx = from.idx;
		}
		else if (from.ptr != NULL)
		{
			ASSERT(len < 0x100000000);
			ptr = new unsigned char[max(theApp.undo_limit_, int(len))];
			memcpy(ptr, from.ptr, size_t(len));
		}
		else
			ptr = NULL;
	}
	// Copy assignment operator
	doc_undo &operator=(const doc_undo &from)
	{
		if (&from != this)
		{
			ASSERT(from.utype != mod_unknown);
			if (ptr != NULL)
				delete[] ptr;

			utype = from.utype;
			len = from.len;
			address = from.address;

			if (utype == mod_insert_file)
			{
				ASSERT(from.idx >= 0 && from.idx < doc_loc::max_data_files);
				idx = from.idx;
			}
			else if (from.ptr != NULL)
			{
				ASSERT(len < 0x100000000);
				ptr = new unsigned char[max(theApp.undo_limit_, int(len))];
				memcpy(ptr, from.ptr, size_t(len));
			}
			else
				ptr = NULL;
		}
		return *this;
	}
	~doc_undo()
	{
		ASSERT(utype != mod_unknown);
		if (utype != mod_insert_file && ptr != NULL)
			delete[] ptr;
	}

	// vector requires a default constructor (even if not used)
//    doc_undo() { utype = mod_unknown; }
//    operator==(const doc_undo &) const { return false; }
//    operator<(const doc_undo &) const { return false; }
};

struct adjustment
{
	adjustment(FILE_ADDRESS ss, FILE_ADDRESS ee, FILE_ADDRESS address, FILE_ADDRESS aa)
	{ start_ = ss; end_ = ee; address_ = address; adjust_ = aa; }
	FILE_ADDRESS start_, end_;
	FILE_ADDRESS address_;
	FILE_ADDRESS adjust_;
};

// Class derived from expr_eval so we can supply our own symbols (by overriding find_symbol)
// in the owning CHexEditDoc class.  Used for template expressions.
class CHexExpr : public expr_eval
{
public:
	CHexExpr(CHexEditDoc *pp) { pdoc = pp; }
	virtual value_t find_symbol(const char *sym, value_t parent, size_t index, int *pac,
								__int64 &sym_size, __int64 &sym_address, CString &sym_str);
	CHexExpr::value_t get_value(int ii, __int64 &sym_size, __int64 &sym_address);

private:
	BOOL sym_found(const char * sym, int ii, CHexExpr::value_t &val, int *pac,
				   __int64 &sym_size, __int64 &sym_address);

	CHexEditDoc *pdoc;
};

class CHexEditDoc : public CDocument
{
	friend class CGridCtrl2;
	friend class CHexEditView;
	friend class CDataFormatView;
	friend class CAerialView;
	friend class CHexExpr;

protected: // create from serialization only
		CHexEditDoc();
		DECLARE_DYNCREATE(CHexEditDoc)

public:
// Attributes
	CFile64 *pfile1_;
	FILE_ADDRESS length() const { return length_; }
	BOOL read_only() { return readonly_; }
	//BOOL keep_times() { return keep_times_; }
	int doc_flags() { return (keep_times_ ? 1 : 0) | (dffd_edit_mode_ ? 2 : 0); }
	BOOL readonly_;
	BOOL shared_;

	timer view_time_, edit_time_;     // Track total time file is open for view (ie, read only) or edit

	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *Replacements()
	{
		if (need_change_track_) rebuild_change_tracking();
		return &replace_pair_;
	}
	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *Insertions()
	{
		if (need_change_track_) rebuild_change_tracking();
		return &insert_pair_;
	}
	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *Deletions()
	{
		if (need_change_track_) rebuild_change_tracking();
		return &delete_pair_;
	}

	const char *why0() const
	{
		// Return reason (as text) that length is zero
		if (length_ == 0)
			return "Empty file";
		else
		{
			ASSERT(0);
			return "Internal error";
		}
	}
	CString GetType() { return strTypeName_; }
	CString GetFileName() { if (pfile1_ == NULL) return ""; else return pfile1_->GetFileName(); }

	BOOL IsDevice() const { return dynamic_cast<CFileNC *>(pfile1_) != NULL; }
	DWORD GetSectorSize() const { if (pfile1_ == NULL) return 0; else return pfile1_->SectorSize(); }
	BOOL HasSectorErrors() const;
	DWORD SectorError(FILE_ADDRESS sec) const;

	CHexEditView *GetBestView();
	void AddBookmark(int index, FILE_ADDRESS pos);
	void RemoveBookmark(int index);
	void DeleteBookmarkList();
	void ClearBookmarks();
	int GetBookmarkAt(FILE_ADDRESS);  // Returns bookmark index or -1 if no bookmark there
	FILE_ADDRESS GetBookmarkPos(int index)
	{
		std::vector<int>::const_iterator pp = std::find(bm_index_.begin(), bm_index_.end(), index);

		if (pp == bm_index_.end())
			return -1;
		else
			return bm_posn_[pp - bm_index_.begin()];
	}
	std::vector<int> bm_index_;             // Handle for all bookmarks in this file
	std::vector<FILE_ADDRESS> bm_posn_;     // File positions of the bookmarks

	CXmlTree *ptree_;
	int xml_file_num_;                      // Index into theApp.xml_file_name_ of current XML file or -1

// Operations
	size_t GetData(unsigned char *buf, size_t len, FILE_ADDRESS loc, int use_bg = -1);
	BOOL WriteData(const CString fname, FILE_ADDRESS start, FILE_ADDRESS end, BOOL append = FALSE);
	void WriteInPlace();
	void Change(enum mod_type, FILE_ADDRESS address, FILE_ADDRESS len,
				unsigned char *buf, int, CView *pview, BOOL ptoo=FALSE);
	BOOL Undo(CView *pview, int index, BOOL same_view);

	int AddDataFile(LPCTSTR name, BOOL temp = FALSE); // returns index where file is or -1 if no more slots available
	BOOL DataFileSlotFree()
	{
		for (int ii = 0; ii < doc_loc::max_data_files; ++ii)
			if (data_file_[ii] == NULL)
				return TRUE;  // found a free slot
		return FALSE;
	}
	void RemoveDataFile(int idx);                     // frees a slot when file no longer used (and deletes temp file)

	HICON GetIcon() { return hicon_; }

	FILE_ADDRESS insert_block(FILE_ADDRESS addr, __int64, const char *data_str, CView *pv = NULL);

public:
// Overrides
		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CHexEditDoc)
		public:
		virtual BOOL OnNewDocument();
		virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
		virtual void OnCloseDocument();
		virtual void DeleteContents();
		virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
		//}}AFX_VIRTUAL

		virtual void SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU = TRUE);
		virtual void SetModifiedFlag(BOOL bMod = TRUE);
		virtual BOOL SaveModified();

public:
		afx_msg void OnFileClose();
		afx_msg void OnFileSave();
		afx_msg void OnFileSaveAs();
		virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace = TRUE);

// Implementation
public:
		virtual ~CHexEditDoc();
#ifdef _DEBUG
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
		//{{AFX_MSG(CHexEditDoc)
	afx_msg void OnDocTest();
	afx_msg void OnKeepTimes();
	afx_msg void OnUpdateKeepTimes(CCmdUI* pCmdUI);
	afx_msg void OnDffdRefresh();
	afx_msg void OnUpdateDffdRefresh(CCmdUI* pCmdUI);
	//}}AFX_MSG
	afx_msg void OnDffdNew();            // Template/New - open default template
	afx_msg void OnDffdOpen(UINT nID);
	afx_msg void OnDffdSave();
	afx_msg void OnDffdSaveAs();
	afx_msg void OnUpdateDffdSave(CCmdUI* pCmdUI);
	afx_msg void OnEditMode();
	afx_msg void OnUpdateEditMode(CCmdUI* pCmdUI);
	afx_msg void OnDffdOptions();
	afx_msg void OnUpdateDffdOptions(CCmdUI* pCmdUI);

	afx_msg void OnTest();
		DECLARE_MESSAGE_MAP()

public:  // These really should be private but are called from SendEmail
	BOOL open_file(LPCTSTR lpszPathName); // Open or reopen file_
	void close_file();

	BOOL DffdEditMode();
	void SetDffdEditMode(BOOL b) { dffd_edit_mode_ = b; }

private:
// Private member functions
	void regenerate();      // Rebuild loc_ list from undo_ array
	BOOL only_over();       // Check if file can be saved in place

	bool ask_insert();      // Allow the user to insert a block
	void fill_rand(char *buf, size_t len, range_set<int> &rr);

	// The following are used to modify the locations list (loc_)
	typedef std::vector <doc_undo>::const_iterator pundo_t;
	typedef std::list <doc_loc>::iterator ploc_t;
	void loc_add(pundo_t pu, FILE_ADDRESS &pos, ploc_t &pl);
	void loc_del(FILE_ADDRESS address, FILE_ADDRESS len, FILE_ADDRESS &pos, ploc_t &pl);
	void loc_split(FILE_ADDRESS address, FILE_ADDRESS pos, ploc_t pl);

	// We allow up to 4 external files to hold some of the file data (if too big for memory)
	CFile64 *data_file_[4 /*doc_loc::max_data_files*/];     // Ptrs to files or NULL if not (yet) used
	CFile64 *data_file2_[4 /*doc_loc::max_data_files*/];    // Ptrs to dupes for use by background search thread
	CFile64 *data_file3_[4 /*doc_loc::max_data_files*/];    // Ptrs to dupes for use by background aerial thread
	BOOL temp_file_[4 /*doc_loc::max_data_files*/];         // Says if the file is temporary (should be deleted when doc closed)

	// The following are used for change tracking
	bool need_change_track_;               // Do change tracking structures need rebuilding
	void rebuild_change_tracking();        // Rebuilds replace_pair_, insert_pair_, delete_pair_
	void send_change_hint(FILE_ADDRESS address); // Invalidate change tracking areas of display
	int base_type_;  // Determines what we compare against 0=orig file, 1=mem blk, 2=temp file

	// The following pairs store all the replacement, insertion and deletion points
	// for change tracking.  The first of the pair is the address, the 2nd is the length.
	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > replace_pair_;
	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > insert_pair_;
	std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > delete_pair_;

	void load_icon(LPCTSTR lpszPathName); // Load icon based on file ext. into hicon_
	void show_icon();           // Show icon in child frame windows of views

private:
	HICON hicon_;               // Icon for child frame windows
	CString strTypeName_;       // Type of file (from registry)

	void GetFileStatus();
	void SetFileStatus(LPCTSTR lpszPathName);
	BOOL keep_times_;           // Do we keep same times & attributes of orig file?
	CFileStatus saved_status_;  // Times and attributes of file when opened
	int dffd_edit_mode_;        // 0 = false, 1 = true, -1 = unknown

	FILE_ADDRESS length_;

	CView *last_view_;          // Last view that caused change to document

	// Array of changes made to file (last change at end)
	std::vector <doc_undo> undo_;

	// List of locations of where to find doc data (disk file/memory)
	std::list <doc_loc> loc_;

public:
	void CheckBGProcessing();   // check if bg searching or bg scan has finished

	// Background search
	void CreateSearchThread();  // Create background search thread
	void KillSearchThread();    // Kill background thread ASAP
	UINT RunSearchThread();     // Main func in bg thread
	void SearchThreadPriority(int ii); // Set bg thread priority
	void StartSearch(FILE_ADDRESS start = -1, FILE_ADDRESS end = -1);
	void StopSearch();          // Stops any current search (via stop_event_), thread waits for start_search_event_
	int SearchOccurrences();    // No of search occurrences (-ve if disabled/not finished)
	FILE_ADDRESS GetNextFound(const unsigned char *pat, const unsigned char *mask, size_t len, 
							  BOOL icase, int tt, BOOL wholeword,
							  int alignment, int offset, bool align_rel, FILE_ADDRESS base_addr,
							  FILE_ADDRESS from);
	FILE_ADDRESS GetPrevFound(const unsigned char *pat, const unsigned char *mask, size_t len,
							  BOOL icase, int tt, BOOL wholeword,
							  int alignment, int offset, bool align_rel, FILE_ADDRESS base_addr,
							  FILE_ADDRESS from);
	std::vector<FILE_ADDRESS> SearchAddresses(FILE_ADDRESS start, FILE_ADDRESS end);
	int SearchProgress(int &occurrences);  // How far are we through the background search now (0 to 100)

	FILE_ADDRESS base_addr_;    // Base address for alignment tests. It is not stored in app (with alignment_ et al as it is per doc - set from mark or SOF in active view)

	// Background scan (for aerial views etc)
	void AddAerialView(CHexEditView *pview);
	void RemoveAerialView();
	void CreateAerialThread();  // Create background thread which fills in the aerial view bitmap
	void KillAerialThread();    // Kill background thread ASAP
	void AerialChange(CHexEditView *pview = NULL);  // Signal bg thread to re-scan
	UINT RunAerialThread();     // Main func in bg thread
	int AerialProgress();       // 0 to 100 (or -1 if not scanning)


	// DFFD stuff
	enum
	{
		DF_FILE,            // The whole file (only one at indent 1)
		DF_STRUCT,          // Contains different types of objects
		DF_DEFINE_STRUCT,   // Definition of a struct (without actually using it)
		DF_USE_STRUCT,      // Use of above defined struct
		DF_SWITCH,          // Contains a single object selected from a case list based on an int expression
		DF_FORV,            // Contains objects that vary in size and/or the length is not known at the start (ie uses stop_test)
		DF_FORF,            // Contains same objects that are fixed in size and there is no stop_test
		DF_IF,              // Contains an optional object based on a boolean expression
		DF_JUMP,            // Jump (and return) to different part of the file
		DF_UNION,           // Not used yet (probably never)

		DF_LEAF1,           // All below here are leaves of the tree (ie no sub-nodes)
		DF_EVAL = DF_LEAF1, // Evaluate an expression
		DF_MORE,            // Bytes at end of DF_FORF array past the 1st max_fix_for_elts_ elements
		DF_EXTRA,           // Bytes past the last expected data (but before real EOF)

		// All below here are data
		DF_DATA = 16,       // All past this point are data objects (must be < others)
		DF_NO_TYPE,         // No specific data type (used for fill and BLOBs)

		DF_STRING,          // Generic (1 byte) character
		DF_STRINGA,         // ASCII char (7 bit)
		DF_STRINGN,         // ANSI char (8 bit)
		DF_STRINGO,         // OEM/IBM char (ASCII + extras)
		DF_STRINGE,         // EBCDIC char (8 bit)
		DF_WSTRING,         // Unicode char (16 bit)

		// integer types
		DF_CHAR,            // Generic (1 byte) character
		DF_CHARA,           // ASCII char (7 bit)
		DF_CHARN,           // ANSI char (8 bit)
		DF_CHARO,           // OEM/IBM char (ASCII + extras)
		DF_CHARE,           // EBCDIC char (8 bit)
		DF_WCHAR,           // Unicode char (16 bit)

		DF_INT8,            // Signed ints (2's complement)
		DF_INT16,
		DF_INT32,
		DF_INT64,
		DF_UINT8,           // Unsigned ints
		DF_UINT16,
		DF_UINT32,
		DF_UINT64,
		DF_MINT8,           // Signed ints (sign and magnitude)
		DF_MINT16,
		DF_MINT32,
		DF_MINT64,
		DF_BITFIELD8,       // Bitfields (with underlying storage unit size)
		DF_BITFIELD16,
		DF_BITFIELD32,
		DF_BITFIELD64,

		DF_LAST_INT,

		// floating point types
		DF_REAL32 = DF_LAST_INT, // IEEE floating point numbers
		DF_REAL64,
		DF_IBMREAL32,       // IBM floating point numbers
		DF_IBMREAL64,
		DF_REAL48,          // Real as in Turbo Pascal and Real48 of Delphi

		DF_LAST_NUM,

		// date types
		DF_DATEC = DF_LAST_NUM, // 32 bit int = secs since 1/1/1970 (most common time_t)
		DF_DATEC51,         // 32 bit int = secs since 1/1/1980 (MSC 5.1 time_t)
		DF_DATEC7,          // unsigned 32 bit int = secs since 31/12/1899 (MSC 7)
		DF_DATECMIN,        // 32 bit int = minutes 1/1/1970 (some UNIX compiler time_t?)
		DF_DATEOLE,         // OLE DATE (IEEE 64 bit value = days since 31/12/1989)
		DF_DATESYSTEMTIME,  // Windows SYSTEMTIME structure
		DF_DATEFILETIME,    // Windows FILETIME (64 bit int = 100 nSecs since 1/1/1601)
		DF_DATEMSDOS,       // bit-field used for MSDOS file system times (resn = 2secs)
		DF_DATEC64,         // New 64 bit CRT type (time64_t)

		DF_LAST_DATE,

		DF_LAST = DF_LAST_DATE
	};

	void OpenDataFormatFile(LPCTSTR data_file_name = NULL);
	void CheckSaveTemplate();
	CString GetXMLFileDesc() { return xml_file_num_ == -1 ? CString("") : theApp.xml_file_name_[xml_file_num_]; }
	CString GetFormatFileName() { return xml_file_num_ == -1 ? CString("") : theApp.xml_file_name_[xml_file_num_]; }
//    std::vector<CString> const &XMLFileList() const { return theApp.xml_file_name_; }
	BOOL ScanInit();
	BOOL ScanFile();
	void CheckUpdate();

	int GetBpe() { return bpe_; }
	int NumElts() { return int((length_ - 1)/bpe_) + 1; }    // Number of elts required for the whole file

private:
	void HandleError(const char *mess);

	// Background (worker) threads are created for several purposes (bg searches, bg scan for aerial view, etc)
	// When the thread starts up it goes into an initial wait state ready for it's event to be
	// fired so it can start processing.  Once finished processing it does not terminate but goes
	// back into a wait state ready to start more processing.
	// Communication with threads is done by sending them commands (and firing their event when
	// they are waiting).  Commands are:
	//   RESTART = start or restart scanning
	//   STOP = stop scanning and return to wait state
	//   DIE = tell thread to terminate itself
	//   NONE = set to this when there is no new command
	// The thread can be in one of these states:
	//   STARTING = thread is starting up and is yet to reach initial WAITING state
	//   WAITING = blocked waiting for event to be triggered
	//   SCANNING = currently processing (but regularly checking for a new command)
	//   DYING = terminating after receiving DIE command
	enum BG_COMMAND { NONE, STOP, DIE };           // Commands sent to background thread
	enum BG_STATE   { STARTING, WAITING, SCANNING, DYING }; // State of background thread

	// -------------- Background searches (see BGsearch.cpp) --------------
	CWinThread *pthread2_;      // Ptr to background search thread or NULL if bg searches are off
	int search_priority_;       // Priority level to do bg search at

	CEvent start_search_event_; // Signal to bg thread to start a new search, or check for termination

	mutable CCriticalSection docdata_;  // Protects access in threads to document data (loc_) file and find data below.
								// Note that this is used for read access of the document from the bg
								// threads or write (but not read) access from the primary thread. This
								// stops the bg thread accessing the data while it is being updated.
	CFile64 *pfile2_;           // We need a copy of file_ so we can access the same file for searching
	// Also see data_file2_ (above)
	enum BG_COMMAND search_command_; // signals search thread to do something
	enum BG_STATE   search_state_;   // indicates what the search thread is doing
	bool search_fin_;           // Flags that the bg search is finished and the view need updating
	unsigned char *search_buf_; // Buffer for holding file data to search

	// List of ranges to search in background (first = start, second = byte past end)
	std::list<pair<FILE_ADDRESS, FILE_ADDRESS> > to_search_;
	std::set<FILE_ADDRESS> found_;      // Addresses where current search text was found
	// List of adjustments pending due to insertions/deletions (first = address, second = adjustment amount)
	std::list<adjustment> to_adjust_;

	FILE_ADDRESS find_total_;   // Total number of bytes for background search (so that progress bar is drawn properly)
	double find_done_;          // This is how far the bg search has progressed in searching the top entry of to_search_ list
	void FixFound(FILE_ADDRESS start, FILE_ADDRESS end, FILE_ADDRESS address, FILE_ADDRESS adjust);
	bool SearchProcessStop();   // Check if the scanning should stop

	// ------------- aerial view (see BGaerial.cpp) -------------------
	CWinThread *pthread3_;      // Ptr to thread or NULL
	CEvent start_aerial_event_; // Starts the thread going
	enum BG_COMMAND aerial_command_;
	enum BG_STATE   aerial_state_;
	bool aerial_fin_;           // Flags that the bg scan is finished and the view needs updating
	unsigned char *aerial_buf_; // Buffer used for holding file data for scan

	FILE_ADDRESS aerial_addr_;  // Current address we are processing (used to show progress)

	// NOTE: kala must not be modified while the bg thread is running!
	COLORREF kala_[256];        // Colours from the first hex view to open the aerial view

	CFile64 *pfile3_;           // Using a copy of the file avoids synchronising access problems
	// Also see data_file3_ (above)
	int av_count_;              // Number of aerial views of this document
	int bpe_;                   // Bytes per bitmap pixel (1 to 65536)
	FIBITMAP *dib_;             // The FreeImage bitmap
	int dib_size_;              // Actual number of bytes allocated

	// MAX_WIDTH = widest we can "reshape" the bitmap to.  Like any width used for the bitmap it must
	// be a multiple of 8 (so there are never "pad" bytes on the end of scan lines).
	enum { MAX_WIDTH = 2048 };
	enum { MAX_BMP  = 256*1024*1024 };      // Biggest bitmap size in bytes - should be made a user option sometime

	void GetAerialBitmap(int clear = 0xC0); // Check if bitmap has been allocated/is big enough and get it if not
	bool AerialProcessStop();   // Check if the scanning should stop

	// -------------- template (DFFD) (see Template.cpp) ----------------
	// Each df_size_ gives the size of a data field or whole array/structure.  If -ve take abs value.
	// Each df_address_ gives the location within the file.  If -1, all or part is not present.
	// The following situations are handled specially:
	//  - a domain error (ie, incorrect value) is indicated by a -ve df_size_ (df_address_ as normal)
	//  - a non-existent element has a zero df_size_ and the same df_address_ as the following element
	//    (this occurs for a "for" where count is zero (one dummy elt is displayed) and "if" where test is false)
	//  - if EOF is encountered prematurely then non-existent elts are marked with an df_address_ of -1
	//    (sizes are normal except where EOF is used to find the size, in which case they are zero)
	// When displayed any element that has a -ve size or an address of -1 is shown in the tree on open -- this
	//   is used to show errors in the file (domain errors and premature EOF).

	int add_branch(CXmlTree::CElt parent, FILE_ADDRESS addr, unsigned char ind, CHexExpr &ee,
				   FILE_ADDRESS &returned_size, int child_num = -1, bool ok_bitfield_at_end = false);
	CHexExpr::value_t Evaluate(CString ss, CHexExpr &ee, int ref, int &ref_ac);

	BOOL df_init_;
	CString df_mess_;                       // Error message
	clock_t m_last_checked;                 // Used in updating scan progress
	bool update_needed_;                    // Is tree out of sync with data (needs rebuild)?

	CString default_byte_order_, default_read_only_, default_char_set_;

	std::vector<signed char> df_type_;      // struct, for, if, data types (-ve => little-endian?)
	std::vector<FILE_ADDRESS> df_size_;     // Size of data elt or whole branch (-ve => invalid)
	std::vector<FILE_ADDRESS> df_address_;  // Location in file of data elt (-1 => not present)
	std::vector<size_t> df_extra_;          // Misc field with several uses:
											// 1. Number of elements (DF_FORV)
											// 2. Number of displayed elts (DF_FORF)
											// 3. Number of extra elts not displayed (DF_MORE)
											// 4. String terminator (DF_STRING to DF_WSTRING)
											// 5. Bifield (bottom) bit number (low byte) and bits (2nd byte)

	std::vector<CXmlTree::CElt> df_elt_;    // Node of CXmlTree that this element is for
	std::vector<ExprStringType> df_info_;   // Info for user ("expr" for STRUCT, jump address for JUMP, etc)
	std::vector<unsigned char> df_indent_;  // Use in CTreeColumn (1=root 2=branch off root etc)
	unsigned char max_indent_;              // The largest value in df_indent_

	int in_jump_;                           // Keep track of nested jumps (we don't update progress bar in JUMPs since address is funny)

	// These are used for keeping track of consecutive bitfields
	FILE_ADDRESS last_size_;                // Storage unit size of previous element if a bitfield (1,2,4, or 8) or 0
	bool last_down_;                        // Direction for previous bitfield (must be same dirn to be put in same stg unit)
	int bits_used_;                         // Bits used by all consec. bitfields so far (0 if previous elt not a bitfield)

	// Storage for enums
	typedef std::map<__int64, CString> enum_t; // One enum: maps values to names
	std::map<MSXML2::IXMLDOMElementPtr::Interface *, enum_t> df_enum_; // Stores all enums: maps an element to its enum
	bool add_enum(CXmlTree::CElt &ee, LPCTSTR estr); // Returns false if error parsing enum string
	enum_t &get_enum(CXmlTree::CElt &ee);   // Returns ref. to enum for an element
	ExprStringType get_str(CHexExpr::value_t val, int ii);

#ifdef _DEBUG
	void dump_tree();
#endif
};

/////////////////////////////////////////////////////////////////////////////
#endif
