// Misc.h - various global functions
//
// Copyright (c) 2012 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#ifndef MISC_INCLUDED_
#define MISC_INCLUDED_  1

#include <istream>                  // used in << and >>

// Macros
#define BRIGHTNESS(cc) ((GetRValue(cc)*30 + GetGValue(cc)*59 + GetBValue(cc)*11)/256)

#define mac_abs(a)  ((a)<0?-(a):(a))

// Colour manipulation
void get_hls(COLORREF rgb, int &hue, int &lightness, int &saturation);
COLORREF get_rgb(int hue, int luminance, int saturation);
COLORREF tone_down(COLORREF col, COLORREF bg_col, double amt = 0.75);
COLORREF same_hue(COLORREF col, int sat, int lum = -1);
COLORREF add_contrast(COLORREF col, COLORREF bg_col);

// Date/time
double TZDiff();
DATE FromTime_t(__int64 v);
DATE FromTime_t_80(long v);
DATE FromTime_t_mins(long v);
DATE FromTime_t_1899(long v);
bool ConvertToFileTime(time_t tt, FILETIME *ft);

// System utils
BOOL IsUs();     // This is just to handle a few differences in American spelling
void BrowseWeb(UINT id);
CString GetExePath();
BOOL GetDataPath(CString &data_path, int csidl = CSIDL_APPDATA);
CString FileErrorMessage(const CFileException *fe, UINT mode = CFile::modeRead|CFile::modeWrite);
bool AbortKeyPress();

CString get_menu_text(CMenu *pmenu,int id);

void LoadHist(std::vector<CString> & hh, LPCSTR name, size_t smax);
void SaveHist(std::vector<CString> const & hh, LPCSTR name, size_t smax);

// Multiple monitor
bool OutsideMonitor(CRect);
CRect MonitorMouse();
CRect MonitorRect(CRect);
bool NeedsFix(CRect &rect);
// Round to nearest integer allowing for -ve values, halves always go away from zero eg -1.5 => -2.0
inline double fround(double r) { return (r > 0.0) ? floor(r + 0.5) : ceil(r - 0.5); }

// Number conversion/analysis
CString NumScale(double val);
CString	bin_str(__int64 val,int bits);
void AddCommas(CString &str);
void AddSpaces(CString &str);

CString DecimalToString(const void * pdecimal, CString & sMantissa, CString & sExponent);
bool StringToDecimal(const char * ss, void * presult);
#ifdef _DEBUG
void TestDecimalRoutines();
#endif

// The number of significant digits in a number
inline int SigDigits(unsigned __int64 val, int base = 10)
{
	ASSERT(base > 1);
	int retval = 0;
	for (retval = 0; val != 0; ++retval)
		val /= base;
	return retval;
}

bool make_real48(unsigned char pp[6], double val, bool big_endian = false);
double real48(const unsigned char *pp, int *pexp = NULL, long double *pmant = NULL, bool big_endian = false);
bool make_ibm_fp32(unsigned char pp[4], double val, bool little_endian = false);
bool make_ibm_fp64(unsigned char pp[8], double val, bool little_endian = false);
long double ibm_fp32(const unsigned char *pp, int *pexp = NULL,
			 long double *pmant = NULL, bool little_endian = false);
long double ibm_fp64(const unsigned char *pp, int *pexp = NULL,
			 long double *pmant = NULL, bool little_endian = false);

__int64 strtoi64(const char *, int radix = 0);
__int64 strtoi64(const char *, int radix, const char **endptr);

#ifndef _LONGLONG
// This is needed since std streams don't support __int64
inline std::istream &operator>>(std::istream &ss, __int64 &ii)
{
	char buf[22];
	for (char *pp = buf; pp < buf + sizeof(buf) - 1; ++pp)
	{
		if (!(ss >> *pp) || !isdigit(*pp))
		{
			if (ss && *pp != '\0')
				ss.putback(*pp);
			break;
		}
	}
	*pp = '\0';
	ii = ::strtoi64(buf);
//    sscanf(buf, "%I64d", &ii);
	if (!ss && pp > buf)
		ss.clear(ss.rdstate() & ~std::ios_base::failbit); // We actually read something so clear failbit
	else if (ss && pp == buf)
		ss.setstate(std::ios_base::failbit);              // We failed to read so set failbit

	return ss;
}

inline std::ostream &operator<<(std::ostream &ss, const __int64 &ii)
{
	char buf[22];
	sprintf(buf, "%I64d", ii);
	return ss << buf;
}
#endif // #ifndef _LONGLONG

unsigned long str_hash(const char *str);   // hash value from string

// PRNGs
unsigned long rand1();
unsigned long rand2();
void rand_good_seed(unsigned long seed);
unsigned long rand_good();

// All in one CRCs
unsigned short crc_ccitt(const void *buf, size_t len);
unsigned long crc_32(const void *buffer, size_t len);
unsigned short crc_16(const void *buffer, size_t len);  // Boost crc 16
unsigned short crc_xmodem(const void *buf, size_t len);

// Use the following when it's too big to do at once
void * crc_16_init();
void crc_16_update(void * handle, const void *buf, size_t len);
unsigned short crc_16_final(void * handle);

void * crc_32_init();
void crc_32_update(void * handle, const void *buf, size_t len);
unsigned long crc_32_final(void * handle);

void * crc_xmodem_init();
void crc_xmodem_update(void * handle, const void *buf, size_t len);
unsigned short crc_xmodem_final(void * handle);

void * crc_ccitt_init();
void crc_ccitt_update(void * handle, const void *buf, size_t len);
unsigned short crc_ccitt_final(void * handle);

// Memory manipulation
int next_diff(const void * buf1, const void * buf2, size_t len);

// flip_bytes is typically used to switch between big- and little-endian byte order but
// works with any number of bytes (including an odd number whence middle byte not moved)
inline void flip_bytes(unsigned char *pp, size_t count)
{
	unsigned char cc;                   // Temp byte used for swap
	unsigned char *end = pp + count - 1;

	while (pp < end)
	{
		cc = *end;
		*(end--) = *pp;
		*(pp++) = cc;
	}
}

// flip bytes in each consecutive word
// if count is odd the last byte is not moved
inline void flip_each_word(unsigned char *pp, size_t count)
{
	ASSERT(count%2 == 0);               // There should be an even number of bytes
	unsigned char cc;                   // Temp byte used for swap
	for (unsigned char *end = pp + count; pp < end; pp += 2)
	{
		cc = *pp;
		*pp = *(pp+1);
		*(pp+1) = cc;
	}
}

// Low level disk stuff (for disk editor)
inline BOOL IsDevice(LPCTSTR ss) { return _tcsncmp(ss, _T("\\\\.\\"), 4) == 0; }
// 0. \\.\D:                 - volume with drive letter D (works for NT and 9X)
// 1. \\.\PhysicalDriveN     - Nth physical drive (including removeable but not floppies)
// 2. \\.\FloppyN            - Nth floppy drive
// 3. \\.\CdRomN             - Nth CD/DVD drive
// NOTE 1, 2 and 3 are converted internally into device names used in NT native
//      API (eg "\\.\Floppy0" => "\device\Floppy0") because FloppyN type names
//      do not work and CdRomN type names only work under XP.
// 4. \device\               - NO LONGER USED
BSTR GetPhysicalDeviceName(LPCTSTR name); // get device name to use with native API (for 1,2,3 above)
CString DeviceName(CString name);         // get nice display name for device file name
int DeviceVolume(LPCTSTR filename);       // get volume (0=A:, 1=B: etc) of physical device (optical/removeable only) or -1 if none

#endif
