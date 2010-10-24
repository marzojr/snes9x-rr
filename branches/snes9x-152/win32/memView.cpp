//MemView dialog was copied and adapted from DeSmuME: http://sourceforge.net/projects/desmume/
//Authors: DeSmuME team

// gocha:
// 1. According to Normmatt, YopYop had released the final version of original desmume as public domain.
// 2. I asked DeSmuME team about the problem between GPL and Snes9x license, but no one raised any objections.
// Therefore, I decided to include this code into Snes9x project.
// Dear DeSmuME coders: if it is not allowed, please tell me.

/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <list>
#include <algorithm>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>
#include "../port.h"
#include "../snes9x.h"
#include "../memmap.h"
#include "CWindow.h"
#include "rsrc/resource.h"
#include "wsnes9x.h"
#include "memView.h"

using namespace std;

typedef uint32_t HWAddressType;

enum RegionType {
	MEMVIEW_RAM = 0,
	MEMVIEW_SRAM,
	MEMVIEW_FILLRAM
};

struct MemViewRegion
{
	char name[16]; // name of this region (ex. SRAM)
	HWAddressType hardwareAddress; // hardware address of the start of this region
	unsigned int size; // number of bytes to the end of this region
};

static const MemViewRegion s_ramRegion = { "RAM", 0x7e0000, 0x20000 };
static const MemViewRegion s_sramRegion = { "SRAM", 0x000000, 0x20000 };
static const MemViewRegion s_fillRAMRegion = { "FillRAM", 0x000000, 0x8000 };

typedef std::vector<MemViewRegion> MemoryList;
static MemoryList s_memoryRegions;

//////////////////////////////////////////////////////////////////////////////

uint8_t memRead8 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || address >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	switch (regionType)
	{
	case MEMVIEW_RAM:
		return S9xGetByte(address, true);
	case MEMVIEW_SRAM:
		return Memory.SRAM[address];
	case MEMVIEW_FILLRAM:
		return Memory.FillRAM[address];
	}
	return 0;
}

uint16_t memRead16 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || (address + 1) >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	switch (regionType)
	{
	case MEMVIEW_RAM:
		return S9xGetWord(address, WRAP_NONE, true);
	case MEMVIEW_SRAM:
		return *(uint16_t*)(&Memory.SRAM[address]);
	case MEMVIEW_FILLRAM:
		return *(uint16_t*)(&Memory.FillRAM[address]);
	}
	return 0;
}

uint32_t memRead32 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || (address + 3) >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	uint32_t value = memRead16(regionType, address);
	value |= (memRead16(regionType, address + 2) << 16);
	return value;
}

void memRead(uint8_t* buffer, RegionType regionType, HWAddressType address, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		buffer[i] = memRead8(regionType, address + i);
	}
}

void memWrite8 (RegionType regionType, HWAddressType address, uint8_t value)
{
	switch (regionType)
	{
	case MEMVIEW_RAM:
		S9xSetByte(value, address, true);
		break;
	case MEMVIEW_SRAM:
		Memory.SRAM[address] = value;
		break;
	case MEMVIEW_FILLRAM:
		Memory.FillRAM[address] = value;
		break;
	}
}

void memWrite16 (RegionType regionType, HWAddressType address, uint16_t value)
{
	switch (regionType)
	{
	case MEMVIEW_RAM:
		S9xSetWord(value, address, WRAP_NONE, WRITE_01, true);
	case MEMVIEW_SRAM:
		*(uint16_t*)(&Memory.SRAM[address]) = value;
		break;
	case MEMVIEW_FILLRAM:
		*(uint16_t*)(&Memory.FillRAM[address]) = value;
		break;
	}
}

void memWrite32 (RegionType regionType, HWAddressType address, uint32_t value)
{
	memWrite16(regionType, address, value & 0xffff);
	memWrite16(regionType, address + 2, (value >> 16) & 0xffff);
}

//////////////////////////////////////////////////////////////////////////////

CMemView::CMemView()
	: CToolWindow(IDD_MEM_VIEW, MemView_DlgProc, "Memory View")
	, region(MEMVIEW_RAM)
	, viewMode(0)
	, sel(FALSE)
	, selPart(0)
	, selAddress(0x00000000)
	, selNewVal(0x00000000)
{
	// initialize memory regions
	if (s_memoryRegions.empty())
	{
		s_memoryRegions.push_back(s_ramRegion);
		s_memoryRegions.push_back(s_sramRegion);
		s_memoryRegions.push_back(s_fillRAMRegion);
	}
	address = s_memoryRegions.front().hardwareAddress;

	PostInitialize();
}

CMemView::~CMemView()
{
	DestroyWindow(hWnd);
	hWnd = NULL;

	UnregWndClass("MemView_ViewBox");
}

//////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK MemView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMemView* wnd = (CMemView*)GetWindowLongPtr(hDlg, DWLP_USER);
	if((wnd == NULL) && (uMsg != WM_INITDIALOG))
		return 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND cur;

			wnd = (CMemView*)lParam;
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG)wnd);
			SetWindowLongPtr(GetDlgItem(hDlg, IDC_MEMVIEWBOX), DWLP_USER, (LONG)wnd);

			wnd->font = CreateFont(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, GetFontQuality(), FIXED_PITCH, "Courier New");

			MemViewRegion& region = s_memoryRegions[wnd->region];

			cur = GetDlgItem(hDlg, IDC_REGION);
			for(MemoryList::iterator iter = s_memoryRegions.begin(); iter != s_memoryRegions.end(); ++iter)
			{
				SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)iter->name);
			}
			SendMessage(cur, CB_SETCURSEL, 0, 0);

			cur = GetDlgItem(hDlg, IDC_VIEWMODE);
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Bytes");
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Halfwords");
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Words");
			SendMessage(cur, CB_SETCURSEL, 0, 0);

			cur = GetDlgItem(hDlg, IDC_ADDRESS);
			SendMessage(cur, EM_SETLIMITTEXT, 8, 0);
			TCHAR addressText[9];
			wsprintf(addressText, _T("%08X"), wnd->address);
			SetWindowText(cur, addressText);

			SetScrollRange(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, (region.size - 1) >> 4, TRUE);
			SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, TRUE);

			wnd->Refresh();
		}
		return 1;

	case WM_CLOSE:
		CloseToolWindow(wnd);
		return 1;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			CloseToolWindow(wnd);
			return 1;

		case IDC_REGION:
			if ((HIWORD(wParam) == CBN_SELCHANGE) || (HIWORD(wParam) == CBN_CLOSEUP))
			{
				wnd->region = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

				MemViewRegion& region = s_memoryRegions[wnd->region];
				wnd->address = region.hardwareAddress;
				SetScrollRange(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, (region.size - 1) >> 4, TRUE);
				SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, TRUE);

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				wnd->Refresh();
			}
			return 1;

		case IDC_VIEWMODE:
			if ((HIWORD(wParam) == CBN_SELCHANGE) || (HIWORD(wParam) == CBN_CLOSEUP))
			{
				wnd->viewMode = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				wnd->Refresh();
			}
			return 1;

		case IDC_GO:
			{
				char addrstr[9];
				int len;
				int i;
				int shift;
				BOOL error = FALSE;
				u32 address = 0x00000000;
				MemViewRegion& region = s_memoryRegions[wnd->region];
				HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
				HWAddressType addrMax = max(addrMin, (region.hardwareAddress + region.size - 0x100 - 1) & 0xFFFFFF00);

				len = GetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), addrstr, 9);

				for(i = 0; i < len; i++)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						continue;

					if((ch >= 'A') && (ch <= 'F'))
						continue;

					if((ch >= 'a') && (ch <= 'f'))
						continue;

					if(ch == '\0')
						break;

					error = TRUE;
					break;
				}

				if(error)
				{
					MessageBox(hDlg, "Error:\nInvalid address specified.\nThe address must be an hexadecimal value.", "DeSmuME", (MB_OK | MB_ICONERROR));
					SetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), "");
					return 1;
				}

				for(i = (len-1), shift = 0; i >= 0; i--, shift += 4)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						address |= ((ch - '0') << shift);
					else if((ch >= 'A') && (ch <= 'F'))
						address |= ((ch - 'A' + 0xA) << shift);
					else if((ch >= 'a') && (ch <= 'f'))
						address |= ((ch - 'a' + 0xA) << shift);
				}

				wnd->address = max((u32)addrMin, min((u32)addrMax, (address & 0xFFFFFFF0)));

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
				wnd->Refresh();
			}
			return 1;

		case IDC_TEXTDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "txt";
				ofn.Flags = OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

				if(GetSaveFileName(&ofn))
				{
					FILE *f;
					u8 memory[0x100];
					int line;

					memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);

					f = fopen(fileName, "a");

					for(line = 0; line < 16; line++)
					{
						int i;
						
						fprintf(f, "%08X\t\t", (wnd->address + (line << 4)));

						switch(wnd->viewMode)
						{
						case 0:
							{
								for(i = 0; i < 16; i++)
								{
									fprintf(f, "%02X ", memory[(line << 4) + i]);
								}
								fprintf(f, "\t");
							}
							break;

						case 1:
							{
								for(i = 0; i < 16; i += 2)
								{
									fprintf(f, "%04X ", *(uint16_t*)(&memory[(line << 4) + i]));
								}
								fprintf(f, "\t\t");
							}
							break;

						case 2:
							{
								for(i = 0; i < 16; i += 4)
								{
									fprintf(f, "%08X ", *(uint32_t*)(&memory[(line << 4) + i]));
								}
								fprintf(f, "\t\t\t");
							}
							break;
						}

						for(i = 0; i < 16; i++)
						{
							u8 val = memory[(line << 4) + i];
									
							if((val >= 32) && (val <= 127))
								fprintf(f, "%c", (char)val);
							else
								fprintf(f, ".");
						}
						fprintf(f, "\n");
					}

					fclose(f);
				}
			}
			return 1;

		case IDC_DUMPALL:
		case IDC_RAWDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Binary file (*.bin)\0*.bin\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "bin";
				ofn.Flags = OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

				if(GetSaveFileName(&ofn))
				{
					if(LOWORD(wParam) == IDC_RAWDUMP)
					{
						FILE *f = fopen(fileName, "ab");
						if (f)
						{
							u8 memory[0x100];
							memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);
							fwrite(memory, 0x100, 1, f);
							fclose(f);
						}
					}
					else
					{
						const size_t blocksize = 0x100;
						byte* memory = new byte[blocksize];
						if (memory)
						{
							FILE *f = fopen(fileName, "wb");
							if (f)
							{
								MemViewRegion& region = s_memoryRegions[wnd->region];
								for (HWAddressType address = region.hardwareAddress;
									address < region.hardwareAddress + region.size; address += blocksize)
								{
									size_t size = blocksize;
									if (address + size > region.hardwareAddress + region.size)
									{
										size = region.size - (address - region.hardwareAddress);
									}
									memRead(memory, (RegionType)wnd->region, address, size);
									fwrite(memory, size, 1, f);
								}
								fclose(f);
							}
							delete [] memory;
						}
					}
				}
			}
			return 1;
		}
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT MemView_ViewBoxPaint(CMemView* wnd, HWND hCtl, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;
	RECT			rc;
	int				w, h;
	SIZE			fontsize;
	HDC				mem_hdc;
	HBITMAP			mem_bmp;
	u32 			addr = wnd->address;
	u8 				memory[0x100];
	char 			text[80];
	int 			startx;
	int 			curx, cury;
	int 			line;

	GetClientRect(hCtl, &rc);
	w = (rc.right - rc.left);
	h = (rc.bottom - rc.top);

	hdc = BeginPaint(hCtl, &ps);

	mem_hdc = CreateCompatibleDC(hdc);
	mem_bmp = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(mem_hdc, mem_bmp);

	SelectObject(mem_hdc, wnd->font);

	SetBkMode(mem_hdc, OPAQUE);
	SetBkColor(mem_hdc, RGB(255, 255, 255));
	SetTextColor(mem_hdc, RGB(0, 0, 0));

	FillRect(mem_hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	GetTextExtentPoint32(mem_hdc, " ", 1, &fontsize);
	startx = ((fontsize.cx * 8) + 5);
	curx = 0;
	cury = (fontsize.cy + 3);

	MoveToEx(mem_hdc, ((fontsize.cx * 8) + 2), 0, NULL);
	LineTo(mem_hdc, ((fontsize.cx * 8) + 2), h);

	MoveToEx(mem_hdc, 0, (fontsize.cy + 1), NULL);
	LineTo(mem_hdc, w, (fontsize.cy + 1));

	switch(wnd->viewMode)
	{
	case 0: sprintf(text, "   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   0123456789ABCDEF"); break;
	case 1: sprintf(text, "       0 1  2 3  4 5  6 7  8 9  A B  C D  E F       0123456789ABCDEF"); break;
	case 2: sprintf(text, "         0 1 2 3  4 5 6 7  8 9 A B  C D E F         0123456789ABCDEF"); break;
	}
	TextOut(mem_hdc, startx, 0, text, strlen(text));
	
	memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);

	for(line = 0; line < 16; line++, addr += 0x10)
	{
		int i;

		sprintf(text, "%08X", addr);
		TextOut(mem_hdc, 0, cury, text, strlen(text));

		curx = startx;

		switch(wnd->viewMode)
		{
		case 0:
			{
				curx += (fontsize.cx * 2);
				for(i = 0; i < 16; i++)
				{
					u8 val = memory[(line << 4) + i];
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%02X", val); break;
						case 1: sprintf(text, "%01X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%02X", val);
					}

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (2+1));
				}
				curx += (fontsize.cx * 2);
			}
			break;

		case 1:
			{
				curx += (fontsize.cx * 6);
				for(i = 0; i < 16; i += 2)
				{
					u16 val = *(uint16_t*)(&memory[(line << 4) + i]);
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%04X", val); break;
						case 1: sprintf(text, "%01X...", wnd->selNewVal); break;
						case 2: sprintf(text, "%02X..", wnd->selNewVal); break;
						case 3: sprintf(text, "%03X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%04X", val);
					}

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (4+1));
				}
				curx += (fontsize.cx * 6);
			}
			break;

		case 2:
			{
				curx += (fontsize.cx * 8);
				for(i = 0; i < 16; i += 4)
				{
					u32 val = *(uint32_t*)(&memory[(line << 4) + i]);
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%08X", val); break;
						case 1: sprintf(text, "%01X.......", wnd->selNewVal); break;
						case 2: sprintf(text, "%02X......", wnd->selNewVal); break;
						case 3: sprintf(text, "%03X.....", wnd->selNewVal); break;
						case 4: sprintf(text, "%04X....", wnd->selNewVal); break;
						case 5: sprintf(text, "%05X...", wnd->selNewVal); break;
						case 6: sprintf(text, "%06X..", wnd->selNewVal); break;
						case 7: sprintf(text, "%07X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%08X", val);
					}	

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (8+1));
				}
				curx += (fontsize.cx * 8);
			}
			break;
		}

		SetBkColor(mem_hdc, RGB(255, 255, 255));
		SetTextColor(mem_hdc, RGB(0, 0, 0));

		for(i = 0; i < 16; i++)
		{
			u8 val = memory[(line << 4) + i];

			if((val >= 32) && (val <= 127))
				text[i] = (char)val;
			else
				text[i] = '.';
		}
		text[16] = '\0';
		TextOut(mem_hdc, curx, cury, text, strlen(text));

		cury += fontsize.cy;
	}

	BitBlt(hdc, 0, 0, w, h, mem_hdc, 0, 0, SRCCOPY);

	DeleteDC(mem_hdc);
	DeleteObject(mem_bmp);

	EndPaint(hCtl, &ps);

	return 0;
}

LRESULT CALLBACK MemView_ViewBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMemView* wnd = (CMemView*)GetWindowLongPtr(hCtl, DWLP_USER);

	switch(uMsg)
	{
	case WM_NCCREATE:
		SetScrollRange(hCtl, SB_VERT, 0x00000000, 0x000FFFF0, TRUE);
		SetScrollPos(hCtl, SB_VERT, 0x00000000, TRUE);
		return 1;

	case WM_NCDESTROY:
		return 1;
	
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		MemView_ViewBoxPaint(wnd, hCtl, wParam, lParam);
		return 1;

	case WM_LBUTTONDOWN:
		{
			HDC hdc;
			HFONT font;
			SIZE fontsize;
			int x, y;

			wnd->sel = FALSE;
			wnd->selAddress = 0x00000000;
			wnd->selPart = 0;
			wnd->selNewVal = 0x00000000;

			hdc = GetDC(hCtl);
			font = (HFONT)SelectObject(hdc, wnd->font);
			GetTextExtentPoint32(hdc, " ", 1, &fontsize);
			
			x = LOWORD(lParam);
			y = HIWORD(lParam);

			if((x >= ((fontsize.cx * 8) + 5)) && (y >= (fontsize.cy + 3)))
			{
				int line, col;

				x -= ((fontsize.cx * 8) + 5);
				y -= (fontsize.cy + 3);
				
				line = (y / fontsize.cy);

				switch(wnd->viewMode)
				{
				case 0:
					{
						if((x < (fontsize.cx * 2)) || (x >= (fontsize.cx * (2 + ((2+1) * 16)))))
							break;

						col = ((x - (fontsize.cx * 2)) / (fontsize.cx * (2+1)));

						wnd->sel = TRUE;
						
					}
					break;

				case 1:
					{
						if((x < (fontsize.cx * 6)) || (x >= (fontsize.cx * (6 + ((4+1) * 8)))))
							break;

						col = ((x - (fontsize.cx * 6)) / (fontsize.cx * (4+1)) * 2);

						wnd->sel = TRUE;
						
					}
					break;

				case 2:
					{
						if((x < (fontsize.cx * 8)) || (x >= (fontsize.cx * (8 + ((8+1) * 4)))))
							break;

						col = ((x - (fontsize.cx * 8)) / (fontsize.cx * (8+1)) * 4);

						wnd->sel = TRUE;
						
					}
					break;
				}
				
				wnd->selAddress = (wnd->address + (line << 4) + col);
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;
			}

			SelectObject(hdc, font);
			ReleaseDC(hCtl, hdc);

			SetFocus(hCtl);				/* Required to receive keyboard messages */
			wnd->Refresh();
		}
		return 1;

	case WM_CHAR:
		{
			char ch = (char)wParam;

			if(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')) || ((ch >= 'a') && (ch <= 'f')))
			{
				//if ((wnd->region == ARMCPU_ARM7) && ((wnd->selAddress & 0xFFFF0000) == 0x04800000))
				//	return DefWindowProc(hCtl, uMsg, wParam, lParam);

				u8 maxSelPart[3] = {2, 4, 8};

				wnd->selNewVal <<= 4;
				wnd->selPart++;

				if((ch >= '0') && (ch <= '9'))
					wnd->selNewVal |= (ch - '0');
				else if((ch >= 'A') && (ch <= 'F'))
					wnd->selNewVal |= (ch - 'A' + 0xA);
				else if((ch >= 'a') && (ch <= 'f'))
					wnd->selNewVal |= (ch - 'a' + 0xA);

				if(wnd->selPart >= maxSelPart[wnd->viewMode])
				{
					switch(wnd->viewMode)
					{
					case 0: memWrite8((RegionType)wnd->region, wnd->selAddress, (u8)wnd->selNewVal); wnd->selAddress++; break;
					case 1: memWrite16((RegionType)wnd->region, wnd->selAddress, (u16)wnd->selNewVal); wnd->selAddress += 2; break;
					case 2: memWrite32((RegionType)wnd->region, wnd->selAddress, wnd->selNewVal); wnd->selAddress += 4; break;
					}
					wnd->selPart = 0;
					wnd->selNewVal = 0x00000000;

					if(wnd->selAddress == 0x00000000)
					{
						wnd->sel = FALSE;
					}
					else if(wnd->selAddress >= (wnd->address + 0x100))
					{
						MemViewRegion& region = s_memoryRegions[wnd->region];
						HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
						HWAddressType addrMax = max(addrMin, (region.hardwareAddress + region.size - 0x100 - 1) & 0xFFFFFF00);
						if (wnd->address + 0x10 <= addrMax)
						{
							wnd->address += 0x10;
							SetScrollPos(hCtl, SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
						}
						else
						{
							switch(wnd->viewMode)
							{
							case 0: wnd->selAddress--; break;
							case 1: wnd->selAddress -= 2; break;
							case 2: wnd->selAddress -= 4; break;
							}
						}
					}
				}
			}

			wnd->Refresh();
		}
		return 1;

	case WM_VSCROLL:
		{
			int firstpos = GetScrollPos(hCtl, SB_VERT);
			MemViewRegion& region = s_memoryRegions[wnd->region];
			HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
			HWAddressType addrMax = (region.hardwareAddress + region.size - 1) & 0xFFFFFF00;

			switch(LOWORD(wParam))
			{
			case SB_LINEUP:
				wnd->address = (u32)max((int)addrMin, ((int)wnd->address - 0x10));
				break;

			case SB_LINEDOWN:
				wnd->address = min((u32)addrMax, (wnd->address + 0x10));
				break;

			case SB_PAGEUP:
				wnd->address = (u32)max((int)addrMin, ((int)wnd->address - 0x100));
				break;

			case SB_PAGEDOWN:
				wnd->address = min((u32)addrMax, (wnd->address + 0x100));
				break;

			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				{
					SCROLLINFO si;
					
					ZeroMemory(&si, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;

					GetScrollInfo(hCtl, SB_VERT, &si);

					wnd->address = min((u32)addrMax, (wnd->address + ((si.nTrackPos - firstpos) * 16)));
				}
				break;
			}

			if((wnd->selAddress < wnd->address) || (wnd->selAddress >= (wnd->address + 0x100)))
			{
				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;
			}

			SetScrollPos(hCtl, SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
			wnd->Refresh();
		}
		return 1;
	}

	return DefWindowProc(hCtl, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
