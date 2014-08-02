// Misc.cpp : miscellaneous routines
//
// Copyright (c) 2012 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

/*

MODULE NAME:    NUMSCALE - OLD version see below for new one

Usage:          CString NumScale(double val)

Returns:        A string containing the scaled number (see below)

Parameters:     val = a value to be scaled

Description:    Converts a number to a string which is more easily read.
				At most 5 significant digits are generated (and hence
				there is a loss of precision) to aid readability.  If
				scaling is necessary a metric modifier is appended to
				the string.  Eg:

				0.0 becomes "0 "
				0.1 becomes "0 "
				1.0 becomes "1 "
				100.0 becomes "100 "
				99999.0 becomes "99,999 "
				100000.0 becomes "100 K"
				99999000.0 becomes "99,999 K"
				100000000.0 becomes "100 M"
				99999000000.0 becomes "99,999 M"
				100000000000.0 becomes "100 G"
				99999000000000.0 becomes "99,999 G"
				100000000000000.0 becomes "100 T"
				99999000000000000.0 becomes "99,999 T"
				100000000000000000.0 returns "1.000e16 "

				Note that scaling values between 1 and -1  (milli, micro, etc)
				are not supported (produce 0).  Negative numbers are scaled
				identically to positive ones but preceded by a minus sign.
				Numbers are rounded to the nearest whole number.

------------------------------------------------------
MODULE NAME:    NUMSCALE - Creat abbreviated number

Usage:          CString NumScale(double val)

Returns:        A 6 char string containing the abbreviated number (see below)

Parameters:     val = a value to be scaled

Description:    Converts a number to a string which is more easily read.
				At most 3 significant digits are generated (and hence
				there is a loss of precision) to aid readability.  If
				scaling is necessary a metric modifier is appended to
				the string.  Eg:

				0.0     becomes  "    0  "
				0.1     becomes  "    0  "
				1.0     becomes  "    1  "
				999     becomes  "  999  "
				-999    becomes  " -999  "
				9999    becomes  " 9.99 K"
				-9999   becomes  "-9.99 K"
				99999   becomes  " 99.9 K"
				999999  becomes  "  999 K"
				1000000 becomes  " 1.00 M"
				1e7     becomes  " 10.0 M"
				1e8     becomes  "  100 M"
				1e9     becomes  " 1.00 G"
				1e12    becomes  " 1.00 T"
				1e15    becomes  " 1.00 P"
				1e18    becomes  " 1.00 E"
				1e21    becomes  " 1.00 Z"
				1e24    becomes  " 1.00 Y"
				1e27    becomes  " 1.0e27"

				Note that scaling values between 1 and -1  (milli, micro, etc)
				are not supported (produce 0).  Negative numbers are scaled
				identically to positive ones but preceded by a minus sign.
				Numbers are rounded to the nearest whole number.

------------------------------------------------------
MODULE NAME:    ADDCOMMAS - Add commas to string of digits

Usage:          void AddCommas(CString &str)

Parameters:     str = string to add commas to

Description:    Adds commas to a string of digits.  The addition of commas
				stops when a character which is not a digit (or comma or
				space) is found and the rest of the string is just appended
				to the result.

				The number may have a leading plus or minus sign and contain
				commas and spaces.  The sign is preserved but any existing commas
				and spaces are removed, unless they occur after the comma
				addition has stopped.

				eg. "" becomes ""
				eg. "1" becomes "1"
				eg. "A,B C" becomes "A,B C"
				eg. "1234567" becomes "1,234,567"
				eg. " - 1 , 234" becomes "-1,234"
				eg. "+1234 ABC" becomes "+1,234 ABC"
				eg. "1234 - 1" becomes "1,234 - 1"
------------------------------------------------------
MODULE NAME:    ADDSPACES - Add spaces to string of hex digits

Usage:          void AddSpaces(CString &str)

Parameters:     str = string to add spaces to

Description:    Like AddCommas() above but adds spaces to a hex number rather
				than commas to a decimal number.

*/

#include "stdafx.h"
#include <MultiMon.h>
#include "HexEdit.h"
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <locale.h>
#include <sys/stat.h>           // For _stat()
#include <sys/utime.h>          // For _utime()
#include <boost/crc.hpp>        // For CRCs
#include <boost/random/mersenne_twister.hpp>
#include <imagehlp.h>           // For ::MakeSureDirectoryPathExists()
#include <winioctl.h>           // For DISK_GEOMETRY, IOCTL_DISK_GET_DRIVE_GEOMETRY etc
#include <direct.h>             // For _getdrive()
#include <FreeImage.h>

#include "misc.h"
#include "ntapi.h"
#include "BigInteger.h"        // Used in C# Decimal conversions

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//-----------------------------------------------------------------------------
// Routines for loading/saving history lists in the registry

// LoadHist reads a list of history strings from the registry
void LoadHist(std::vector<CString> & hh, LPCSTR name, size_t smax)
{
	CString ss, entry;
	CString fmt = CString(name) + "%d";

	hh.clear();
	for (int ii = smax; ii > 0; ii--)
	{
		entry.Format(fmt, ii);
		ss = theApp.GetProfileString("History", entry);
		if (!ss.IsEmpty())
			hh.push_back(ss);
	}
}

// SaveHist writes history strings to the registry for later reading by LoadHist
void SaveHist(std::vector<CString> const & hh, LPCSTR name, size_t smax)
{
	CString entry;
	CString fmt = CString(name) + "%d";

	int ii, num = min(hh.size(), smax);

	// Write th new entries
	for (ii = 1; ii <= num; ++ii)
	{
		// Check that we don't write any empty strings as this will cause problems
		ASSERT(!hh[hh.size()-ii].IsEmpty());

		entry.Format(fmt, ii);
		theApp.WriteProfileString("History", entry, hh[hh.size()-ii]);
	}
	// Wipe out any old entries past the new end
	for ( ; ; ++ii)
	{
		entry.Format(fmt, ii);

		// Stop when there are no more entries in the registry to be blanked out.
		if (theApp.GetProfileString("History", entry).IsEmpty())
			break;

		theApp.WriteProfileString("History", entry, NULL);
	}
}

//-----------------------------------------------------------------------------
// Routines for handling C# Decimal type

// Notes on behaviour of C# Decimal type:
// If mantissa is zero, exponent is ignored
// If any bit of bottom byte is one then it is -ve
// If exponent is > 28 then is printed as stored but when used in calcs is treated as if exponent is zero
// If exponent byte is 127 then calcs crash with null reference exception

// Example decimals and how they display:
// 12345678901234567890123456789  28   -> 1.2345678901234567890123456789
// 10000000000000000000000000000  28   -> 1.0000000000000000000000000000
// 10000000000000000000000000000  27   -> 10.000000000000000000000000000
// 12345678901234567890123456789  0    -> 12345678901234567890123456789
// 1                              -27  -> 0.000000000000000000000000001

#ifdef _DEBUG
void TestToFromDecimal(const char * in, const char * out, const char * mant, const char * exp)
{
	unsigned char dec[16];  // the Decimal value (128 bits)

	ASSERT(StringToDecimal(in, dec));

	CString ss, mm, ee;
	ss = DecimalToString(dec, mm, ee);
	ASSERT(strcmp(out, ss) == 0 && strcmp(mant, mm) == 0 && strcmp(exp, ee) == 0);
}
void TestDecimalRoutines()
{
	TestToFromDecimal("0",    "0",   "0", "0");
	TestToFromDecimal("0.0",  "0",   "0", "0");
	TestToFromDecimal("-0",   "0",   "0", "0");
	TestToFromDecimal("1",    "1",   "1", "0");
	TestToFromDecimal("1.0","1.0",  "10","-1");
	TestToFromDecimal("-1.", "-1",  "-1", "0");
	TestToFromDecimal("-1.0", "-1.0",  "-10", "-1");
	TestToFromDecimal("1.2345678901234567890123456789", "1.2345678901234567890123456789", "12345678901234567890123456789", "-28");

	TestToFromDecimal("79228162514264337593543950335", "79228162514264337593543950335", "79228162514264337593543950335", "0");
	TestToFromDecimal("-79228162514264337593543950335", "-79228162514264337593543950335", "-79228162514264337593543950335", "0");
	TestToFromDecimal("7.9228162514264337593543950335", "7.9228162514264337593543950335", "79228162514264337593543950335", "-28");
	//TestToFromDecimal("", "", "", "");

	unsigned char dec[16];  // Decimal value (128 bits)
	ASSERT(StringToDecimal("79228162514264337593543950336", dec) == false);
}
#endif

bool StringToDecimal(const char *ss, void *presult)
{
	// View the decimal (result) as four 32 bit integers
	BigInteger::Unit *dd = (BigInteger::Unit *)presult;

	BigInteger mant;            // Mantisssa - initially zero
	BigInteger ten(10L);        // Just used to multiply values by 10
	int exp = 0;                // Exponent
	bool dpseen = false;        // decimal point seen yet?
	bool neg = false;           // minus sign seen?

	// Scan the characters of the value
	const char *pp;
	for (pp = ss; *pp != '\0'; ++pp)
	{
		if (*pp == '-')
		{
			if (pp != ss)
				return false;      // minus sign not at start
			neg = true;
		}
		else if (isdigit(*pp))
		{
			BigInteger::Mul(mant, ten, mant);
			BigInteger::Add(mant, BigInteger(long(*pp - '0')), mant);
			if (dpseen) ++exp;  // Keep track of digits after decimal pt
		}
		else if (*pp == '.')
		{
			if (dpseen)
				return false;    // more than one decimal point
			dpseen = true;
		}
		else if (*pp == 'e' || *pp == 'E')
		{
			char *end;
			exp -= strtol(pp+1, &end, 10);
			pp = end;
			break;
		}
		else
			return false;       // unexpected character
	}
	if (*pp != '\0')
		return false;           // extra characters after end

	if (exp < -28 || exp > 28)
		return false;          // exponent outside valid range

	// Adjust mantissa for -ve exponent
	if (exp < 0)
	{
		BigInteger tmp;
		BigInteger::Exp(ten, BigInteger(long(-exp)), tmp);
		BigInteger::Mul(mant, tmp, mant);
		exp = 0;
	}

	// Check for mantissa too big.
	BigInteger::Unit dmax[4];
	dmax[0] = dmax[1] = dmax[2] = ~0UL;
	dmax[3] = 0UL;
	BigInteger max_mant(dmax, 4); 
	if (mant > max_mant)
		return false;
	else if (mant == BigInteger(0L))
		exp = 0;

	// Set integer part
	dd[2] = mant.GetUnit(2);
	dd[1] = mant.GetUnit(1);
	dd[0] = mant.GetUnit(0);

	// Set exponent and sign
	dd[3] = exp << 16;
	if (neg && !mant.IsZero())
		dd[3] |= 0x80000000;

	return true;
}

CString DecimalToString(const void *pdecimal, CString &sMantissa, CString &sExponent)
{
	// View the decimal as four 32 bit integers
	const BigInteger::Unit * dd = (const BigInteger::Unit *)pdecimal;

	// Get the 96 bit integer part (mantissa) adding a top unit of zero
	BigInteger::Unit tmp[4];
	memcpy(tmp, (const BigInteger::Unit *)pdecimal, sizeof(tmp));
	tmp[3] = 0;             // need this so we always get an unsigned mantissa

	// Convert the mantissa to a string
	BigInteger big(tmp, 4);
	big.ToString(sMantissa.GetBuffer(64), 10);
	sMantissa.ReleaseBuffer();

	// Get the exponent
	int exp = (dd[3] >> 16) & 0xFF;
	sExponent.Format("%d", - exp);

	// Check that unused bits are zero and that exponent is within range
	if ((dd[3] & 0x0000FFFF) != 0 || exp > 28)
		return CString("Invalid Decimal");

	// Create a string in which we insert a decimal point
	CString ss = sMantissa;
	int len = ss.GetLength();

	if (exp >= len)
	{
		// Add leading zeroes
		ss = CString('0', exp - len + 1) + ss;
		len = exp + 1;
	}

	// if any bit of the very last byte is on then assume the value is -ve
	if ((dd[3] & 0xFF000000) != 0)
	{
		// Add -ve sign to mantissa and return value with sign and decimal point
		sMantissa.Insert(0, '-');
		return "-" + ss.Left(len - exp) + (exp > 0 ? "." + ss.Right(exp) : "");
	}
	else
		return ss.Left(len - exp) + (exp > 0 ? "." + ss.Right(exp) : "");
}

// -------------------------------------------------------------------------
// Colour routines

static int hlsmax = 100;            // This determines the ranges of values returned
static int rgbmax = 255;

void get_hls(COLORREF rgb, int &hue, int &luminance, int &saturation)
{

	int red = GetRValue(rgb);
	int green = GetGValue(rgb);
	int blue = GetBValue(rgb);
	int cmax = max(red, max(green, blue));
	int cmin = min(red, min(green, blue));

	// Work out luminance
	luminance = ((cmax+cmin)*hlsmax + rgbmax)/(2*rgbmax);

	if (cmax == cmin)
	{
		hue = -1;
		saturation = 0;
	}
	else
	{
		// Work out saturation
		if (luminance <= hlsmax/2)
			saturation = ((cmax-cmin)*hlsmax + (cmax+cmin)/2) / (cmax+cmin);
		else
			saturation = ((cmax-cmin)*hlsmax + (2*rgbmax-cmax-cmin)/2)/(2*rgbmax-cmax-cmin);

		// Work out hue
		int Rdelta = ((cmax-red  )*(hlsmax/6) + (cmax-cmin)/2) / (cmax-cmin);
		int Gdelta = ((cmax-green)*(hlsmax/6) + (cmax-cmin)/2) / (cmax-cmin);
		int Bdelta = ((cmax-blue )*(hlsmax/6) + (cmax-cmin)/2) / (cmax-cmin);
		if (red == cmax)
			hue = Bdelta - Gdelta;
		else if (green == cmax)
			hue = (hlsmax/3) + Rdelta - Bdelta;
		else
		{
			ASSERT(blue == cmax);
			hue = ((2*hlsmax)/3) + Gdelta - Rdelta;
		}

		if (hue < 0)
			hue += hlsmax;
		if (hue > hlsmax)
			hue -= hlsmax;
	}
}

static int hue2rgb(int n1, int n2, int hue)
{
	// Keep hue within range
	if (hue < 0)
		hue += hlsmax;
	else if (hue > hlsmax)
		hue -= hlsmax;

	if (hue < hlsmax/6)
		return n1 + ((n2-n1)*hue + hlsmax/12)/(hlsmax/6);
	else if (hue < hlsmax/2)
		return n2;
	else if (hue < (hlsmax*2)/3)
		return n1 + ((n2-n1)*((hlsmax*2)/3 - hue) + hlsmax/12)/(hlsmax/6);
	else
		return n1;
} 

COLORREF get_rgb(int hue,int luminance, int saturation)
{
	int red, green, blue;
	int magic1, magic2;

	if (hue == -1 || saturation == 0)
	{
		red = green = blue = (luminance*rgbmax)/hlsmax;
	}
	else
	{
		if (luminance <= hlsmax/2)
			magic2 = (luminance*(hlsmax + saturation) + hlsmax/2)/hlsmax;
		else
			magic2 = luminance + saturation - (luminance*saturation + hlsmax/2)/hlsmax;
		magic1 = 2*luminance - magic2;

		// get RGB, changing units from HLSMAX to RGBMAX
		red =   (hue2rgb(magic1, magic2, hue + hlsmax/3)*rgbmax + hlsmax/2)/hlsmax;
		green = (hue2rgb(magic1, magic2, hue           )*rgbmax + hlsmax/2)/hlsmax;
		blue =  (hue2rgb(magic1, magic2, hue - hlsmax/3)*rgbmax + hlsmax/2)/hlsmax;
	}

	return RGB(red, green, blue);
}

// Make a colour (col) closer in tone (ie brightness or luminance) to another
// colour (bg_col) but the otherwise keep the same colour (hue/saturation).
// The parameter amt determines the amount of adjustment.
//  0 = no change
//  0.5 = default means half way to background
//  1 = fully toned down (same as background colour)
//  -ve values = tone "up"
// Note. Despite the name if bg_col is brighter than col it will "tone up".
COLORREF tone_down(COLORREF col, COLORREF bg_col, double amt /* = 0.75*/)
{
	int hue, luminance, saturation;
	get_hls(col, hue, luminance, saturation);

	int bg_hue, bg_luminance, bg_saturation;
	get_hls(bg_col, bg_hue, bg_luminance, bg_saturation);

	int diff = bg_luminance - luminance;

	// If toning "up" make sure that there is some change in the colour
	if (diff == 0 && amt < 0.0)
		diff = luminance > 50 ? 1 : -1;
	luminance += int(diff*amt);
	if (luminance > 100)
		luminance = 100;
	else if (luminance < 0)
		luminance = 0;

	// Make colour the same shade as col but less "bright"
	return get_rgb(hue, luminance, saturation);
}

// Increase contrast (tone up) if necessary.
// If a colour (col) does not have enough contrast with a background
// colour (bg_col) then increase the contrast.  This is similar to
// passing a -ve value  to tone_up (above) but "tones up" conditionally.
// Returns col with adjusted luminance which may be exactly the
// colour of col if therre is already enough contrast.
COLORREF add_contrast(COLORREF col, COLORREF bg_col)
{
	int hue, luminance, saturation;
	get_hls(col, hue, luminance, saturation);

	int bg_hue, bg_luminance, bg_saturation;
	get_hls(bg_col, bg_hue, bg_luminance, bg_saturation);

	if (bg_luminance >= 50 && luminance > bg_luminance - 25)
	{
		// Decrease luminance to increase contrast
		luminance = bg_luminance - 25;
	}
	else if (bg_luminance < 50 && luminance < bg_luminance + 25)
	{
		// Increase luminance to increase contrast
		luminance = bg_luminance + 25;
	}
	assert(luminance <= 100 && luminance >= 0);

	// Make colour the same shade as col but less "bright"
	return get_rgb(hue, luminance, saturation);
}

// This gets a colour of the same hue but with adjusted luminance and/or
// saturation.  This can be used for nice colour effects.
// Supply values for lum and sat in the range 0 - 100 or
// use -1 for lum or sat to not change them.
COLORREF same_hue(COLORREF col, int sat, int lum /* = -1 */)
{
	int hue, luminance, saturation;
	get_hls(col, hue, luminance, saturation);

	if (lum > -1)
		luminance = lum;
	if (sat > -1)
		saturation = sat;

	return get_rgb(hue, luminance, saturation);
}

// -------------------------------------------------------------------------
// Time routines

double TZDiff()
{
	static double tz_diff = -1e30;

	if (tz_diff != -1e30)
		return tz_diff;
	time_t dummy = time_t(1000000L);
	tz_diff = (1000000L - mktime(gmtime(&dummy)))/86400.0;
	return tz_diff;
}

DATE FromTime_t(__int64 v)  // Normal time_t and time64_t (secs since 1/1/1970)
{
	return (365.0*70.0 + 17 + 2) + v/(24.0*60.0*60.0) + TZDiff();
}

DATE FromTime_t_80(long v)
{
	return (365.0*80.0 + 19 + 2) + v/(24.0*60.0*60.0) + TZDiff();
}

DATE FromTime_t_mins(long v)
{
	return (365.0*70.0 + 17 + 2) + v/(24.0*60.0) + TZDiff();
}

DATE FromTime_t_1899(long v)
{
	return v/(24.0*60.0*60.0) + TZDiff();
}

// Convert time_t to FILETIME
bool ConvertToFileTime(time_t tt, FILETIME *ft)
{
	assert(ft != NULL);
	bool retval = false;

	struct tm * ptm = ::localtime(&tt);
	if (ptm != NULL)
	{
		SYSTEMTIME st;
		st.wYear   = ptm->tm_year + 1900;
		st.wMonth  = ptm->tm_mon + 1;
		st.wDay    = ptm->tm_mday;
		st.wHour   = ptm->tm_hour;
		st.wMinute = ptm->tm_min;
		st.wSecond = ptm->tm_sec;
		st.wMilliseconds = 500;

		FILETIME ftemp;
		if (::SystemTimeToFileTime(&st, &ftemp) && ::LocalFileTimeToFileTime(&ftemp, ft))
			retval = true;
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Make nice looking numbers etc

CString NumScale(double val)
{
	double dd = val;
	CString retval;
	bool negative = false;
	static const char *unit_str = " KMGTPE";  // (unit), kilo, mega, giga, tera, peta, exa

	// Allow for negative values (may give "-0")
	if (dd < 0.0)
	{
		dd = -dd;
		negative = true;
	}

	size_t idx;
	for (idx = 0; dd + 0.5 >= (idx >= theApp.k_abbrev_ ? 1000.0 : 1024.0); ++idx)
		dd = dd / (idx >= theApp.k_abbrev_ ? 1000.0 : 1024.0);

	// If too big just print in scientific notation
	if (idx >= strlen(unit_str))
	{
		retval.Format("%6.1g ", val);   // Just output original value with exponent
		return retval;
	}

	if (idx == 0 || dd + 0.5 >= 100.0)
	{
		dd = floor(dd + 0.5);                   // Remove fractional part so %g does not try to show it
		retval.Format("%5g %c", negative ? -dd : dd, unit_str[idx]);
	}
	else
		retval.Format("%#5.3g %c", negative ? -dd : dd, unit_str[idx]);

	return retval;
}

// Make a string of binary digits from a number (64 bit int)
CString bin_str(__int64 val, int bits)
{
	ASSERT(bits <= 64);
	char buf[100], *pp = buf;

	for (int ii = 0; ii < bits; ++ii)
	{
		if (ii%8 == 0)
			*pp++ = ' ';
		*pp++ = (val&(_int64(1)<<ii)) ? '1' : '0';
	}
	*pp = '\0';

	CString retval(buf+1);
	retval.MakeReverse();
	return retval;
}

// Add commas every 3 digits (or as appropriate to the locale) to a decimal string
void AddCommas(CString &str)
{
	const char *pp, *end;                   // Ptrs into orig. string
	int ndigits = 0;                        // Number of digits in string
//	struct lconv *plconv = localeconv(); // Locale settings (grouping etc) are now just read once in InitInstance

	// Allocate enough space (allowing for space padding)
	char *out = new char[(str.GetLength()*(theApp.dec_group_+1))/theApp.dec_group_ + 2]; // Result created here
	char *dd = out;                     // Ptr to current output char

	// Skip leading whitespace
	pp = str.GetBuffer(0);
	while (isspace(*pp))
		pp++;

	// Keep initial sign if any
	if (*pp == '+' || *pp == '-')
		*dd++ = *pp++;

	// Find end of number and work out how many digits it has
	end = pp + strspn(pp, ", \t0123456789");
	for (const char *qq = pp; qq < end; ++qq)
		if (isdigit(*qq))
			++ndigits;

	for ( ; ndigits > 0; ++pp)
		if (isdigit(*pp))
		{
			*dd++ = *pp;
			if (--ndigits > 0 && ndigits%theApp.dec_group_ == 0)
				*dd++ = theApp.dec_sep_char_;
		}
	ASSERT(pp <= end);
	strcpy(dd, pp);

	str = out;
	delete[] out;
}

// Add spaces to make a string of hex digits easier to read
void AddSpaces(CString &str)
{
	const char *pp, *end;               // Ptrs into orig. string
	int ndigits = 0;                    // Number of digits in string
	const char sep_char = ' ';  // Used to separate groups of digits
	const int group = 4;                // How many is in a group

	// Allocate enough space (allowing for space padding)
	char *out = new char[(str.GetLength()*(group+1))/group + 2]; // Result created here
	char *dd = out;                     // Ptr to current output char

	// Skip leading whitespace
	pp = str.GetBuffer(0);
	while (isspace(*pp))
		pp++;

	// Find end of number and work out how many digits it has
	end = pp + strspn(pp, " \t0123456789ABCDEFabcdef");
	for (const char *qq = pp; qq < end; ++qq)
		if (isxdigit(*qq))
			++ndigits;

	for ( ; ndigits > 0; ++pp)
		if (isxdigit(*pp))
		{
			*dd++ = *pp;
			if (--ndigits > 0 && ndigits%group == 0)
				*dd++ = sep_char;
		}
	ASSERT(pp <= end);
	strcpy(dd, pp);

	str = out;
	delete[] out;
}

// When a menu item is selected we only get an id which is usually a command ID
// but for the menu created with make_var_menu_tree the id is only a unique number.
// What we really want is the menu text which contains a variable name.
// Given a menu ptr and an id the menu is searched and the menu item text for
// that id is returned or an empty string if it is not found.
CString get_menu_text(CMenu *pmenu, int id)
{
	CString retval;

	// Check menu items first
	if (pmenu->GetMenuString(id, retval, MF_BYCOMMAND) > 0)
		return retval;

	// Check ancestor menus
	int item_count = pmenu->GetMenuItemCount();
	for (int ii = 0; ii < item_count; ++ii)
	{
		CMenu *psub = pmenu->GetSubMenu(ii);
		if (psub != NULL && psub->GetMenuString(id, retval, MF_BYCOMMAND) > 0)
			return retval;
	}

	return CString("");
}

//-----------------------------------------------------------------------------
// Conversions

static const long double two_pow40 = 1099511627776.0L;

// Make a real48 (8 bit exponent, 39 bit mantissa) from a double.
// Returns false on overflow, returns true (and zero value) on underflow
bool make_real48(unsigned char pp[6], double val, bool big_endian /*=false*/)
{
	int exp;                    // Base 2 exponent of val
	val = frexp(val, &exp);

	if (val == 0.0 || exp < -128)       // zero or underflow
	{
		memset(pp, 0, 6);
		return true;
	}
	else if (exp== -1 || exp > 127)     // inf or overflow
		return false;

	assert(exp + 128 >= 0 && exp + 128 < 256);
	pp[0] = exp + 128;                  // biassed exponent in first byte

	bool neg = val < 0.0;               // remember if -ve
	val = fabs(val);

	// Work out mantissa bits
	__int64 imant = (__int64)(val * two_pow40);
	assert(imant < two_pow40);
	pp[1] = unsigned char((imant) & 0xFF);
	pp[2] = unsigned char((imant>>8) & 0xFF);
	pp[3] = unsigned char((imant>>16) & 0xFF);
	pp[4] = unsigned char((imant>>24) & 0xFF);
	pp[5] = unsigned char((imant>>32) & 0x7F);  // masks off bit 40 (always on)
	if (neg)
		pp[5] |= 0x80;                  // set -ve bit

	if (big_endian)
		flip_bytes(pp, 6);
	return true;
}

// Make a double from a Real48
// Also returns the exponent (base 2) and mantissa if pexp/pmant are not NULL
double real48(const unsigned char *pp, int *pexp, long double *pmant, bool big_endian /*=false*/)
{
	// Copy bytes containing the real48 in case we have to flip byte order
	unsigned char buf[6];
	memcpy(buf, pp, 6);
	if (big_endian)
		flip_bytes(buf, 6);

	int exponent = (int)buf[0] - 128;
	if (pexp != NULL) *pexp = exponent;

	// Build integer mantissa (without implicit leading bit)
	__int64 mantissa = buf[1] + ((__int64)buf[2]<<8) + ((__int64)buf[3]<<16) +
					((__int64)buf[4]<<24) + ((__int64)(buf[5]&0x7F)<<32);

	// Special check for zero
	if (::memcmp(buf, "\0\0\0\0\0", 5)== 0 && (buf[5] & 0x7F) == 0)
	{
		if (pmant != NULL) *pmant = 0.0;
		return 0.0;
	}

	// Add implicit leading 1 bit
	mantissa +=  (_int64)1<<39;

	if (pmant != NULL)
	{
		if ((buf[5] & 0x80) == 0)
			*pmant = mantissa / (two_pow40 / 2);
		else
			*pmant = -(mantissa / (two_pow40 / 2));
	}

	// Check sign bit and return appropriate result
	if (buf[0] == 0 && mantissa < two_pow40/4)
		return 0.0;                            // underflow
	else if ((buf[5] & 0x80) == 0)
		return (mantissa / two_pow40) * powl(2, exponent);
	else
		return -(mantissa / two_pow40) * powl(2, exponent);
}

static const long double two_pow24 = 16777216.0L;
static const long double two_pow56 = 72057594037927936.0L;

bool make_ibm_fp32(unsigned char pp[4], double val, bool little_endian /*=false*/)
{
	int exp;                    // Base 2 exponent of val

	val = frexp(val, &exp);

	if (val == 0.0 || exp < -279)
	{
		// Return IBM zero for zero or exponent too small
		memset(pp, '\0', 4);
		return true;
	}
	else if (exp > 252)
		return false;           // Rteurn error for exponent too big

	// Change signed exponent into +ve biassed exponent (-256=>0, 252=>508)
	exp += 256;

	// normalize exponent to base 16 (with corresp. adjustment to mantissa)
	while (exp < 0 || (exp&3) != 0)   // While bottom 2 bits are not zero we don't have a base 16 exponent
	{
		val /= 2.0;
		++exp;
	}
	exp = exp>>2;               // Convert base 2 exponent to base 16

	// At this point we have a signed base 16 exponent in range 0 to 127 and
	// a signed mantissa in the range 0.0625 (1/16) to (just less than) 1.0.
	// (although val may be < 0.625 if the exponent was < -256).

	assert(exp >= 0 && exp < 128);
	assert(val < 1.0);

	// Store exponent and sign bit
	if (val < 0.0)
	{
		val = -val;
		pp[0] = exp | 0x80;     // Turn on sign bit
	}
	else
	{
		pp[0] = exp;
	}

	long imant = long(val * two_pow24);
	assert(imant < two_pow24);
	pp[1] = unsigned char((imant>>16) & 0xFF);
	pp[2] = unsigned char((imant>>8) & 0xFF);
	pp[3] = unsigned char((imant) & 0xFF);

	if (little_endian)
	{
		unsigned char cc = pp[0]; pp[0] = pp[3]; pp[3] = cc;
		cc = pp[1]; pp[1] = pp[2]; pp[2] = cc;
	}
	return true;
}

bool make_ibm_fp64(unsigned char pp[8], double val, bool little_endian /*=false*/)
{
	int exp;                    // Base 2 exponent of val

	val = frexp(val, &exp);

	if (val == 0.0 || exp < -279)
	{
		// Return IBM zero for zero or exponent too small
		memset(pp, '\0', 8);
		return true;
	}
	else if (exp > 252)
		return false;           // Rteurn error for exponent too big

	// Change signed exponent into +ve biassed exponent (-256=>0, 252=>508)
	exp += 256;

	// normalize exponent to base 16 (with corresp. adjustment to mantissa)
	while (exp < 0 || (exp&3) != 0)   // While bottom 2 bits are not zero we don't have a base 16 exponent
	{
		val /= 2.0;
		++exp;
	}
	exp = exp>>2;               // Convert base 2 exponent to base 16

	// At this point we have a signed base 16 exponent in range 0 to 127 and
	// a signed mantissa in the range 0.0625 (1/16) to (just less than) 1.0.
	// (although val may be < 0.625 if the exponent was < -256).

	assert(exp >= 0 && exp < 128);
	assert(val < 1.0);

	// Store exponent and sign bit
	if (val < 0.0)
	{
		val = -val;
		pp[0] = exp | 0x80;     // Turn on sign bit
	}
	else
	{
		pp[0] = exp;
	}

	__int64 imant = (__int64)(val * two_pow56);
	assert(imant < two_pow56);
	pp[1] = unsigned char((imant>>48) & 0xFF);
	pp[2] = unsigned char((imant>>40) & 0xFF);
	pp[3] = unsigned char((imant>>32) & 0xFF);
	pp[4] = unsigned char((imant>>24) & 0xFF);
	pp[5] = unsigned char((imant>>16) & 0xFF);
	pp[6] = unsigned char((imant>>8) & 0xFF);
	pp[7] = unsigned char((imant) & 0xFF);

	if (little_endian)
	{
		unsigned char cc = pp[0]; pp[0] = pp[7]; pp[7] = cc;
		cc = pp[1]; pp[1] = pp[6]; pp[6] = cc;
		cc = pp[2]; pp[2] = pp[5]; pp[5] = cc;
		cc = pp[3]; pp[3] = pp[4]; pp[4] = cc;
	}
	return true;
}

long double ibm_fp32(const unsigned char *pp, int *pexp /*=NULL*/,
					 long double *pmant /*=NULL*/, bool little_endian /*=false*/)
{
	unsigned char tt[4];                // Used to store bytes in little-endian order
	int exponent;                       // Base 16 exponent of IBM FP value
	long mantissa;                      // Unsigned integer mantissa

	if (little_endian)
	{
		tt[0] = pp[3];
		tt[1] = pp[2];
		tt[2] = pp[1];
		tt[3] = pp[0];
		pp = tt;
	}

	// Work out the binary exponent:
	// - remove the high (sign) bit from the first byte
	// - subtract the bias of 64
	// - convert hex exponent to binary [note 16^X == 2^(4*X)]
	exponent = ((int)(pp[0] & 0x7F) - 64);
	if (pexp != NULL) *pexp = exponent;
	// - convert hex exponent to binary
	//   note that we multiply the exponent by 4 since 16^X == 2^(4*X)
	exponent *= 4;

	// Get the mantissa and return zero if there are no bits on
	mantissa = ((long)pp[1]<<16) + ((long)pp[2]<<8) + pp[3];
	if (mantissa == 0)
	{
		if (pmant != NULL) *pmant = 0.0;
		return 0.0;
	}

	if (pmant != NULL)
	{
		if ((pp[0] & 0x80) == 0)
			*pmant = mantissa / two_pow24;
		else
			*pmant = -(mantissa / two_pow24);
	}

	// Check sign bit and return appropriate result
	if ((pp[0] & 0x80) == 0)
		return (mantissa / two_pow24) * powl(2, exponent);
	else
		return -(mantissa / two_pow24) * powl(2, exponent);
}

long double ibm_fp64(const unsigned char *pp, int *pexp /*=NULL*/,
				long double *pmant /*=NULL*/, bool little_endian /*=false*/)
{
	unsigned char tt[8];                // Used to store bytes in little-endian order
	int exponent;                       // Base 16 exponent of IBM FP value
	__int64 mantissa;                   // Unsigned integral mantissa

	if (little_endian)
	{
		tt[0] = pp[7];
		tt[1] = pp[6];
		tt[2] = pp[5];
		tt[3] = pp[4];
		tt[4] = pp[3];
		tt[5] = pp[2];
		tt[6] = pp[1];
		tt[7] = pp[0];
		pp = tt;
	}

	// Work out the binary exponent:
	// - remove the high (sign) bit from the first byte
	// - subtract the bias of 64
	exponent = ((int)(pp[0] & 0x7F) - 64);
	if (pexp != NULL) *pexp = exponent;
	// - convert hex exponent to binary
	//   note that we multiply the exponent by 4 since 16^X == 2^(4*X)
	exponent *= 4;

	// Get the mantissa and return zero if there are no bits on
	mantissa =  ((__int64)pp[1]<<48) + ((__int64)pp[2]<<40) + ((__int64)pp[3]<<32) +
				((__int64)pp[4]<<24) + ((__int64)pp[5]<<16) + ((__int64)pp[6]<<8) + pp[7];
	if (mantissa == 0)
	{
		if (pmant != NULL) *pmant = 0.0;
		return 0.0;
	}

	if (pmant != NULL)
	{
		if ((pp[0] & 0x80) == 0)
			*pmant = mantissa / two_pow56;
		else
			*pmant = -(mantissa / two_pow56);
	}

	// Check sign bit and return appropriate result
	if ((pp[0] & 0x80) == 0)
		return (mantissa / two_pow56) * powl(2, exponent);
	else
		return -(mantissa / two_pow56) * powl(2, exponent);
}

// The compiler does not provide a function for reading a 64 bit int from a string?!!
__int64 strtoi64(const char *ss, int radix /*=0*/)
{
	if (radix == 0) radix = 10;

	__int64 retval = 0;

	for (const char *src = ss; *src != '\0'; ++src)
	{
		// Ignore everything except valid digits
		unsigned int digval;
		if (isdigit(*src))
			digval = *src - '0';
		else if (isalpha(*src))
			digval = toupper(*src) - 'A' + 10;
		else
			continue;                   // Ignore separators (or any other garbage)

		if (digval >= radix)
		{
			ASSERT(0);                  // How did this happen?
			continue;                   // Ignore invalid digits
		}

		// Ignore overflow
		retval = retval * radix + digval;
	}

	return retval;
}

// Slightly better version with overflow checking and returns ptr to 1st char not used
__int64 strtoi64(const char *ss, int radix, const char **endptr)
{
	if (radix == 0) radix = 10;

	__int64 retval = 0;

	unsigned __int64 maxval = _UI64_MAX / radix;

	const char * src;
	for (src = ss; *src != '\0'; ++src)
	{
		// Ignore everything except valid digits
		unsigned int digval;
		if (isdigit(*src))
			digval = *src - '0';
		else if (isalpha(*src))
			digval = toupper(*src) - 'A' + 10;
		else
			break;   // Not a digit

		if (digval >= radix)
			break;   // Digit too large for radix

		if (retval < maxval || (retval == maxval && (unsigned __int64)digval <= _UI64_MAX % radix))
			retval = retval * radix + digval;
		else
			break;  // overflow
	}

	if (endptr != NULL)
		*endptr = src;

	return retval;
}

//-----------------------------------------------------------------------------
void BrowseWeb(UINT id)
{
	CString str;
	VERIFY(str.LoadString(id));

	::ShellExecute(AfxGetMainWnd()->m_hWnd, _T("open"), str, NULL, NULL, SW_SHOWNORMAL);
}

//-----------------------------------------------------------------------------
// File handling

// Try to find the absolute path the .EXE and return it (including trailing backslash)
CString GetExePath()
{
	char fullpath[_MAX_PATH];
	char *end;                          // End of path of exe file

	// First try argv[0] which is usually the full pathname of the .EXE
	strncpy(fullpath, __argv[0], sizeof(fullpath)-1);

	// Check if we did not get an absolute path (if fired up from command line it may be relative)
	if (fullpath[1] != ':')
	{
		// Use path of help file which is stored in the same directory as the .EXE
		strncpy(fullpath, AfxGetApp()->m_pszHelpFilePath, sizeof(fullpath)-1);
	}

	fullpath[sizeof(fullpath)-1] = '\0';  // make sure it is nul-terminated

	// Remove any trailing filename
	if ((end = strrchr(fullpath, '\\')) == NULL && (end = strrchr(fullpath, ':')) == NULL)
		end = fullpath;
	else
		++end;   // move one more so backslash is included
	*end = '\0';

	return CString(fullpath);
}

// Get the place to store user data files
BOOL GetDataPath(CString &data_path, int csidl /*=CSIDL_APPDATA*/)
{
	if (theApp.win95_)
	{
		// Windows 95 does not support app data special folder so just use .exe location
		data_path = ::GetExePath();
		return TRUE;
	}

	BOOL retval = FALSE;
	LPTSTR pbuf = data_path.GetBuffer(MAX_PATH);

	LPMALLOC pMalloc;
	if (SUCCEEDED(SHGetMalloc(&pMalloc)))
	{
		ITEMIDLIST *itemIDList;
		if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, csidl, &itemIDList)))
		{
			SHGetPathFromIDList(itemIDList, pbuf);
			pMalloc->Free(itemIDList);
			retval = TRUE;
		}
		pMalloc->Release();
	}

	data_path.ReleaseBuffer();
	if (!retval)
		data_path.Empty();     // Make string empty if we failed just in case
	else if (csidl == CSIDL_APPDATA || csidl == CSIDL_COMMON_APPDATA)
		data_path += '\\' + CString(AfxGetApp()->m_pszRegistryKey) + 
					 '\\' + CString(AfxGetApp()->m_pszProfileName) +
					 '\\';
	return retval;
}

// FileErrorMessage - generate a meaningful error string from a CFileException
// fe = pointer to error information (including file name)
// mode = this parameter is used to make the message more meaningful depending
//        on the type of operations being performed on the file.  If the only
//        file operations being performed are reading file(s) the use
//        CFile::modeRead.  If only creating/writing files use CFile::modeWrite.
//        Otherwise use the default (both flags combined) to get messages that
//        are less specific but will not mislead the user if the flag does not
//        match the operations.
CString FileErrorMessage(const CFileException *fe, UINT mode /*=CFile::modeRead|CFile::modeWrite*/)
{
		CString retval;                                         // Returned error message
		CFileStatus fs;                                         // Current file status (if access error)
		UINT drive_type;                                        // Type of drive where the file is
	char rootdir[4] = "?:\\";			// Use with GetDriveType

		if (mode == CFile::modeRead)
				retval.Format("An error occurred reading from the file \"%s\".\n", fe->m_strFileName);
		else if (mode = CFile::modeWrite)
				retval.Format("An error occurred writing to the file \"%s\".\n", fe->m_strFileName);
		else
				retval.Format("An error occurred using the file \"%s\".\n", fe->m_strFileName);

		switch (fe->m_cause)
		{
		case CFileException::none:
				ASSERT(0);                                                      // There should be an error for this function to be called
				retval += "Apparently there was no error!";
				break;
#if _MSC_VER < 1400  // generic is now a C++ reserved word
		case CFileException::generic:
#else
		case CFileException::genericException:
#endif
				retval += "The error is not specified.";
				break;
		case CFileException::fileNotFound:
				ASSERT(mode != CFile::modeWrite);       // Should be reading from an existing file
				retval += "The file does not exist.";
				break;
		case CFileException::badPath:
				retval += "The file name contains invalid characters or the drive\n"
							  "or one or more component directories do not exist.";
				break;
		case CFileException::tooManyOpenFiles:
				retval += "There are too many open files in this system.\n"
								  "Try closing some programs or rebooting the system.";
				break;
		case CFileException::accessDenied:
				// accessDenied (or errno == EACCES) is a general purpose error
				// value and can mean many things.  We try to find out more about
				// the file to work out what exactly went wrong.
				if (!CFile::GetStatus(fe->m_strFileName, fs))
				{
						if (fe->m_strFileName.Compare("\\") == 0 ||
							fe->m_strFileName.GetLength() == 3 && fe->m_strFileName[1] == ':' && fe->m_strFileName[2] == '\\')
						{
								retval += "This is the root directory.\n"
												  "You cannot use it as a file.";
						}
						else if (mode == CFile::modeWrite)
						{
								retval += "Check that you have permission to write\n"
												  "to the directory and that the disk is\n"
												  "not write-protected or read-only media.";
						}
						else
								retval += "Access denied. Check that you have permission\n"
												  "to read the directory and use the file.";
				}
				else if (fs.m_attribute & CFile::directory)
						retval += "This is a directory - you cannot use it as a file.";
				else if (fs.m_attribute & (CFile::volume|CFile::hidden|CFile::system))
						retval += "The file is a special system file.";
				else if ((fs.m_attribute & CFile::readOnly) && mode != CFile::modeRead)
						retval += "You cannot write to a read-only file.";
				else
						retval += "You cannot access this file.";
				break;
		case CFileException::invalidFile:
				ASSERT(0);                                                              // Uninitialised or corrupt file handle
				retval += "A software error occurred: invalid file handle.\n"
								  "Please report this defect to the software vendor.";
				break;
		case CFileException::removeCurrentDir:
				retval += "An attempt was made to remove the current directory.";
				break;
		case CFileException::directoryFull:
				ASSERT(mode != CFile::modeRead);        // Must be creating a file
				retval += "The file could not be created because the directory\n"
								  "for the file is full.  Delete some files from the\n"
								  "root directory or use a sub-directory.";
				break;
		case CFileException::badSeek:
				ASSERT(0);                                                      // Normally caused by a bug
				retval += "A software error occurred: seek to bad file position.";
				break;
		case CFileException::hardIO:
				if (fe->m_strFileName.GetLength() > 2 && fe->m_strFileName[1] == ':')
						rootdir[0] = fe->m_strFileName[0];
				else
						rootdir[0] = _getdrive() + 'A' - 1;

				drive_type = GetDriveType(rootdir);
				switch (drive_type)
				{
				case DRIVE_REMOVABLE:
				case DRIVE_CDROM:
						retval += "There was a problem accessing the drive.\n"
										  "Please ensure that the medium is present.";
						break;
				case DRIVE_REMOTE:
						retval += "There was a problem accessing the file.\n"
										  "There may be a problem with your network.";
						break;
				default:
						retval += "There was a hardware error.  There may be\n"
										  "a problem with your computer or disk drive.";
						break;
				}
				break;
		case CFileException::sharingViolation:
				retval += "SHARE.EXE is not loaded or the file is in use.\n"
								  "Check that SHARE.EXE is installed, then try\n"
								  "closing other programs or rebooting the system";
				break;
		case CFileException::lockViolation:
				retval += "The file (or part thereof) is in use.\n"
								  "Try closing other programs or rebooting the system";
				break;
		case CFileException::diskFull:
				ASSERT(mode != CFile::modeRead);        // Must be writing to a file
				retval += "The file could not be written as the disk is full.\n"
								  "Please delete some files to make room on the disk.";
				break;
		case CFileException::endOfFile:
				ASSERT(mode != CFile::modeWrite);       // Should be reading from a file
				retval += "An attempt was made to access an area past the end of the file.";
				break;
		default:
				ASSERT(0);
				retval += "An undocumented file error occurred.";
				break;
		}
		return retval;
}

//-----------------------------------------------------------------------------
// Multiple monitor handling

// Gets rect of the monitor which contains the mouse
CRect MonitorMouse()
{
	CPoint pt;
	GetCursorPos(&pt);

	CRect rect(pt.x, pt.y, pt.x+1, pt.y+1);
	return MonitorRect(rect);
}

// Gets rect of monitor which contains most of rect.
// In non-multimon environment it just returns the rect of the
// screen work area (excludes "always on top" docked windows).
CRect MonitorRect(CRect rect)
{
	CRect cont_rect;

	if (theApp.mult_monitor_)
	{
		// Use rect of containing monitor as "container"
		HMONITOR hh = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (hh != 0 && GetMonitorInfo(hh, &mi))
			cont_rect = mi.rcWork;  // work area of nearest monitor
		else
		{
			// Shouldn't happen but if it does use the whole virtual screen
			ASSERT(0);
			cont_rect = CRect(::GetSystemMetrics(SM_XVIRTUALSCREEN),
				::GetSystemMetrics(SM_YVIRTUALSCREEN),
				::GetSystemMetrics(SM_XVIRTUALSCREEN) + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
				::GetSystemMetrics(SM_YVIRTUALSCREEN) + ::GetSystemMetrics(SM_CYVIRTUALSCREEN));
		}
	}
	else if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, &cont_rect, 0))
	{
		// I don't know if this will ever happen since the Windows documentation
		// is pathetic and does not say when or why SystemParametersInfo might fail.
		cont_rect = CRect(0, 0, ::GetSystemMetrics(SM_CXFULLSCREEN),
								::GetSystemMetrics(SM_CYFULLSCREEN));
	}

	return cont_rect;
}

// Returns true if rect is wholly or partially off the screen.
// In multimon environment it also returns true if rect extends over more than one monitor.
bool OutsideMonitor(CRect rect)
{
	CRect cont_rect = MonitorRect(rect);

	return rect.left   < cont_rect.left  ||
		   rect.right  > cont_rect.right ||
		   rect.top    < cont_rect.top   ||
		   rect.bottom > cont_rect.bottom;
}

// Check if most of window is off all monitors.  If so it returns true and
// adjusts the parameter (rect) so the rect is fully within the closest monitor.
bool NeedsFix(CRect &rect)
{
	CRect cont_rect = MonitorRect(rect);
	CRect small_rect(rect);
	small_rect.DeflateRect(rect.Width()/4, rect.Height()/4);
	CRect tmp;
	if (!tmp.IntersectRect(cont_rect, small_rect))
	{
		// Fix the rect
#if _MSC_VER >= 1300
		if (rect.right > cont_rect.right)
			rect.MoveToX(rect.left - (rect.right - cont_rect.right));
		if (rect.bottom > cont_rect.bottom)
			rect.MoveToY(rect.top - (rect.bottom - cont_rect.bottom));
		if (rect.left < cont_rect.left)
			rect.MoveToX(cont_rect.left);
		if (rect.top < cont_rect.top)
			rect.MoveToY(cont_rect.top);
#endif
		return true;
	}
	else
		return false;  // its OK
}

// Check if a lengthy operation should be aborted.
// Updates display and checks for user pressing Escape key.
bool AbortKeyPress()
{
	bool retval = false;
	MSG msg;

	// Do any redrawing, but nothing else
	while (::PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	// Check if any key has been pressed
	if (::PeekMessage(&msg, NULL, WM_KEYDOWN, WM_KEYDOWN, PM_REMOVE))
	{
		int cc = msg.wParam;

		// Windows does not like to miss key down events (needed to match key up events)
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);

		// Remove any characters resulting from keypresses (so they are not inserted into the active file)
		while (::PeekMessage(&msg, NULL, WM_CHAR, WM_CHAR, PM_REMOVE))
			;

		// Abort is signalled with Escape key or SPACE bar
		retval = cc == VK_ESCAPE || cc == VK_SPACE;
	}

	return retval;
}

/////////////////////////////////////////////////////////////////////////////
// Windows NT native API functions
HINSTANCE hNTDLL;        // Handle to NTDLL.DLL

PFRtlInitUnicodeString pfInitUnicodeString;
PFNtOpenFile pfOpenFile;
PFNtClose pfClose;
PFNtDeviceIoControlFile pfDeviceIoControlFile;
PFNtQueryInformationFile pfQueryInformationFile;
PFNtSetInformationFile pfSetInformationFile;
PFNtQueryVolumeInformationFile pfQueryVolumeInformationFile;
PFNtReadFile pfReadFile;
PFNtWriteFile pfWriteFile;

void GetNTAPIFuncs()
{
	if (!theApp.is_nt_)
		return;

	if (hNTDLL != (HINSTANCE)0)
		return;                 // Already done

	if ((hNTDLL = ::LoadLibrary("ntdll.dll")) == (HINSTANCE)0)
		return;

	pfInitUnicodeString    = (PFRtlInitUnicodeString)  ::GetProcAddress(hNTDLL, "RtlInitUnicodeString");
	pfOpenFile             = (PFNtOpenFile)            ::GetProcAddress(hNTDLL, "NtOpenFile");
	pfClose                = (PFNtClose)               ::GetProcAddress(hNTDLL, "NtClose");
	pfDeviceIoControlFile  = (PFNtDeviceIoControlFile) ::GetProcAddress(hNTDLL, "NtDeviceIoControlFile");
	pfQueryInformationFile = (PFNtQueryInformationFile)::GetProcAddress(hNTDLL, "NtQueryInformationFile");
	pfSetInformationFile   = (PFNtSetInformationFile)  ::GetProcAddress(hNTDLL, "NtSetInformationFile");
	pfQueryVolumeInformationFile = (PFNtQueryVolumeInformationFile)::GetProcAddress(hNTDLL, "NtQueryVolumeInformationFile");
	pfReadFile             = (PFNtReadFile)             ::GetProcAddress(hNTDLL, "NtReadFile");
	pfWriteFile            = (PFNtWriteFile)            ::GetProcAddress(hNTDLL, "NtWriteFile");
}

// Given a "fake" device file name return a NT native API device name. Eg \\.\Floppy1 => \device\Floppy1
// Note: For \device\HarddiskN entries the sub-entry "Partition0" is not really a partition but
//       allows access to the whole disk.  (This is only a real device under NT, but is a valid
//       alias under 2K/XP to something like \device\HarddiskN\DRM (where N != M, although N == M
//       for fixed devices and M > N for removeable (M increases every time media is swapped).
BSTR GetPhysicalDeviceName(LPCTSTR name)
{
	CString dev_name;

	if (_tcsncmp(name, _T("\\\\.\\PhysicalDrive"), 17) == 0)
		dev_name.Format(_T("\\device\\Harddisk%d\\Partition0"), atoi(name + 17));
	else if (_tcsncmp(name, _T("\\\\.\\CdRom"), 9) == 0)
		dev_name.Format(_T("\\device\\Cdrom%d"), atoi(name + 9));
	else if (_tcsncmp(name, _T("\\\\.\\Floppy"), 10) == 0)
		dev_name.Format(_T("\\device\\Floppy%d"), atoi(name + 10));
	else
		ASSERT(0);

	// Return as a BSTR (so Unicode string is available)
	return dev_name.AllocSysString();
}

// Get volume number (0-25) assoc. with a physical device or -1 if not found.
// Note: This currently only works for optical and removeable drives (not floppy or fixed drives).
int DeviceVolume(LPCTSTR filename)
{
	const char *dev_str;                // Start of device name that we are looking for
	UINT drive_type;                    // Type of drive looking for
	int dev_num;                        // The device number we are looking for

	if (_tcsncmp(filename, _T("\\\\.\\PhysicalDrive"), 17) == 0)
	{
		drive_type = DRIVE_REMOVABLE;
		dev_num = atoi(filename + 17);
		dev_str = "\\device\\Harddisk";
	}
	else if (_tcsncmp(filename, _T("\\\\.\\CdRom"), 9) == 0)
	{
		drive_type = DRIVE_CDROM;
		dev_num = atoi(filename + 9);
		dev_str = "\\device\\Cdrom";
	}
	//else if (_tcsncmp(filename, _T("\\\\.\\Floppy"), 10) == 0)
	//{
	//	dev_num = atoi(filename + 10);
	//	dev_str = "\\device\\Floppy";
	//}
	else
	{
		ASSERT(0);  // invalid physical device name
		return -1;
	}

	// First scan all volumes (A: to Z:) for drives of the desired type
	DWORD dbits = GetLogicalDrives();   // Bit is on for every drive present
	char vol[4] = "?:\\";               // Vol name used for volume enquiry calls
	for (int ii = 0; ii < 26; ++ii)
	{
		vol[0] = 'A' + (char)ii;
		if ((dbits & (1<<ii)) != 0 && ::GetDriveType(vol) == drive_type)
		{
			char dev[3] = "?:";
			dev[0] = 'A' + (char)ii;
			char buf[128];
			buf[0] = '\0';
			::QueryDosDevice(dev, buf, sizeof(buf));  // eg \device\cdrom0
			int offset = strlen(dev_str);
			if (_strnicmp(buf, dev_str, offset) == 0 &&
				isdigit(buf[offset]) &&
				atoi(buf + offset) == dev_num)
			{
				return ii;
			}
		}
	}
	return -1;
}

// Get display name for device file name
CString DeviceName(CString name) // TODO get rid of this (use SpecialList::name() instead)
{
	CString retval;
	if (name.Left(17) == _T("\\\\.\\PhysicalDrive"))
		retval = _T("Physical Drive ") + name.Mid(17);
	else if (name.Left(9) == _T("\\\\.\\CdRom"))
		retval = _T("Optical Drive ") + name.Mid(9);
	else if (name.Left(10) == _T("\\\\.\\Floppy"))
		retval = _T("Floppy Drive ") + name.Mid(10);
#if 0
	if (name.Left(14) == _T("\\device\\Floppy"))
		retval.Format("Floppy Drive %d", atoi(name.Mid(14)));
	else if (name.Left(16) == _T("\\device\\Harddisk"))
		retval.Format("Hard Drive %d", atoi(name.Mid(16)));
	else if (name.Left(13) == _T("\\device\\Cdrom"))
		retval.Format("Optical Drive %d", atoi(name.Mid(13)));
#endif
	//else if (name.Left(17) == _T("\\\\.\\PhysicalDrive"))
	//	return _T("Physical Drive ") + name.Mid(17);
	//else if (name.Left(9) == _T("\\\\.\\CdRom"))
	//	return _T("Optical Drive ") + name.Mid(9);
	//else if (name == _T("\\\\.\\a:"))
	//	return CString(_T("Floppy Drive 0"));
	//else if (name == _T("\\\\.\\b:"))
	//	return CString(_T("Floppy Drive 1"));
	else if (name.GetLength() == 6)
	{
		_TCHAR vol[4] = _T("?:\\");     // Vol name used for volume enquiry calls
		vol[0] = name[4];

		// Find out what sort of drive it is
		retval.Format(_T(" (%c:)"), vol[0]);
		switch (::GetDriveType(vol))
		{
		default:
			ASSERT(0);
			break;
		case DRIVE_NO_ROOT_DIR:
		case DRIVE_REMOVABLE:
			if (vol[0] == 'A' || vol[0] == 'B')
				retval = "Floppy Drive " + retval;
			else
				retval = "Removable " + retval;
			break;
		case DRIVE_FIXED:
			retval = "Fixed Drive " + retval;
			break;
		case DRIVE_REMOTE:
			retval = "Network Drive " + retval;
			break;
		case DRIVE_CDROM:
			retval = "Optical Drive " + retval;
			break;
		case DRIVE_RAMDISK:
			retval = "RAM Drive " + retval;
			break;
		}
	}
	else
		ASSERT(0);

	return retval;
}

//-----------------------------------------------------------------------------
// PRNGs

static boost::mt19937 rng;

void rand_good_seed(unsigned long seed)
{
	rng.seed(boost::mt19937::result_type(seed));
}

unsigned long rand_good()
{
	return rng();
}

//-----------------------------------------------------------------------------
// Memory

// Compare two blocks of memory to find the first different byte.  (This is 
// similar to memcmp but returns the number of bytes the same.)
// Parameters:
//   pp, qq = pointers to the blocks of memory to compare
//   len = max number of bytes to compare
// Returns:
//   the number of bytes up to the difference OR
//   -1 if all bytes are the same
// Notes:
//   The code is optimized to do 8-byte compares for a 64-bit processor.
//   Best performance is when both pp and qq have the same 8-byte alignment.
//   However, even if pp and qq have different alignements better performance 
//   is obtained if 8-byte reads are done from one of the buffers - this is the 
//   reason for skipping up to 7 bytes at the start of the comparison.
//   Under MS C++ version 15 (VS2008) this gives comparable speed to memcmp()
//   when both buffers start on a 4-byte boundary but much better performance
//   in all other situations.  (The other problem is that memcmp does not say 
//   where in the buffers the difference was found.)
//   This code would be even faster compiled for a 64-bit processor since a
//   comparison between two __int64 values would be a single instruction, etc.
int next_diff(const void * buf1, const void * buf2, size_t len)
{
	const unsigned char * pp = (const unsigned char *)buf1;
	const unsigned char * qq = (const unsigned char *)buf2;

	// Check up to 7 initial bytes (until pp and/or qq is aligned on a 8-byte boundary)
	while (((int)pp % 8) != 0 && len > 0)
	{
		if (*pp != *qq)
			return pp - (const unsigned char *)buf1;
		++pp, ++qq;
		len--;
	}

	// Check most of the buffers 8 bytes at a time (best if qq is also 8 byte aligned)
	div_t len8 = div(len, 8);
	const unsigned __int64 * pend = (const unsigned __int64 *)pp + len8.quot;

	while ((const unsigned __int64 *)pp < pend)
	{
		if (*((const unsigned __int64 *)pp) != *((const unsigned __int64 *)qq))
		{
			// This 8-byte value is different - work out which byte it was
			while (*pp == *qq)
				++pp, ++qq;

			return pp - (const unsigned char *)buf1;
		}

		pp += 8;
		qq += 8;
	}

	// Check up to 7 bytes at the end
	while (len8.rem--)
	{
		if (*pp != *qq)
			return pp - (const unsigned char *)buf1;
		++pp, ++qq;
	}

	return -1;
}

// Returns a 32-bit hash value given a string.  Fast with a good
// distribution (except for very short strings of course).
unsigned long str_hash(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *(const unsigned char *)str++) != '\0')
		hash = ((hash << 5) + hash) + c;  // h = h*33 + c

	return hash;
}

//-----------------------------------------------------------------------------
// CRCs

void * crc_16_init()
{
	return new boost::crc_16_type;
}

void crc_16_update(void *hh, const void *buf, size_t len)
{
	boost::crc_16_type *pcrc = (boost::crc_16_type *)hh;
	pcrc->process_bytes(buf, len);
}

unsigned short crc_16_final(void *hh)
{
	boost::crc_16_type *pcrc = (boost::crc_16_type *)hh;
	unsigned short retval = pcrc->checksum();
	delete pcrc;
	return retval;
}

unsigned short crc_16(const void *buf, size_t len)
{
	void * hh = crc_16_init();
	crc_16_update(hh, buf, len);
	return crc_16_final(hh);
}

void * crc_32_init()
{
	return new boost::crc_32_type;
}

void crc_32_update(void *hh, const void *buf, size_t len)
{
	boost::crc_32_type *pcrc = (boost::crc_32_type *)hh;
	pcrc->process_bytes(buf, len);
}

DWORD crc_32_final(void *hh)
{
	boost::crc_32_type *pcrc = (boost::crc_32_type *)hh;
	DWORD retval = pcrc->checksum();
	delete pcrc;
	return retval;
}

DWORD crc_32(const void *buf, size_t len)
{
	void * hh = crc_32_init();
	crc_32_update(hh, buf, len);
	return crc_32_final(hh);
}

void * crc_ccitt_init()
{
	return new boost::crc_ccitt_type;
}

void crc_ccitt_update(void *hh, const void *buf, size_t len)
{
	boost::crc_ccitt_type *pcrc = (boost::crc_ccitt_type *)hh;
	pcrc->process_bytes(buf, len);
}

unsigned short crc_ccitt_final(void *hh)
{
	boost::crc_ccitt_type *pcrc = (boost::crc_ccitt_type *)hh;
	unsigned short retval = pcrc->checksum();
	delete pcrc;
	return retval;
}

unsigned short crc_ccitt(const void *buf, size_t len)
{
	void * hh = crc_ccitt_init();
	crc_ccitt_update(hh, buf, len);
	return crc_ccitt_final(hh);
}

void * crc_xmodem_init()
{
	return new boost::crc_xmodem_type;
}

void crc_xmodem_update(void *hh, const void *buf, size_t len)
{
	boost::crc_xmodem_type *pcrc = (boost::crc_xmodem_type *)hh;
	pcrc->process_bytes(buf, len);
}

unsigned short crc_xmodem_final(void *hh)
{
	boost::crc_xmodem_type *pcrc = (boost::crc_xmodem_type *)hh;
	unsigned short retval = pcrc->checksum();
	delete pcrc;
	return retval;
}
unsigned short crc_xmodem(const void *buf, size_t len)
{
	void * hh = crc_xmodem_init();
	crc_xmodem_update(hh, buf, len);
	return crc_xmodem_final(hh);
}
