/*
 * MPTrack.cpp
 * -----------
 * Purpose: OpenMPT core application class.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "SynMPTTheme.h"
#include <map>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")
#include "Mptrack.h"
#include "AboutDialog.h"
#include "AutoSaver.h"
#include "Childfrm.h"
#include "CloseMainDialog.h"
#include "DialogBase.h"
#include "ExceptionHandler.h"
#include "FileDialog.h"
#include "FolderScanner.h"
#include "Globals.h"
#include "Image.h"
#include "InputHandler.h"
#include "IPCWindow.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "ModDocTemplate.h"
#include "MPTrackUtil.h"
#include "MPTrackWine.h"
#include "PlugNotFoundDlg.h"
#include "Reporting.h"
#include "resource.h"
#include "SettingsIni.h"
#include "TrackerSettings.h"
#include "UpdateCheck.h"
#include "WelcomeDialog.h"
#include "WineSoundDeviceStub.h"
#include "../common/ComponentManager.h"
#include "../common/mptStringBuffer.h"
#include "../common/version.h"
#include "../misc/mptOS.h"
#include "../soundlib/Dlsbank.h"
#include "../soundlib/plugins/PluginManager.h"
#include "../test/test.h"
#include "../test/synmpt/runtime.h"
#include "mpt/arch/arch.hpp"
#include "mpt/fs/common_directories.hpp"
#include "mpt/fs/fs.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/random/seed.hpp"
#include "mpt/string/utility.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"

#include <thread>

#if MPT_MSVC_AT_LEAST(2022, 2) && MPT_MSVC_BEFORE(2022, 3)
// Work-around <https://developercommunity.visualstudio.com/t/warning-C4311-in-MFC-header-afxrecovery/10041328>,
// see <https://developercommunity.visualstudio.com/t/Compiler-warnings-after-upgrading-to-17/10036311#T-N10061908>.
template <class ARG_KEY>
AFX_INLINE UINT AFXAPI HashKey(ARG_KEY key);
template <>
AFX_INLINE UINT AFXAPI HashKey<CDocument*>(CDocument *key)
{
	// (algorithm copied from STL hash in xfunctional)
#pragma warning(suppress: 4302) // 'type cast' : truncation
#pragma warning(suppress: 4311) // pointer truncation
	ldiv_t HashVal = ldiv((long)(CDocument*)key, 127773);
	HashVal.rem = 16807 * HashVal.rem - 2836 * HashVal.quot;
	if(HashVal.rem < 0)
		HashVal.rem += 2147483647;
	return ((UINT)HashVal.rem);
}
#endif
#include <afxdatarecovery.h>

// GDI+
#include <atlbase.h>
#if MPT_MSVC_BEFORE(2019, 11)  // really < Windows 10 SDK 2104 (10.0.20348.0)
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#if MPT_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4458)  // declaration of 'x' hides class member
#endif
#include <gdiplus.h>
#if MPT_COMPILER_MSVC
#pragma warning(pop)
#endif
#if MPT_MSVC_BEFORE(2019, 11)  // really < Windows 10 SDK 2104 (10.0.20348.0)
#undef min
#undef max
#endif

#if MPT_COMPILER_MSVC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif


OPENMPT_NAMESPACE_BEGIN

/////////////////////////////////////////////////////////////////////////////
// The one and only CTrackApp object

CTrackApp theApp;

const TCHAR *szSpecialNoteNamesMPT[] = {_T("PCs"), _T("PC"), _T("~~ (Note Fade)"), _T("^^ (Note Cut)"), _T("== (Note Off)")};
const TCHAR *szSpecialNoteShortDesc[] = {_T("Param Control (Smooth)"), _T("Param Control"), _T("Note Fade"), _T("Note Cut"), _T("Note Off")};

// Make sure that special note arrays include string for every note.
static_assert(NOTE_MAX_SPECIAL - NOTE_MIN_SPECIAL + 1 == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);
static_assert(mpt::array_size<decltype(szSpecialNoteShortDesc)>::size == mpt::array_size<decltype(szSpecialNoteNamesMPT)>::size);

const char *szHexChar = "0123456789ABCDEF";


#ifdef MPT_WITH_ASIO
class ComponentASIO
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentASIO, "ASIO")
public:
	ComponentASIO() = default;
	virtual ~ComponentASIO() = default;
};
#endif // MPT_WITH_ASIO

#if defined(MPT_WITH_DIRECTSOUND)
class ComponentDirectSound 
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentDirectSound, "DirectSound")
public:
	ComponentDirectSound() = default;
	virtual ~ComponentDirectSound() = default;
};
#endif // MPT_WITH_DIRECTSOUND

#if defined(MPT_WITH_PORTAUDIO)
class ComponentPortAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPortAudio, "PortAudio")
public:
	ComponentPortAudio() = default;
	virtual ~ComponentPortAudio() = default;
};
#endif // MPT_WITH_PORTAUDIO

#if defined(MPT_WITH_PULSEAUDIO)
class ComponentPulseaudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudio, "Pulseaudio")
public:
	ComponentPulseaudio() = default;
	virtual ~ComponentPulseaudio() = default;
};
#endif // MPT_WITH_PULSEAUDIO

#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
class ComponentPulseaudioSimple
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentPulseaudioSimple, "PulseaudioSimple")
public:
	ComponentPulseaudioSimple() = default;
	virtual ~ComponentPulseaudioSimple() = default;
};
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE

#if defined(MPT_WITH_RTAUDIO)
class ComponentRtAudio
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentRtAudio, "RtAudio")
public:
	ComponentRtAudio() = default;
	virtual ~ComponentRtAudio() = default;
};
#endif // MPT_WITH_RTAUDIO

#if MPT_OS_WINDOWS
class ComponentWaveOut
	: public ComponentBuiltin
{
	MPT_DECLARE_COMPONENT_MEMBERS(ComponentWaveOut, "WaveOut")
public:
	ComponentWaveOut() = default;
	virtual ~ComponentWaveOut() = default;
};
#endif // MPT_OS_WINDOWS

struct AllSoundDeviceComponents
{
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
	ComponentHandle<ComponentPulseaudio> m_Pulseaudio;
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
	ComponentHandle<ComponentPulseaudioSimple> m_PulseaudioSimple;
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#if MPT_OS_WINDOWS
	ComponentHandle<ComponentWaveOut> m_WaveOut;
#endif // MPT_OS_WINDOWS
#if defined(MPT_WITH_DIRECTSOUND)
	ComponentHandle<ComponentDirectSound> m_DirectSound;
#endif // MPT_WITH_DIRECTSOUND
#ifdef MPT_WITH_ASIO
	ComponentHandle<ComponentASIO> m_ASIO;
#endif // MPT_WITH_ASIO
#ifdef MPT_WITH_PORTAUDIO
	ComponentHandle<ComponentPortAudio> m_PortAudio;
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
	ComponentHandle<ComponentRtAudio> m_RtAudio;
#endif // MPT_WITH_RTAUDIO
	operator SoundDevice::EnabledBackends() const
	{
		SoundDevice::EnabledBackends result;
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_ENABLE_PULSEAUDIO_FULL)
		result.Pulseaudio = IsComponentAvailable(m_PulseAudio);
#endif // MPT_WITH_PULSEAUDIO && MPT_ENABLE_PULSEAUDIO_FULL
#if defined(MPT_WITH_PULSEAUDIO) && defined(MPT_WITH_PULSEAUDIOSIMPLE)
		result.PulseaudioSimple = IsComponentAvailable(m_PulseAudioSimple);
#endif // MPT_WITH_PULSEAUDIO && MPT_WITH_PULSEAUDIOSIMPLE
#if MPT_OS_WINDOWS
		result.WaveOut = IsComponentAvailable(m_WaveOut);
#endif // MPT_OS_WINDOWS
#if defined(MPT_WITH_DIRECTSOUND)
		result.DirectSound = IsComponentAvailable(m_DirectSound);
#endif // MPT_WITH_DIRECTSOUND
#ifdef MPT_WITH_ASIO
		result.ASIO = IsComponentAvailable(m_ASIO);
#endif // MPT_WITH_ASIO
#ifdef MPT_WITH_PORTAUDIO
		result.PortAudio = IsComponentAvailable(m_PortAudio);
#endif // MPT_WITH_PORTAUDIO
#ifdef MPT_WITH_RTAUDIO
		result.RtAudio = IsComponentAvailable(m_RtAudio);
#endif // MPT_WITH_RTAUDIO
		return result;
	}
};


void CTrackApp::OnFileCloseAll()
{
	if(!(TrackerSettings::Instance().patternSetup & PatternSetup::NoCustomCloseDialog))
	{
		// Show modified documents window
		CloseMainDialog dlg;
		if(dlg.DoModal() != IDOK)
		{
			return;
		}
	}

	for(auto &doc : GetOpenDocuments())
	{
		doc->SafeFileClose();
	}
}


void CTrackApp::OnUpdateAnyDocsOpen(CCmdUI *cmd)
{
	cmd->Enable(!GetModDocTemplate()->empty());
}


int CTrackApp::GetOpenDocumentCount() const
{
	return static_cast<int>(GetModDocTemplate()->size());
}


// Retrieve a list of all open modules.
std::vector<CModDoc *> CTrackApp::GetOpenDocuments() const
{
	std::vector<CModDoc *> documents;
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		POSITION pos = pDocTmpl->GetFirstDocPosition();
		CDocument *pDoc;
		while((pos != nullptr) && ((pDoc = pDocTmpl->GetNextDoc(pos)) != nullptr))
		{
			documents.push_back(dynamic_cast<CModDoc *>(pDoc));
		}
	}

	return documents;
}


void CTrackApp::UpdateAllViews(UpdateHint hint, CObject *pHint)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->UpdateAllViews(nullptr, hint, pHint);
		}
	}
}


void CTrackApp::PostMessageToAllViews(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if(auto *pDocTmpl = GetModDocTemplate())
	{
		for(auto &doc : *pDocTmpl)
		{
			doc->PostMessageToAllViews(uMsg, wParam, lParam);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Command Line options

class CMPTCommandLineInfo: public CCommandLineInfo
{
public:
	std::vector<mpt::PathString> m_fileNames;
	bool m_noDls = false, m_noPlugins = false, m_noAssembly = false, m_noSysCheck = false, m_noWine = false,
		m_portable = false, m_noCrashHandler = false, m_debugCrashHandler = false, m_sharedInstance = false,
		m_autoPlay = false, m_synmptChecks = false;
#ifdef ENABLE_TESTS
	bool m_noTests = false;
#endif

public:
	CMPTCommandLineInfo()
	{
		std::vector<TCHAR> curDir(::GetCurrentDirectory(0, nullptr), _T('\0'));
		::GetCurrentDirectory(static_cast<DWORD>(curDir.size()), curDir.data());
		m_workingDir = mpt::PathString::FromNative(curDir.data());
	}

	void ParseParam(LPCTSTR param, BOOL isFlag, BOOL isLast) override
	{
		if(isFlag)
		{
			if(!lstrcmpi(param, _T("synmptChecks"))) { m_synmptChecks = true; return; }
			if(!lstrcmpi(param, _T("nologo"))) { m_bShowSplash = FALSE; return; }
			if(!lstrcmpi(param, _T("nodls"))) { m_noDls = true; return; }
			if(!lstrcmpi(param, _T("noplugs"))) { m_noPlugins = true; return; }
			if(!lstrcmpi(param, _T("portable"))) { m_portable = true; return; }
			if(!lstrcmpi(param, _T("fullMemDump"))) { ExceptionHandler::fullMemDump = true; return; }
			if(!lstrcmpi(param, _T("noAssembly"))) { m_noAssembly = true; return; }
			if(!lstrcmpi(param, _T("noSysCheck"))) { m_noSysCheck = true; return; }
			if(!lstrcmpi(param, _T("noWine"))) { m_noWine = true; return; }
			if(!lstrcmpi(param, _T("noCrashHandler"))) { m_noCrashHandler = true; return; }
			if(!lstrcmpi(param, _T("DebugCrashHandler"))) { m_debugCrashHandler = true; return; }
			if(!lstrcmpi(param, _T("shared"))) { m_sharedInstance = true; return; }
			if(!lstrcmpi(param, _T("play"))) { m_autoPlay = true; return; }
#ifdef ENABLE_TESTS
			if (!lstrcmpi(param, _T("noTests"))) { m_noTests = true; return; }
#endif
		} else
		{
			m_fileNames.push_back(mpt::RelativePathToAbsolute(mpt::PathString::FromNative(param), m_workingDir));
			if(m_nShellCommand == FileNew) m_nShellCommand = FileOpen;
		}
		CCommandLineInfo::ParseParam(param, isFlag, isLast);
	}

protected:
	mpt::PathString m_workingDir;
};


// Splash Screen

static void StartSplashScreen();
static void StopSplashScreen();
static void TimeoutSplashScreen();


/////////////////////////////////////////////////////////////////////////////
// Midi Library

MidiLibrary CTrackApp::midiLibrary;

void CTrackApp::ImportMidiConfig(const mpt::PathString &filename, bool hideWarning)
{
	if(filename.empty()) return;

	if(CDLSBank::IsDLSBank(filename))
	{
		ConfirmAnswer result = cnfYes;
		if(!hideWarning)
		{
			result = Reporting::Confirm("You are about to replace the current MIDI library:\n"
				"Do you want to replace only the missing instruments? (recommended)",
				"Warning", true);
		}
		if(result == cnfCancel) return;
		const bool replaceAll = (result == cnfNo);
		CDLSBank dlsbank;
		if (dlsbank.Open(filename))
		{
			for(uint32 ins = 0; ins < 256; ins++)
			{
				if(replaceAll || !midiLibrary[ins] || midiLibrary[ins]->empty())
				{
					uint32 prog = (ins < 128) ? ins : 0xFF;
					uint32 key = (ins < 128) ? 0xFF : ins & 0x7F;
					if(dlsbank.FindInstrument(ins >= 128, 0xFFFF, prog, key))
					{
						midiLibrary[ins] = filename;
					}
				}
			}
		}
		return;
	}

	IniFileSettingsContainer file(filename);
	ImportMidiConfig(file, filename.GetDirectoryWithDrive());
}


static mpt::PathString GetUltraSoundPatchDir(SettingsContainer &file, const mpt::ustring &iniSection, const mpt::PathString &path, bool forgetSettings)
{
	mpt::PathString patchDir = file.Read<mpt::PathString>(iniSection, U_("PatchDir"), {});
	if(forgetSettings)
		file.Forget(U_("Ultrasound"), U_("PatchDir"));
	if(patchDir.empty() || patchDir == P_(".\\"))
		patchDir = path;
	if(!patchDir.empty())
		patchDir = patchDir.WithTrailingSlash();
	return patchDir;
}

void CTrackApp::ImportMidiConfig(SettingsContainer &file, const mpt::PathString &path, bool forgetSettings)
{
	const mpt::PathString patchDir = GetUltraSoundPatchDir(file, U_("Ultrasound"), path, forgetSettings);
	for(uint32 prog = 0; prog < 256; prog++)
	{
		mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? U_("Midi") : U_("Perc"), prog & 0x7F);
		mpt::PathString filename = file.Read<mpt::PathString>(U_("Midi Library"), key, mpt::PathString());
		// Check for ULTRASND.INI
		if(filename.empty())
		{
			mpt::ustring section = (prog < 128) ? UL_("Melodic Patches") : UL_("Drum Patches");
			key = mpt::ufmt::val(prog & 0x7f);
			filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
			if(forgetSettings) file.Forget(section, key);
			if(filename.empty())
			{
				section = (prog < 128) ? UL_("Melodic Bank 0") : UL_("Drum Bank 0");
				filename = file.Read<mpt::PathString>(section, key, mpt::PathString());
				if(forgetSettings) file.Forget(section, key);
			}
			const mpt::PathString localPatchDir = GetUltraSoundPatchDir(file, section, patchDir, forgetSettings);
			if(!filename.empty())
			{
				filename = localPatchDir + filename + P_(".pat");
			}
		}

		if(filename == P_("*empty"))
			midiLibrary[prog] = P_("");
		else if(!filename.empty())
			midiLibrary[prog] = theApp.PathInstallRelativeToAbsolute(filename);
	}
}


void CTrackApp::ExportMidiConfig(const mpt::PathString &filename)
{
	if(filename.empty())
		return;
	IniFileSettingsContainer file(filename);
	ExportMidiConfig(file);
}

void CTrackApp::ExportMidiConfig(SettingsContainer &file)
{
	for(uint32 prog = 0; prog < 256; prog++)
	{
		const mpt::ustring key = MPT_UFORMAT("{}{}")((prog < 128) ? U_("Midi") : U_("Perc"), prog & 0x7F);
		if(!midiLibrary[prog])
		{
			file.Forget(U_("Midi Library"), key);
			continue;
		}

		mpt::PathString fileName = *midiLibrary[prog];
		if(midiLibrary[prog]->empty())
			fileName = P_("*empty");
		else if(theApp.IsPortableMode())
			fileName = theApp.PathAbsoluteToInstallRelative(fileName);
		file.Write<mpt::PathString>(U_("Midi Library"), key, fileName);
	}
}


/////////////////////////////////////////////////////////////////////////////
// DLS Banks support

std::vector<std::unique_ptr<CDLSBank>> CTrackApp::gpDLSBanks;


struct CompareLessPathStringNoCase
{
	inline bool operator()(const mpt::PathString &l, const mpt::PathString &r) const
	{
		return mpt::PathCompareNoCase(l, r) < 0;
	}
};

std::future<std::vector<std::unique_ptr<CDLSBank>>> CTrackApp::LoadDefaultDLSBanks()
{
	std::set<mpt::PathString, CompareLessPathStringNoCase> paths;

	uint32 numBanks = theApp.GetSettings().Read<uint32>(U_("DLS Banks"), U_("NumBanks"), 0);
	for(uint32 i = 0; i < numBanks; i++)
	{
		mpt::PathString path = theApp.GetSettings().Read<mpt::PathString>(U_("DLS Banks"), MPT_UFORMAT("Bank{}")(i + 1), mpt::PathString());
		paths.insert(theApp.PathInstallRelativeToAbsolute(path));
	}

	HKEY key;
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectMusic"), 0, KEY_READ, &key) == ERROR_SUCCESS)
	{
		DWORD dwRegType = REG_SZ;
		DWORD dwSize = 0;
		if(RegQueryValueEx(key, _T("GMFilePath"), NULL, &dwRegType, nullptr, &dwSize) == ERROR_SUCCESS && dwSize > 0)
		{
			std::vector<TCHAR> filenameT(dwSize / sizeof(TCHAR));
			if(RegQueryValueEx(key, _T("GMFilePath"), NULL, &dwRegType, reinterpret_cast<LPBYTE>(filenameT.data()), &dwSize) == ERROR_SUCCESS)
			{
				mpt::winstring filenamestr = ParseMaybeNullTerminatedStringFromBufferWithSizeInBytes<mpt::winstring>(filenameT.data(), dwSize);
				std::vector<TCHAR> filenameExpanded(::ExpandEnvironmentStrings(filenamestr.c_str(), nullptr, 0));
				::ExpandEnvironmentStrings(filenamestr.c_str(), filenameExpanded.data(), static_cast<DWORD>(filenameExpanded.size()));
				auto filename = mpt::PathString::FromNative(filenameExpanded.data());
				ImportMidiConfig(filename, true);
				paths.insert(std::move(filename));
			}
		}
		RegCloseKey(key);
	}

	if(paths.empty())
		return {};

	return std::async(std::launch::async, [paths = std::move(paths)]()
	{
		std::vector<std::unique_ptr<CDLSBank>> banks;
		banks.reserve(paths.size());
		for(const auto &filename : paths)
		{
			if(filename.empty() || !CDLSBank::IsDLSBank(filename))
				continue;
			try
			{
				auto bank = std::make_unique<CDLSBank>();
				if(bank->Open(filename))
				{
					banks.push_back(std::move(bank));
					continue;
				}
			} catch(mpt::out_of_memory e)
			{
				mpt::delete_out_of_memory(e);
			} catch(const std::exception &)
			{
			}
		}
		// Avoid the overhead of future::wait_for(0) until future::is_ready is finally non-experimental
		theApp.m_scannedDlsBanksAvailable = true;
		return banks;
	});
}


void CTrackApp::SaveDefaultDLSBanks()
{
	uint32 nBanks = 0;
	for(const auto &bank : gpDLSBanks)
	{
		if(!bank || bank->GetFileName().empty())
			continue;

		mpt::PathString path = bank->GetFileName();
		if(theApp.IsPortableMode())
		{
			path = theApp.PathAbsoluteToInstallRelative(path);
		}

		mpt::ustring key = MPT_UFORMAT("Bank{}")(nBanks + 1);
		theApp.GetSettings().Write<mpt::PathString>(U_("DLS Banks"), key, path);
		nBanks++;

	}
	theApp.GetSettings().Write<uint32>(U_("DLS Banks"), U_("NumBanks"), nBanks);
}


void CTrackApp::RemoveDLSBank(UINT nBank)
{
	if(nBank < gpDLSBanks.size())
		gpDLSBanks[nBank] = nullptr;
}


bool CTrackApp::AddDLSBank(const mpt::PathString &filename)
{
	if(filename.empty() || !CDLSBank::IsDLSBank(filename)) return false;
	// Check for dupes
	for(const auto &bank : gpDLSBanks)
	{
		if(bank && !mpt::PathCompareNoCase(filename, bank->GetFileName()))
			return true;
	}
	try
	{
		auto bank = std::make_unique<CDLSBank>();
		if(bank->Open(filename))
		{
			gpDLSBanks.push_back(std::move(bank));
			return true;
		}
	} catch(mpt::out_of_memory e)
	{
		mpt::delete_out_of_memory(e);
	} catch(const std::exception &)
	{
	}
	return false;
}


size_t CTrackApp::AddScannedDLSBanks()
{
	if(!m_scannedDlsBanks.valid())
		return 0;

	size_t numAdded = 0;
	auto scannedBanks = m_scannedDlsBanks.get();
	gpDLSBanks.reserve(gpDLSBanks.size() + scannedBanks.size());
	const size_t existingBanks = gpDLSBanks.size();
	for(auto &bank : scannedBanks)
	{
		if(std::find_if(gpDLSBanks.begin(), gpDLSBanks.begin() + existingBanks, [&bank](const auto &other) { return other && *bank == *other; }) == gpDLSBanks.begin() + existingBanks)
		{
			gpDLSBanks.push_back(std::move(bank));
			numAdded++;
		}
	}
	return numAdded;
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp

MODTYPE CTrackApp::m_nDefaultDocType = MOD_TYPE_IT;

BEGIN_MESSAGE_MAP(CTrackApp, CWinApp)
	//{{AFX_MSG_MAP(CTrackApp)
	ON_COMMAND(ID_FILE_NEW,       &CTrackApp::OnFileNew)
	ON_COMMAND(ID_FILE_NEWMOD,    &CTrackApp::OnFileNewMOD_Amiga)
	ON_COMMAND(ID_FILE_NEWMOD_PC, &CTrackApp::OnFileNewMOD_PC)
	ON_COMMAND(ID_FILE_NEWS3M,    &CTrackApp::OnFileNewS3M)
	ON_COMMAND(ID_FILE_NEWXM,     &CTrackApp::OnFileNewXM)
	ON_COMMAND(ID_FILE_NEWIT,     &CTrackApp::OnFileNewIT)
	ON_COMMAND(ID_NEW_MPT,        &CTrackApp::OnFileNewMPT)
	ON_COMMAND(ID_FILE_OPEN,      &CTrackApp::OnFileOpen)
	ON_COMMAND(ID_FILE_CLOSEALL,  &CTrackApp::OnFileCloseAll)
	ON_COMMAND(ID_APP_ABOUT,      &CTrackApp::OnAppAbout)
	ON_UPDATE_COMMAND_UI(ID_FILE_CLOSEALL, &CTrackApp::OnUpdateAnyDocsOpen)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTrackApp construction

CTrackApp::CTrackApp()
{
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART | AFX_RESTART_MANAGER_REOPEN_PREVIOUS_FILES;
}


CTrackApp::~CTrackApp()
{
#if !defined(MPT_LIBCXX_QUIRK_NO_CHRONO) && !defined(MPT_LIBCXX_QUIRK_NO_CHRONO_DATE) && defined(MPT_LIBCXX_QUIRK_CHRONO_TZ_MEMLEAK)
	// Work-around memleak (see <https://github.com/microsoft/STL/issues/2504#issuecomment-1068008937>)
	try
	{
#if defined(MODPLUG_TRACKER)
		if(mpt::OS::Windows::IsWine())
		{
			// <https://bugs.openmpt.org/view.php?id=1933>
			throw std::runtime_error("zoned_time is blacklisted on Wine");
		}
#endif
		std::chrono::get_tzdb_list().~tzdb_list();
	} catch(const std::exception &)
	{
		// nothing
	} catch(...)
	{
		// nothing
	}
#endif
}


class OpenMPTDataRecoveryHandler
	: public CDataRecoveryHandler
{
public:
	OpenMPTDataRecoveryHandler(_In_ DWORD dwRestartManagerSupportFlags, _In_ int nAutosaveInterval)
		: CDataRecoveryHandler(dwRestartManagerSupportFlags, nAutosaveInterval)
	{
		return;
	}
	~OpenMPTDataRecoveryHandler() override = default;

	BOOL SaveOpenDocumentList() override
	{
		BOOL bRet = TRUE;  // return FALSE if document list non-empty and not saved

		POSITION posAutosave = m_mapDocNameToAutosaveName.GetStartPosition();
		if (posAutosave != NULL)
		{
			bRet = FALSE;

			// Save the open document list and associated autosave info to the registry
			IniFileSettingsContainer ini(theApp.GetConfigPath() + P_("restart.") + mpt::PathString::FromCString(GetRestartIdentifier()) + P_(".ini"));
			int32 count = 0;
			while (posAutosave != NULL)
			{
				CString strDocument, strAutosave;
				m_mapDocNameToAutosaveName.GetNextAssoc(posAutosave, strDocument, strAutosave);

				ini.Write<mpt::ustring>({ U_("RestartDocument"), mpt::ufmt::val(count) }, mpt::ToUnicode(strDocument));
				ini.Write<mpt::ustring>({ U_("RestartAutosave"), mpt::ufmt::val(count) }, mpt::ToUnicode(strAutosave));
				count++;
			}
			ini.Write<int32>({ U_("Restart"), U_("Count") }, count);

			return TRUE;
		}

		return bRet;
	}

	BOOL ReopenPreviousDocuments() override
	{
		// In case no previously open documents are to be re-opened,
		// MFC RestartManager integration assumes the default action to be
		// opening a new empty document (see CWinApp::ProcessShellCommand()
		// and CWinApp::RestartInstance().
		// We do not want that, so we always return TRUE.
		CDataRecoveryHandler::ReopenPreviousDocuments();
		return TRUE;
	}

	BOOL ReadOpenDocumentList() override
	{
		{
			IniFileSettingsContainer ini(theApp.GetConfigPath() + P_("restart.") + mpt::PathString::FromCString(GetRestartIdentifier()) + P_(".ini"));
			int32 count = ini.Read<int32>({ U_("Restart"), U_("Count") }, 0);

			for(int32 index = 0; index < count; ++index)
			{
				mpt::ustring document = ini.Read<mpt::ustring>({ U_("RestartDocument"), mpt::ufmt::val(index) }, U_(""));
				mpt::ustring autosave = ini.Read<mpt::ustring>({ U_("RestartAutosave"), mpt::ufmt::val(index) }, U_(""));
				if(!document.empty())
				{
					m_mapDocNameToAutosaveName[mpt::ToCString(document)] = mpt::ToCString(autosave);
				}
			}

			ini.Remove(U_("Restart"));
			ini.Remove(U_("RestartDocument"));
			ini.Remove(U_("RestartAutosave"));

		}

		DeleteFile((theApp.GetConfigPath() + P_("restart.") + mpt::PathString::FromCString(GetRestartIdentifier()) + P_(".ini")).AsNative().c_str());

		// In case no previously open documents are found,
		// MFC RestartManager integration assumes the default action to be
		// opening a new empty document (see CWinApp::RestartInstance() and
		// after the call to RestartInstance()).
		// We do not want that, so we always return TRUE.
		return TRUE;

	}

};


CDataRecoveryHandler *CTrackApp::GetDataRecoveryHandler()
{
	static BOOL bTriedOnce = FALSE;

	// Since the application restart and application recovery are supported only on Windows
	// Vista and above, we don't need a recovery handler on Windows versions less than Vista.
	if (SupportsRestartManager() || SupportsApplicationRecovery())
	{
		if (!bTriedOnce && m_pDataRecoveryHandler == NULL)
		{
			m_pDataRecoveryHandler = new OpenMPTDataRecoveryHandler(m_dwRestartManagerSupportFlags, m_nAutosaveInterval);
			if (!m_pDataRecoveryHandler->Initialize())
			{
				delete m_pDataRecoveryHandler;
				m_pDataRecoveryHandler = NULL;
			}
		}
	}

	bTriedOnce = TRUE;
	return m_pDataRecoveryHandler;
}


CDocument *CTrackApp::OpenTemplateFile(const mpt::PathString &file) const
{
	return GetModDocTemplate()->OpenTemplateFile(file);
}


void CTrackApp::AddToRecentFileList(LPCTSTR lpszPathName)
{
	AddToRecentFileList(mpt::PathString::FromCString(lpszPathName));
}


void CTrackApp::AddToRecentFileList(const mpt::PathString &path)
{
	RemoveMruItem(path);
	TrackerSettings::Instance().mruFiles.insert(TrackerSettings::Instance().mruFiles.begin(), path);
	if(TrackerSettings::Instance().mruFiles.size() > TrackerSettings::Instance().mruListLength)
	{
		TrackerSettings::Instance().mruFiles.resize(TrackerSettings::Instance().mruListLength);
	}
	CMainFrame::GetMainFrame()->UpdateMRUList();
}


void CTrackApp::RemoveMruItem(const size_t item)
{
	if(item < TrackerSettings::Instance().mruFiles.size())
	{
		TrackerSettings::Instance().mruFiles.erase(TrackerSettings::Instance().mruFiles.begin() + item);
		CMainFrame::GetMainFrame()->UpdateMRUList();
	}
}


void CTrackApp::RemoveMruItem(const mpt::PathString &path)
{
	auto &mruFiles = TrackerSettings::Instance().mruFiles;
	for(auto i = mruFiles.begin(); i != mruFiles.end(); i++)
	{
		if(!mpt::PathCompareNoCase(*i, path))
		{
			mruFiles.erase(i);
			break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// CTrackApp initialization


namespace Tracker
{
mpt::recursive_mutex_with_lock_count & GetGlobalMutexRef()
{
	return theApp.GetGlobalMutexRef();
}
} // namespace Tracker


class ComponentManagerSettings
	: public IComponentManagerSettings
{
private:
	TrackerSettings &conf;
	mpt::PathString configPath;
public:
	ComponentManagerSettings(TrackerSettings &conf, const mpt::PathString &configPath)
		: conf(conf)
		, configPath(configPath)
	{
		return;
	}
	bool LoadOnStartup() const override
	{
		return conf.ComponentsLoadOnStartup;
	}
	bool KeepLoaded() const override
	{
		return conf.ComponentsKeepLoaded;
	}
	bool IsBlocked(const std::string &key) const override
	{
		return conf.IsComponentBlocked(key);
	}
};


SettingsContainer &CTrackApp::GetPluginState()
{
	ASSERT(m_pPluginState);
	return *m_pPluginState;
}


SettingsContainer &CTrackApp::GetPluginCache()
{
	ASSERT(m_pPluginCache);
	return *m_pPluginCache;
}


SettingsContainer &CTrackApp::GetSongSettings()
{
	ASSERT(m_pSongSettings);
	return *m_pSongSettings;
}


const mpt::PathString &CTrackApp::GetSongSettingsFilename() const
{
	return m_pSongSettingsIniFile->Filename();
}


// Move a config file called fileName from the App's directory (or one of its sub directories specified by subDir) to
// %APPDATA%. If specified, it will be renamed to newFileName. Existing files are never overwritten.
// Returns true on success.
bool CTrackApp::MoveConfigFile(const mpt::PathString &fileName, mpt::PathString subDir, mpt::PathString newFileName)
{
	const mpt::PathString oldPath = GetInstallPath() + subDir + fileName;
	mpt::PathString newPath = GetConfigPath() + subDir;
	if(!newFileName.empty())
		newPath += newFileName;
	else
		newPath += fileName;

	if(!mpt::native_fs{}.is_file(newPath) && mpt::native_fs{}.is_file(oldPath))
	{
		return MoveFile(oldPath.AsNative().c_str(), newPath.AsNative().c_str()) != 0;
	}
	return false;
}


// Set up paths were configuration data is written to. Set overridePortable to true if application's own directory should always be used.
void CTrackApp::SetupPaths(bool overridePortable)
{

	// First, determine if the executable is installed in multi-arch mode or in the old standard mode.
	bool modeMultiArch = false;
	bool modeSourceProject = false;
	const mpt::PathString exePath = mpt::common_directories::get_application_directory();
	auto exePathComponents = mpt::split(exePath.GetDirectory().WithoutTrailingSlash().ToUnicode(), P_("\\").ToUnicode());
	if(exePathComponents.size() >= 2)
	{
		if(exePathComponents[exePathComponents.size()-1] == mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()))
		{
			if(exePathComponents[exePathComponents.size()-2] == U_("bin"))
			{
				modeMultiArch = true;
			}
		}
	}
	// Check if we are running from the source tree.
	if(!modeMultiArch && exePathComponents.size() >= 4)
	{
		if(exePathComponents[exePathComponents.size()-1] == mpt::OS::Windows::Name(mpt::OS::Windows::GetProcessArchitecture()))
		{
			if(exePathComponents[exePathComponents.size()-4] == U_("bin"))
			{
				modeSourceProject = true;
			}
		}
	}
	if(modeSourceProject)
	{
		mpt::native_fs fs;
		m_InstallPath = fs.absolute(exePath + P_("..\\") + P_("..\\") + P_("..\\") + P_("..\\"));
		m_InstallBinPath = fs.absolute(exePath + P_("..\\"));
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = fs.absolute(exePath + P_("..\\") + P_("..\\") + P_("..\\") + P_("..\\packageTemplate\\"));
	} else if(modeMultiArch)
	{
		mpt::native_fs fs;
		m_InstallPath = fs.absolute(exePath + P_("..\\") + P_("..\\"));
		m_InstallBinPath = fs.absolute(exePath + P_("..\\"));
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = fs.absolute(exePath + P_("..\\") + P_("..\\"));
	} else
	{
		m_InstallPath = exePath;
		m_InstallBinPath = exePath;
		m_InstallBinArchPath = exePath;
		m_InstallPkgPath = exePath;
	}

	// Determine paths, portable mode, first run. Do not yet update any state.
	mpt::PathString configPathPortable = (modeSourceProject ? exePath : m_InstallPath); // config path in portable mode
	mpt::PathString configPathUser; // config path in default non-portable mode
	{
		// Try to find a nice directory where we should store our settings (default: %APPDATA%)
		TCHAR dir[MAX_PATH] = { 0 };
		if((SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, dir) == S_OK)
			|| (SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, dir) == S_OK))
		{
			// Store our app settings in %APPDATA% or "My Documents"
			configPathUser = mpt::PathString::FromNative(dir) + P_("\\OpenMPT\\");
		}
	}

	// Check if the user has configured portable mode.
	bool configInstallPortable = false;
	mpt::PathString portableFlagFilename = (configPathPortable + P_("OpenMPT.portable"));
	bool configPortableFlag = mpt::native_fs{}.is_file(portableFlagFilename);
	configInstallPortable = configInstallPortable || configPortableFlag;
	// before 1.29.00.13:
	configInstallPortable = configInstallPortable || (GetPrivateProfileInt(_T("Paths"), _T("UseAppDataDirectory"), 1, (configPathPortable + P_("mptrack.ini")).AsNative().c_str()) == 0);
	// convert to new style
	if(configInstallPortable && !configPortableFlag)
	{
		mpt::IO::SafeOutputFile f(portableFlagFilename);
	}

	// Determine portable mode.
	bool portableMode = overridePortable || configInstallPortable || configPathUser.empty();

	// Update config dir
	m_ConfigPath = portableMode ? configPathPortable : configPathUser;

	// Set up default file locations
	m_szConfigFileName = m_ConfigPath + P_("mptrack.ini"); // config file
	m_PluginStateFileName = m_ConfigPath + P_("PluginState.ini"); // state of plugin loader for crash recovery
	m_szPluginCacheFileName = m_ConfigPath + P_("plugin.cache"); // plugin cache

	// Force use of custom ini file rather than windowsDir\executableName.ini
	if(m_pszProfileName)
	{
		free((void *)m_pszProfileName);
	}
	m_pszProfileName = _tcsdup(m_szConfigFileName.ToCString());

	m_bInstallerMode = !modeSourceProject && !portableMode;
	m_bPortableMode = portableMode;
	m_bSourceTreeMode = modeSourceProject;

}


void CTrackApp::CreatePaths()
{
	// Create missing directories
	if(!IsPortableMode())
	{
		if(!mpt::native_fs{}.is_directory(m_ConfigPath))
		{
			CreateDirectory(m_ConfigPath.AsNative().c_str(), 0);
		}
	}

	// Handle updates from old versions.
	if(!IsPortableMode())
	{
		// Move the config files if they're still in the old place.
		MoveConfigFile(P_("mptrack.ini"));
		MoveConfigFile(P_("plugin.cache"));
	
		// Import old tunings
		const mpt::PathString oldTunings = GetInstallPath() + P_("tunings\\");

		if(mpt::native_fs{}.is_directory(oldTunings))
		{
			FolderScanner scanner{oldTunings, FolderScanner::kOnlyFiles};
			mpt::PathString fileName;
			while(scanner.Next(fileName))
			{
				MoveConfigFile(fileName.GetFilename(), P_("tunings\\"));
			}
			RemoveDirectory(oldTunings.AsNative().c_str());
		}
	}
}


mpt::PathString CTrackApp::GetUserTemplatesPath() const
{
	return GetConfigPath() + P_("TemplateModules\\");
}


mpt::PathString CTrackApp::GetExampleSongsPath() const
{
	return GetInstallPath() + P_("ExampleSongs\\");
}


UINT CTrackApp::GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault)
{
	if(!lpszSection)
	{
		return nDefault;
	}
	if(!lpszEntry)
	{
		return nDefault;
	}
	return GetSettings().Read<int32>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))}, nDefault);
}

BOOL CTrackApp::WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue)
{
	if(!lpszSection)
	{
		return FALSE;
	}
	if(!lpszEntry)
	{
		return FALSE;
	}
	GetSettings().Write<int32>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))}, nValue);
	return TRUE;
}

CString CTrackApp::GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault)
{
	if(!lpszSection)
	{
		return lpszDefault ? CString(lpszDefault) : CString();
	}
	if(!lpszEntry)
	{
		return lpszDefault ? CString(lpszDefault) : CString();
	}
	return mpt::ToCString(GetSettings().Read<mpt::ustring>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))}, mpt::ToUnicode(lpszDefault ? CString(lpszDefault) : CString())));
}

BOOL CTrackApp::WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue)
{
	if(!lpszSection)
	{
		return FALSE;
	}
	if(!lpszEntry)
	{
		GetSettings().Remove(mpt::ToUnicode(CString(lpszSection)));
		return TRUE;
	}
	GetSettings().Write<mpt::ustring>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))}, lpszValue ? mpt::ToUnicode(CString(lpszValue)) : mpt::ustring{});
	return TRUE;
}

BOOL CTrackApp::GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes)
{
	if(!lpszSection)
	{
		return FALSE;
	}
	if(!lpszEntry)
	{
		return FALSE;
	}
	if(!ppData)
	{
		return FALSE;
	}
	if(!pBytes)
	{
		return FALSE;
	}
	std::vector<std::byte> data = GetSettings().Read<std::vector<std::byte>>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))});
	*pBytes = mpt::saturate_cast<UINT>(data.size());
	*ppData = new BYTE[data.size()];
	std::memcpy(*ppData, data.data(), data.size());
	return TRUE;
}

BOOL CTrackApp::WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes)
{
	if(!lpszSection)
	{
		return FALSE;
	}
	if(!lpszEntry)
	{
		return FALSE;
	}
	if(!pData && nBytes > 0)
	{
		return FALSE;
	}
	std::vector<std::byte> data = mpt::make_vector(mpt::byte_cast<std::byte*>(pData), nBytes);
	GetSettings().Write<std::vector<std::byte>>(SettingPath{mpt::ToUnicode(CString(lpszSection)), mpt::ToUnicode(CString(lpszEntry))}, data);
	return TRUE;
}



#if defined(MPT_ENABLE_SYSTEM_SUPPORT_CHECK)

static bool ProcessorCanRunCurrentBuild()
{
#ifdef MPT_ENABLE_ARCH_INTRINSICS
	if(!mpt::arch::get_cpu_info().has_features(mpt::arch::current::assumed_features()))
	{
		return false;
	}
#endif // MPT_ENABLE_ARCH_INTRINSICS
	return true;
}

static bool SystemCanRunCurrentBuild()
{
	if(mpt::OS::Windows::IsOriginal())
	{
		if(mpt::osinfo::windows::Version::Current().IsBefore(mpt::OS::Windows::Version::GetMinimumKernelLevel()))
		{
			return false;
		}
		if(mpt::osinfo::windows::Version::Current().IsBefore(mpt::OS::Windows::Version::GetMinimumAPILevel()))
		{
			return false;
		}
	} else if(mpt::OS::Windows::IsWine() && theApp.GetWineVersion()->IsValid())
	{
		if(theApp.GetWineVersion()->IsBefore(mpt::OS::Wine::GetMinimumWineVersion()))
		{
			return false;
		}
	}
	return true;
}

bool CTrackApp::CheckSystemSupport()
{
	const mpt::ustring lf = U_("\n");
	const mpt::ustring url = Build::GetURL(Build::Url::Download);
	if(!ProcessorCanRunCurrentBuild())
	{
		mpt::ustring text;
		text += U_("Your CPU is too old to run this variant of OpenMPT.") + lf;
		text += U_("OpenMPT will exit now.") + lf;
		Reporting::Error(text, "OpenMPT");
		return false;
	}
	if(!SystemCanRunCurrentBuild())
	{
		if(mpt::OS::Windows::IsOriginal())
		{
			mpt::ustring text;
			text += U_("Your system does not meet the minimum requirements for this variant of OpenMPT.") + lf;
			text += U_("OpenMPT will exit now.") + lf;
			Reporting::Error(text, "OpenMPT");
			return false;
		} else
		{
			mpt::ustring text;
			text += U_("Your system does not meet the minimum requirements for this variant of OpenMPT.") + lf;
			Reporting::Error(text, "OpenMPT");
			// may work though
		}
	}
	return true;
}

#endif // MPT_ENABLE_SYSTEM_SUPPORT_CHECK


BOOL CTrackApp::InitInstanceEarly(CMPTCommandLineInfo &cmdInfo)
{
	// The first step of InitInstance, always executed without any crash handler.

	#ifndef UNICODE
		if(MessageBox(NULL,
			_T("STOP!!!") _T("\n")
			_T("This is an ANSI (as opposed to a UNICODE) build of OpenMPT.") _T("\n")
			_T("\n")
			_T("ANSI builds are NOT SUPPORTED and WILL CAUSE CORRUPTION of the OpenMPT configuration and exhibit other unintended behaviour.") _T("\n")
			_T("\n")
			_T("Please use an official build of OpenMPT or compile 'OpenMPT.sln' instead of 'OpenMPT-ANSI.sln'.") _T("\n")
			_T("\n")
			_T("Continue starting OpenMPT anyway?") _T("\n"),
			_T("OpenMPT"), MB_ICONSTOP | MB_YESNO| MB_DEFBUTTON2)
			!= IDYES)
		{
			ExitProcess(1);
		}
	#endif

	// Call the base class.
	// This is required for MFC RestartManager integration.
	if(!CWinApp::InitInstance())
	{
		return FALSE;
	}

	#if MPT_COMPILER_MSVC
		_CrtSetDebugFillThreshold(0); // Disable buffer filling in secure enhanced CRT functions.
	#endif

	// Avoid e.g. audio APIs trying to load wdmaud.drv from arbitrary working directory
	::SetCurrentDirectory(mpt::common_directories::get_application_directory().AsNative().c_str());

	// Initialize OLE MFC support
	[[maybe_unused]] BOOL oleinit = AfxOleInit();
	ASSERT(oleinit != FALSE); // no MPT_ASSERT here!

	// Parse command line for standard shell commands, DDE, file open
	ParseCommandLine(cmdInfo);

	// Set up paths to store configuration in
	SetupPaths(cmdInfo.m_portable);

	if(cmdInfo.m_sharedInstance && IPCWindow::SendToIPC(cmdInfo.m_fileNames, cmdInfo.m_autoPlay))
	{
		ExitProcess(0);
	}

	// Initialize DocManager (for DDE)
	// requires mpt::PathString
	ASSERT(nullptr == m_pDocManager); // no MPT_ASSERT here!
	m_pDocManager = new CModDocManager();

	if(IsDebuggerPresent() && cmdInfo.m_debugCrashHandler)
	{
		ExceptionHandler::useAnyCrashHandler = true;
		ExceptionHandler::useImplicitFallbackSEH = false;
		ExceptionHandler::useExplicitSEH = true;
		ExceptionHandler::handleStdTerminate = true;
		ExceptionHandler::handleMfcExceptions = true;
		ExceptionHandler::debugExceptionHandler = true;
	} else if(IsDebuggerPresent() || cmdInfo.m_noCrashHandler)
	{
		ExceptionHandler::useAnyCrashHandler = false;
		ExceptionHandler::useImplicitFallbackSEH = false;
		ExceptionHandler::useExplicitSEH = false;
		ExceptionHandler::handleStdTerminate = false;
		ExceptionHandler::handleMfcExceptions = false;
		ExceptionHandler::debugExceptionHandler = false;
	} else
	{
		ExceptionHandler::useAnyCrashHandler = true;
		ExceptionHandler::useImplicitFallbackSEH = true;
		ExceptionHandler::useExplicitSEH = true;
		ExceptionHandler::handleStdTerminate = true;
		ExceptionHandler::handleMfcExceptions = true;
		ExceptionHandler::debugExceptionHandler = false;
	}

	return TRUE;
}


BOOL CTrackApp::InitInstanceImpl(CMPTCommandLineInfo &cmdInfo)
{

	m_GuiThreadId = GetCurrentThreadId();

	mpt::log::Trace::SetThreadId(mpt::log::Trace::ThreadKindGUI, m_GuiThreadId);

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::Register();
	}

	// Start loading
	BeginWaitCursor();

	MPT_LOG_GLOBAL(LogInformation, "", U_("OpenMPT Start"));

	// create the tracker-global random device
	m_RD = std::make_unique<mpt::random_device>();
	// make the device available to non-tracker-only code
	mpt::set_global_random_device(m_RD.get());
	// create and seed the traker-global best PRNG with the random device
	m_PRNG = std::make_unique<mpt::thread_safe_prng<mpt::default_prng> >(mpt::make_prng<mpt::default_prng>(RandomDevice()));
	// make the best PRNG available to non-tracker-only code
	mpt::set_global_prng(m_PRNG.get());
	// additionally, seed the C rand() PRNG, just in case any third party library calls rand()
	mpt::crand::reseed(RandomDevice());

	m_Gdiplus = std::make_unique<GdiplusRAII>();

	if(cmdInfo.m_noWine)
	{
		mpt::OS::Windows::PreventWineDetection();
	}

	#ifdef MPT_ENABLE_ARCH_INTRINSICS
		if(!cmdInfo.m_noAssembly)
		{
			CPU::EnableAvailableFeatures();
		}
	#endif // MPT_ENABLE_ARCH_INTRINSICS

	if(mpt::OS::Windows::IsWine())
	{
		m_WineVersion = std::make_shared<mpt::OS::Wine::VersionContext>(mpt::OS::Wine::VersionContext::Current().value());
	}

	// Create paths to store configuration in
	CreatePaths();

	m_pSettingsIniFile = std::make_unique<IniFileSettingsBackend>(m_szConfigFileName);
	m_pSettings = std::make_unique<SettingsContainer>(m_pSettingsIniFile.get());

	m_pDebugSettings = std::make_unique<DebugSettings>(*m_pSettings);

	m_pTrackerSettings = std::make_unique<TrackerSettings>(*m_pSettings);
	if(cmdInfo.m_synmptChecks)
	{
		const int result = SynMPTChecks::Run(GetInstallBinArchPath(), cmdInfo.m_fileNames);
		::ExitProcess(static_cast<UINT>(result));
	}

	MPT_LOG_GLOBAL(LogInformation, "", U_("OpenMPT settings initialized."));

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::ConfigureSystemHandler();
	}

	if(TrackerSettings::Instance().MiscUseSingleInstance && IPCWindow::SendToIPC(cmdInfo.m_fileNames, cmdInfo.m_autoPlay))
	{
		ExitProcess(0);
	}

	IPCWindow::Open(m_hInstance);

	m_pSongSettingsIniFile = std::make_unique<IniFileSettingsBackend>(GetConfigPath() + P_("SongSettings.ini"));
	m_pSongSettings = std::make_unique<SettingsContainer>(m_pSongSettingsIniFile.get());

	m_pPluginState = std::make_unique<IniFileSettingsContainer>(m_PluginStateFileName);
	m_pPluginCache = std::make_unique<IniFileSettingsContainer>(m_szPluginCacheFileName);

	m_pComponentManagerSettings = std::make_unique<ComponentManagerSettings>(TrackerSettings::Instance(), GetConfigPath());

	// Load standard INI file options (without MRU)
	// requires SetupPaths+CreatePaths called
	LoadStdProfileSettings(0);

// Set process priority class
#ifndef _DEBUG
	SetPriorityClass(GetCurrentProcess(), TrackerSettings::Instance().MiscProcessPriorityClass);
#endif

	// Dynamic DPI-awareness. Some users might want to disable DPI-awareness because of their DPI-unaware VST plugins.
	switch(TrackerSettings::Instance().dpiAwareness)
	{
	case DPIAwarenessMode::NoDPIAwareness:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::LowDpi);
		break;
	case DPIAwarenessMode::NoDPIAwarenessGDIUpscaled:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::LowDpiUpscaled);
		break;
	case DPIAwarenessMode::SystemDPIAware:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiSystem);
		break;
	case DPIAwarenessMode::PerMonitorDPIAware:
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiPerMonitor);
		break;
	default:
		// Malformed entry in INI file
		TrackerSettings::Instance().dpiAwareness = DPIAwarenessMode::PerMonitorDPIAware;
		HighDPISupport::SetDPIAwareness(HighDPISupport::Mode::HighDpiPerMonitor);
		break;
	}

	// create main MDI Frame window
	CMainFrame *pMainFrame = new CMainFrame();
	if(!pMainFrame->LoadFrame(IDR_MAINFRAME))
	{
		return FALSE;
	}
	m_pMainWnd = pMainFrame;

	// Show splash screen
	if(cmdInfo.m_bShowSplash && TrackerSettings::Instance().m_ShowSplashScreen)
	{
		StartSplashScreen();
	}

	// create component manager
	ComponentManager::Init(*m_pComponentManagerSettings);

	// load components
	ComponentManager::Instance()->Startup();

	// Wine Support
	if(mpt::OS::Windows::IsWine())
	{
		WineIntegration::Initialize();
		WineIntegration::Load();
	}

	// Register document templates
	m_pModTemplate = new CModDocTemplate(
		IDR_MODULETYPE,
		RUNTIME_CLASS(CModDoc),
		RUNTIME_CLASS(CChildFrame), // custom MDI child frame
		RUNTIME_CLASS(CModControlView));
	AddDocTemplate(m_pModTemplate);

	// Enable DDE Execute open
	// requires m_pDocManager
	EnableShellOpen();

	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();

	// Load sound APIs
	// requires TrackerSettings
	m_pAllSoundDeviceComponents = std::make_unique<AllSoundDeviceComponents>();
	auto GetSysInfo = [&]()
	{
		if(mpt::OS::Windows::IsWine())
		{
			return SoundDevice::SysInfo(mpt::osinfo::get_class(), mpt::osinfo::windows::Version::Current(), GetWineVersion()->Version(), GetWineVersion()->HostClass());
		}
		return SoundDevice::SysInfo(mpt::osinfo::get_class(), mpt::osinfo::windows::Version::Current());
	};
	SoundDevice::SysInfo sysInfo = GetSysInfo();
	SoundDevice::AppInfo appInfo;
	appInfo.SetName(U_("OpenMPT"));
	appInfo.SetHWND(*m_pMainWnd);
	appInfo.BoostedThreadPriorityXP = TrackerSettings::Instance().SoundBoostedThreadPriority;
	appInfo.BoostedThreadMMCSSClassVista = TrackerSettings::Instance().SoundBoostedThreadMMCSSClass;
	appInfo.BoostedThreadRealtimePosix = TrackerSettings::Instance().SoundBoostedThreadRealtimePosix;
	appInfo.BoostedThreadNicenessPosix = TrackerSettings::Instance().SoundBoostedThreadNicenessPosix;
	appInfo.BoostedThreadRtprioPosix = TrackerSettings::Instance().SoundBoostedThreadRtprioPosix;
	appInfo.MaskDriverCrashes = TrackerSettings::Instance().SoundMaskDriverCrashes;
	appInfo.AllowDeferredProcessing = TrackerSettings::Instance().SoundAllowDeferredProcessing;
	std::vector<std::shared_ptr<SoundDevice::IDevicesEnumerator>> deviceEnumerators = SoundDevice::Manager::GetEnabledEnumerators(*m_pAllSoundDeviceComponents);
	deviceEnumerators.push_back(std::static_pointer_cast<SoundDevice::IDevicesEnumerator>(std::make_shared<SoundDevice::DevicesEnumerator<SoundDevice::SoundDeviceStub>>()));
	m_pSoundDevicesManager = std::make_unique<SoundDevice::Manager>(m_GlobalLogger, sysInfo, appInfo, std::move(deviceEnumerators));
	m_pTrackerSettings->MigrateOldSoundDeviceSettings(*m_pSoundDevicesManager);

	// Windows 11 lazily initializes its MIDI service on first use, which can
	// take several seconds as it scans for Bluetooth and Network MIDI devices.
	// Once the service is running, the delay / hang when trying to use any
	// MIDI APIs effectively disappears.
	// To work around that, force some MIDI API usage on a separate thread to
	// force-initialize MidiSrv. Adding an instance of the MIDI I/O plugin,
	// reviewing MIDI settings or turning on MIDI recording should no longer
	// hang on first use then.
	// Note that this can still block the OpenMPT UI if the main thread
	// enumerates MME audio devices at the same time, e.g. because the user
	// opened the settings dialog, as there appears to be shared internal lock
	// between these two APIs.
	// See https://github.com/microsoft/MIDI/issues/1076
	std::thread{midiInGetNumDevs}.detach();

	// Set default note names
	CSoundFile::SetDefaultNoteNames();

	// Load Soundfonts and default MIDI Library
	if(!cmdInfo.m_noDls)
	{
		m_scannedDlsBanks = LoadDefaultDLSBanks();
	}
	// Load user-defined MIDI Library
	ImportMidiConfig(theApp.GetSettings(), {}, true);

	// Initialize Plugins
	if(!cmdInfo.m_noPlugins)
	{
		InitializeDXPlugins();
	}

	// Initialize CMainFrame
	pMainFrame->Initialize();

	// Dispatch commands specified on the command line
	if(cmdInfo.m_nShellCommand == CCommandLineInfo::FileNew)
	{
		// When not asked to open any existing file,
		// we do not want to open an empty new one on startup.
		cmdInfo.m_nShellCommand = CCommandLineInfo::FileNothing;
	}
	bool shellSuccess = false;
	if(cmdInfo.m_fileNames.empty())
	{
		shellSuccess = ProcessShellCommand(cmdInfo) != FALSE;
	} else
	{
		cmdInfo.m_nShellCommand = CCommandLineInfo::FileOpen;
		for(const auto &filename : cmdInfo.m_fileNames)
		{
			cmdInfo.m_strFileName = filename.ToCString();
			shellSuccess |= ProcessShellCommand(cmdInfo) != FALSE;
		}
	}
	if(!shellSuccess)
	{
		EndWaitCursor();
		StopSplashScreen();
		m_pMainWnd->DestroyWindow();
		m_pMainWnd = nullptr;
		return FALSE;
	}

	pMainFrame->UpdateDocumentCount();
	pMainFrame->ShowWindow(m_nCmdShow);
	pMainFrame->UpdateWindow();

	EndWaitCursor();

	// Perform startup tasks.

#if defined(MPT_ENABLE_SYSTEM_SUPPORT_CHECK)
	// Check whether we are running the best build for the given system.
	if(!cmdInfo.m_noSysCheck)
	{
		if(!CheckSystemSupport())
		{
			StopSplashScreen();
			return FALSE;
		}
	}
#endif // MPT_ENABLE_SYSTEM_SUPPORT_CHECK

	if(TrackerSettings::Instance().FirstRun)
	{
		new WelcomeDlg(m_pMainWnd);
	} else
	{
#if defined(MPT_ENABLE_SYSTEM_SUPPORT_CHECK)
		bool deprecatedSoundDevice = GetSoundDevicesManager()->FindDeviceInfo(TrackerSettings::Instance().GetSoundDeviceIdentifier()).IsDeprecated();
		bool showSettings = deprecatedSoundDevice && !TrackerSettings::Instance().m_SoundDeprecatedDeviceWarningShown && (Reporting::Confirm(
			U_("You have currently selected a sound device which is deprecated. MME/WaveOut support will be removed in a future OpenMPT version.\n") +
			U_("The recommended sound device type is WASAPI.\n") +
			U_("Do you want to change your sound device settings now?"),
			U_("OpenMPT - Deprecated sound device")
			) == cnfYes);
		if(showSettings)
		{
			TrackerSettings::Instance().m_SoundDeprecatedDeviceWarningShown = true;
			m_pMainWnd->PostMessage(WM_COMMAND, ID_VIEW_OPTIONS);
		}
#endif // MPT_ENABLE_SYSTEM_SUPPORT_CHECK
	}

#ifdef ENABLE_TESTS
	if(!cmdInfo.m_noTests)
	{
		Test::DoTests();
	}
#endif

	theApp.GetSettings().Flush();

	if(TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup)
	{
		pMainFrame->InitPreview();
		pMainFrame->PreparePreview(NOTE_NOTECUT, 0);
		pMainFrame->PlayPreview();
	}

	if(cmdInfo.m_autoPlay)
	{
		pMainFrame->PlayMod(pMainFrame->GetActiveDoc());
	}

	if(!TrackerSettings::Instance().FirstRun)
	{
#if defined(MPT_ENABLE_UPDATE)
		if(CUpdateCheck::IsSuitableUpdateMoment())
		{
			CUpdateCheck::DoAutoUpdateCheck();
		}
#endif // MPT_ENABLE_UPDATE
	}

	return TRUE;
}


BOOL CTrackApp::InitInstance()
{
	CMPTCommandLineInfo cmdInfo;
	if(!InitInstanceEarly(cmdInfo))
	{
		return FALSE;
	}
	return InitInstanceLate(cmdInfo);
}


BOOL CTrackApp::InitInstanceLate(CMPTCommandLineInfo &cmdInfo)
{
	BOOL result = FALSE;
	if(ExceptionHandler::useExplicitSEH)
	{
		// https://support.microsoft.com/en-us/kb/173652
		__try
		{
			result = InitInstanceImpl(cmdInfo);
		} __except(ExceptionHandler::ExceptionFilter(GetExceptionInformation()))
		{
			std::abort();
		}
	} else
	{
		result = InitInstanceImpl(cmdInfo);
	}
	return result;
}


int CTrackApp::Run()
{
	int result = 255;
	if(ExceptionHandler::useExplicitSEH)
	{
		// https://support.microsoft.com/en-us/kb/173652
		__try
		{
			result = CWinApp::Run();
		} __except(ExceptionHandler::ExceptionFilter(GetExceptionInformation()))
		{
			std::abort();
		}
	} else
	{
		result = CWinApp::Run();
	}
	return result;
}


LRESULT CTrackApp::ProcessWndProcException(CException * e, const MSG * pMsg)
{
	if(ExceptionHandler::handleMfcExceptions)
	{
		LRESULT result = 0L; // as per documentation
		if(pMsg)
		{
			if(pMsg->message == WM_COMMAND)
			{
				result = (LRESULT)TRUE; // as per documentation
			}
		}
		if(dynamic_cast<CMemoryException*>(e))
		{
			e->ReportError();
			//ExceptionHandler::UnhandledMFCException(e, pMsg);
		} else
		{
			ExceptionHandler::UnhandledMFCException(e, pMsg);
		}
		return result;
	} else
	{
		return CWinApp::ProcessWndProcException(e, pMsg);
	}
}


int CTrackApp::ExitInstance()
{
	int result = 0;
	if(ExceptionHandler::useExplicitSEH)
	{
		// https://support.microsoft.com/en-us/kb/173652
		__try
		{
			result = ExitInstanceImpl();
		} __except(ExceptionHandler::ExceptionFilter(GetExceptionInformation()))
		{
			std::abort();
		}
	} else
	{
		result = ExitInstanceImpl();
	}
	return result;
}


int CTrackApp::ExitInstanceImpl()
{
	IPCWindow::Close();

	// Uninitialize Plugins
	UninitializeDXPlugins();

	ExportMidiConfig(theApp.GetSettings());
	AddScannedDLSBanks();
	SaveDefaultDLSBanks();
	gpDLSBanks.clear();

	m_pSoundDevicesManager.reset();
	m_pAllSoundDeviceComponents.reset();

	// implicitly unload Wine Integration in next step

	ComponentManager::Release();

	m_pComponentManagerSettings.reset();

	m_pPluginCache.reset();
	m_pPluginState.reset();

	m_pSongSettings.reset();
	m_pSongSettingsIniFile.reset();

	m_pTrackerSettings.reset();
	m_pDebugSettings.reset();
	m_pSettings.reset();
	m_pSettingsIniFile.reset();

	if(mpt::OS::Windows::IsWine())
	{
		m_WineVersion.reset();
	}

	m_Gdiplus.reset();

	mpt::set_global_prng(nullptr);
	m_PRNG.reset();
	mpt::set_global_random_device(nullptr);
	m_RD.reset();

	if(ExceptionHandler::useAnyCrashHandler)
	{
		ExceptionHandler::UnconfigureSystemHandler();
		ExceptionHandler::Unregister();
	}

#ifdef USE_PROFILER
	Profiler::Update();
	Reporting::Information(Profiler::DumpProfiles());
#endif

	return CWinApp::ExitInstance();
}


////////////////////////////////////////////////////////////////////////////////
// App Messages


CModDoc *CTrackApp::NewDocument(MODTYPE newType)
{
	// Build from template
	if(newType == MOD_TYPE_NONE)
	{
		const mpt::PathString templateFile = TrackerSettings::Instance().defaultTemplateFile;
		if(TrackerSettings::Instance().defaultNewFileAction == nfDefaultTemplate && !templateFile.empty())
		{
			// Template file can be either a filename inside one of the preset and user TemplateModules folders, or a full path.
			const mpt::PathString dirs[] = { GetUserTemplatesPath(), GetInstallPath() + P_("TemplateModules\\"), mpt::PathString() };
			for(const auto &dir : dirs)
			{
				if(mpt::native_fs{}.is_file(dir + templateFile))
				{
					if(CModDoc *modDoc = static_cast<CModDoc *>(m_pModTemplate->OpenTemplateFile(dir + templateFile)))
					{
						return modDoc;
					}
				}
			}
		}


		// Default module type
		newType = TrackerSettings::Instance().defaultModType;

		// Get active document to make the new module of the same type
		CModDoc *pModDoc = CMainFrame::GetMainFrame()->GetActiveDoc();
		if(pModDoc != nullptr && TrackerSettings::Instance().defaultNewFileAction == nfSameAsCurrent)
		{
			newType = pModDoc->GetSoundFile().GetBestSaveFormat();
		}
	}

	SetDefaultDocType(newType);
	return static_cast<CModDoc *>(m_pModTemplate->OpenDocumentFile(_T("")));
}


void CTrackApp::OpenModulesDialog(std::vector<mpt::PathString> &files, const mpt::PathString &overridePath)
{
	files.clear();

	static constexpr std::string_view commonExts[] = {"mod", "s3m", "xm", "it", "mptm", "mo3", "oxm", "nst", "stk", "m15", "pt36", "mid", "rmi", "smf", "wav", "mdz", "s3z", "xmz", "itz", "mdr"};
	std::string exts, extsWithoutCommon;
	for(const auto &ext : CSoundFile::GetSupportedExtensions(true))
	{
		const auto filter = std::string("*.") + ext + std::string(";");
		exts += filter;
		if(!mpt::contains(commonExts, ext))
			extsWithoutCommon += filter;
	}

	static int nFilterIndex = 0;
	FileDialog dlg = OpenFileDialog()
		.AllowMultiSelect()
		.ExtensionFilter("All Modules (*.mptm,*.mod,*.xm,*.s3m,*.it,...)|" + exts + ";mod.*"
		"|"
		"Compressed Modules (*.mdz,*.s3z,*.xmz,*.itz,*.mo3,*.oxm,...)|*.mdz;*.s3z;*.xmz;*.itz;*.mdr;*.zip;*.rar;*.lha;*.pma;*.lzs;*.gz;*.mo3;*.oxm"
		"|"
		"ProTracker Modules (*.mod,*.nst)|*.mod;mod.*;*.mdz;*.nst;*.m15;*.stk;*.pt36|"
		"Scream Tracker Modules (*.s3m,*.stm)|*.s3m;*.stm;*.s3z;*.stx|"
		"FastTracker Modules (*.xm)|*.xm;*.xmz|"
		"Impulse Tracker Modules (*.it)|*.it;*.itz|"
		"OpenMPT Modules (*.mptm)|*.mptm;*.mptmz|"
		"Other Modules (*.mtm,*.okt,*.mdl,*.669,*.far,...)|" + extsWithoutCommon + "|"
		"Wave Files (*.wav)|*.wav|"
		"MIDI Files (*.mid,*.rmi)|*.mid;*.rmi;*.smf|"
		"All Files (*.*)|*.*||")
		.WorkingDirectory(overridePath.empty() ? TrackerSettings::Instance().PathSongs.GetWorkingDir() : overridePath)
		.FilterIndex(&nFilterIndex);
	if(!dlg.Show()) return;

	if(overridePath.empty())
		TrackerSettings::Instance().PathSongs.SetWorkingDir(dlg.GetWorkingDirectory());

	files = dlg.GetFilenames();
}


void CTrackApp::OnFileOpen()
{
	FileDialog::PathList files;
	OpenModulesDialog(files);
	for(const auto &file : files)
	{
		OpenDocumentFile(file.ToCString());
	}
}


// App command to run the dialog
void CTrackApp::OnAppAbout()
{
	if (CAboutDlg::instance) return;
	CAboutDlg::instance = new CAboutDlg();
	CAboutDlg::instance->Create(IDD_ABOUTBOX, m_pMainWnd);
}


/////////////////////////////////////////////////////////////////////////////
// Splash Screen

class CSplashScreen : public DialogBase
{
protected:
	std::unique_ptr<Gdiplus::Image> m_Image;

public:
	~CSplashScreen();
	BOOL OnInitDialog() override;
	void OnOK() override;
	void OnCancel() override { OnOK(); }
	void OnPaint();
	BOOL OnEraseBkgnd(CDC *) { return TRUE; }

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CSplashScreen, DialogBase)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

static CSplashScreen *gpSplashScreen = nullptr;

static DWORD64 gSplashScreenStartTime = 0;


CSplashScreen::~CSplashScreen()
{
	gpSplashScreen = nullptr;
}


void CSplashScreen::OnPaint()
{
	CPaintDC dc(this);
	Gdiplus::Graphics gfx(dc);

	CRect rect;
	GetClientRect(&rect);
	gfx.SetInterpolationMode(Gdiplus::InterpolationModeHighQuality);
	gfx.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
	gfx.DrawImage(m_Image.get(), 0, 0, rect.right, rect.bottom);

	DialogBase::OnPaint();
}


BOOL CSplashScreen::OnInitDialog()
{
	DialogBase::OnInitDialog();

	try
	{
		m_Image = GDIP::LoadPixelImage(GetResource(MAKEINTRESOURCE(IDB_SPLASHNOFOLDFIN), _T("PNG")));
	} catch(const bad_image &)
	{
		return FALSE;
	}

	CRect rect;
	GetWindowRect(&rect);
	const int width = HighDPISupport::ScalePixels(m_Image->GetWidth(), m_hWnd) / 2;
	const int height = HighDPISupport::ScalePixels(m_Image->GetHeight(), m_hWnd) / 2;
	SetWindowPos(nullptr,
		rect.left - ((width - rect.Width()) / 2),
		rect.top - ((height - rect.Height()) / 2),
		width,
		height,
		SWP_NOZORDER | SWP_NOCOPYBITS);

	return TRUE;
}


void CSplashScreen::OnOK()
{
	StopSplashScreen();
}


static void StartSplashScreen()
{
	if(!gpSplashScreen)
	{
		gpSplashScreen = new CSplashScreen();
		gpSplashScreen->Create(IDD_SPLASHSCREEN, theApp.m_pMainWnd);
		gpSplashScreen->ShowWindow(SW_SHOW);
		gpSplashScreen->UpdateWindow();
		gpSplashScreen->BeginWaitCursor();
		gSplashScreenStartTime = Util::GetTickCount64();
	}
}


static void StopSplashScreen()
{
	if(gpSplashScreen)
	{
		gpSplashScreen->EndWaitCursor();
		gpSplashScreen->DestroyWindow();
		delete gpSplashScreen;
		gpSplashScreen = nullptr;
	}
}


static void TimeoutSplashScreen()
{
	if(gpSplashScreen)
	{
		if(Util::GetTickCount64() - gSplashScreenStartTime > 100)
		{
			StopSplashScreen();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////
// Idle-time processing

BOOL CTrackApp::PreTranslateMessage(MSG *message)
{
    if(message && SynMPTTheme::HandleTransportKey(*message)) return TRUE;
    return CWinApp::PreTranslateMessage(message);
}

BOOL CTrackApp::OnIdle(LONG lCount)
{
	BOOL b = CWinApp::OnIdle(lCount);
    SynMPTTheme::RefreshWindows();

	TimeoutSplashScreen();

	if(CMainFrame::GetMainFrame())
	{
		CMainFrame::GetMainFrame()->IdleHandlerSounddevice();

		if(m_scannedDlsBanksAvailable)
		{
			if(AddScannedDLSBanks())
				CMainFrame::GetMainFrame()->RefreshDlsBanks();
		}
	}

	// Call plugins idle routine for open editor
	if (m_pPluginManager)
	{
		DWORD curTime = timeGetTime();
		//rewbs.vstCompliance: call @ 50Hz
		if (curTime - m_dwLastPluginIdleCall > 20 || curTime < m_dwLastPluginIdleCall)
		{
			m_pPluginManager->OnIdle();
			m_dwLastPluginIdleCall = curTime;
		}
	}

	return b;
}


/////////////////////////////////////////////////////////////////////////////
// DIB


RGBQUAD rgb2quad(COLORREF c)
{
	RGBQUAD r;
	r.rgbBlue = GetBValue(c);
	r.rgbGreen = GetGValue(c);
	r.rgbRed = GetRValue(c);
	r.rgbReserved = 0;
	return r;
}


void DibBlt(HDC hdc, int x, int y, int sizex, int sizey, int srcx, int srcy, MODPLUGDIB *lpdib)
{
	if (!lpdib) return;
	SetDIBitsToDevice(	hdc,
						x,
						y,
						sizex,
						sizey,
						srcx,
						lpdib->bmiHeader.biHeight - srcy - sizey,
						0,
						lpdib->bmiHeader.biHeight,
						lpdib->lpDibBits,
						(LPBITMAPINFO)lpdib,
						DIB_RGB_COLORS);
}


MODPLUGDIB *LoadDib(LPCTSTR lpszName)
{
	mpt::const_byte_span data = GetResource(lpszName, RT_BITMAP);
	if(!data.data())
	{
		return nullptr;
	}
	LPBITMAPINFO p = (LPBITMAPINFO)data.data();
	MODPLUGDIB *pmd = new MODPLUGDIB;
	pmd->bmiHeader = p->bmiHeader;
	for (int i=0; i<16; i++) pmd->bmiColors[i] = p->bmiColors[i];
	LPBYTE lpDibBits = (LPBYTE)p;
	lpDibBits += p->bmiHeader.biSize + 16 * sizeof(RGBQUAD);
	pmd->lpDibBits = lpDibBits;
	return pmd;
}

int DrawTextT(HDC hdc, const wchar_t *lpchText, int cchText, LPRECT lprc, UINT format)
{
	return ::DrawTextW(hdc, lpchText, cchText, lprc, format);
}

int DrawTextT(HDC hdc, const char *lpchText, int cchText, LPRECT lprc, UINT format)
{
	return ::DrawTextA(hdc, lpchText, cchText, lprc, format);
}

template<typename Tchar>
static void DrawButtonRectImpl(HDC hdc, int lineWidth, HFONT font, CRect rect, const Tchar *text, bool disabled, bool pushed, DWORD textFlags, uint32 topMargin)
{
    const auto colorHighlight = SynMPTTheme::Color(COLOR_BTNHIGHLIGHT), colorShadow = SynMPTTheme::Color(COLOR_BTNSHADOW);
    auto oldpen = SelectPen(hdc, GetStockObject(DC_PEN));
    ::SetDCPenColor(hdc, pushed ? colorShadow : colorHighlight);
    ::SetDCBrushColor(hdc, SynMPTTheme::Color(pushed ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
    ::FillRect(hdc, rect, GetStockBrush(DC_BRUSH));
    ::MoveToEx(hdc, rect.left, rect.bottom - 1, nullptr); ::LineTo(hdc, rect.left, rect.top); ::LineTo(hdc, rect.right - 1, rect.top);
    ::SetDCPenColor(hdc, colorShadow); ::LineTo(hdc, rect.right - 1, rect.bottom - 1); ::LineTo(hdc, rect.left, rect.bottom - 1);
    SelectPen(hdc, oldpen);

	if(text && text[0])
	{
		rect.DeflateRect(lineWidth, lineWidth);
		if(pushed)
		{
			rect.top += lineWidth;
			rect.left += lineWidth;
		}
		::SetTextColor(hdc, SynMPTTheme::Color(disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
		::SetBkMode(hdc, TRANSPARENT);
		rect.top += topMargin;
		auto oldFont = SelectFont(hdc, font);
		DrawTextT(hdc, text, -1, &rect, textFlags | DT_SINGLELINE | DT_NOPREFIX);
		SelectFont(hdc, oldFont);
	}
}


void DrawButtonRect(HDC hdc, int lineWidth, const CRect &rect, const char *text, bool disabled, bool pushed, DWORD dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, CMainFrame::GetGUIFont(), rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(HDC hdc, int lineWidth, const CRect &rect, const wchar_t *text, bool disabled, bool pushed, DWORD dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, CMainFrame::GetGUIFont(), rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(HDC hdc, int lineWidth, HFONT font, const CRect &rect, const char *text, bool disabled, bool pushed, DWORD dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, font, rect, text, disabled, pushed, dwFlags, topMargin);
}


void DrawButtonRect(HDC hdc, int lineWidth, HFONT font, const CRect &rect, const wchar_t *text, bool disabled, bool pushed, DWORD dwFlags, uint32 topMargin)
{
	DrawButtonRectImpl(hdc, lineWidth, font, rect, text, disabled, pushed, dwFlags, topMargin);
}


//////////////////////////////////////////////////////////////////////////////////
// Misc functions


void ErrorBox(UINT nStringID, CWnd *parent)
{
	CString str;
	BOOL resourceLoaded = str.LoadString(nStringID);
	if(!resourceLoaded)
	{
		str.Format(_T("Resource string %u not found."), nStringID);
	}
	MPT_ASSERT(resourceLoaded);
	Reporting::Error(str, _T("Error!"), parent);
}


CString GetWindowTextString(const CWnd &wnd)
{
	CString result;
	wnd.GetWindowText(result);
	return result;
}


CString FormatFileSize(uint64 fileSize)
{
	static constexpr std::array<const TCHAR *, 4> Unit = {_T(" B"), _T(" KB"), _T(" MB"), _T(" GB")};
	double size = static_cast<double>(fileSize);
	for(int i = 0; i < 4; i++)
	{
		if(size < 1024.0 || i == 3)
		{
			// Variable-length formatting may decide on a whim to switch to scientific formatting, so used a fixed width and trim manually...
			CString s = mpt::cfmt::fix(size, 2);
			if(i == 0)
			{
				if(int pos = s.ReverseFind(_T('.')); pos >= 0)
					s.Delete(pos, s.GetLength() - pos);
			}
			return s + Unit[i];
		}
		size /= 1024.0;
	}
	return _T("");
}


CString FormatOrderRow(uint32 value)
{
	const bool rowAndOrderNumbersHex = TrackerSettings::Instance().patternSetup & PatternSetup::RowAndOrderNumbersHex;
	if(rowAndOrderNumbersHex)
		return mpt::cfmt::HEX0<2>(value);
	else
		return mpt::cfmt::val(value);
}


////////////////////////////////////////////////////////////////////////////////
// CFastBitmap 8-bit output / 4-bit input
// useful for lots of small blits with color mapping
// combined in one big blit

void CFastBitmap::Init(MODPLUGDIB *lpTextDib)
{
	m_nBlendOffset = 0;
	m_pTextDib = lpTextDib;
	MemsetZero(m_Dib.bmiHeader);
	m_nTextColor = 0;
	m_nBkColor = 1;
	m_Dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_Dib.bmiHeader.biWidth = 0;	// Set later
	m_Dib.bmiHeader.biHeight = 0;	// Ditto
	m_Dib.bmiHeader.biPlanes = 1;
	m_Dib.bmiHeader.biBitCount = 8;
	m_Dib.bmiHeader.biCompression = BI_RGB;
	m_Dib.bmiHeader.biSizeImage = 0;
	m_Dib.bmiHeader.biXPelsPerMeter = 96;
	m_Dib.bmiHeader.biYPelsPerMeter = 96;
	m_Dib.bmiHeader.biClrUsed = 0;
	m_Dib.bmiHeader.biClrImportant = 256; // MAX_MODPALETTECOLORS;
	m_n4BitPalette[0] = (BYTE)m_nTextColor;
	m_n4BitPalette[4] = MODCOLOR_SEPSHADOW;
	m_n4BitPalette[12] = MODCOLOR_SEPFACE;
	m_n4BitPalette[14] = MODCOLOR_SEPHILITE;
	m_n4BitPalette[15] = (BYTE)m_nBkColor;
}


void CFastBitmap::Blit(HDC hdc, int x, int y, int cx, int cy, const std::vector<BitmapTintSpan> &tints)
{
    if(m_smoothUsed || !tints.empty())
    {
        cx = std::min(cx, static_cast<int>(m_Dib.bmiHeader.biWidth));
        cy = std::min(cy, static_cast<int>(m_Dib.bmiHeader.biHeight));
        if(cx <= 0 || cy <= 0) return;
        m_rgbPixels.resize(static_cast<size_t>(cx) * cy);
        const auto toColor = [&](unsigned index) { const auto &q = m_Dib.bmiColors[index]; return RGB(q.rgbRed, q.rgbGreen, q.rgbBlue); };
        const auto isBackground = [](unsigned index) { index &= 0x7F; return index == MODCOLOR_BACKNORMAL || index == MODCOLOR_BACKHILIGHT || index == MODCOLOR_2NDHIGHLIGHT; };
        const auto rgbPixel = [](COLORREF color) { return (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color); };
        for(int row = 0; row < cy; row++)
        {
            size_t span = 0;
            const size_t offset = (m_Dib.bmiHeader.biHeight - 1 - row) * m_Dib.bmiHeader.biWidth;
            for(int col = 0; col < cx; col++)
            {
                while(span < tints.size() && col >= tints[span].right) span++;
                const bool tinted = span < tints.size() && col >= tints[span].left;
                const unsigned index = m_Dib.DibBits[offset + col];
                auto color = toColor(index);
                if(tinted && isBackground(index)) color = SynMPTTheme::Mix(color, tints[span].color, 16);
                const auto coverage = m_coverage[offset + col];
                if(coverage)
                {
                    const unsigned backgroundIndex = coverage >> 4;
                    auto background = toColor(backgroundIndex);
                    if(tinted && isBackground(backgroundIndex)) background = SynMPTTheme::Mix(background, tints[span].color, 16);
                    color = SynMPTTheme::Mix(background, color, (coverage & 15) * 100 / 15);
                }
                m_rgbPixels[static_cast<size_t>(row) * cx + col] = rgbPixel(color);
            }
        }
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = cx; info.bmiHeader.biHeight = -cy; info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        ::SetDIBitsToDevice(hdc, x, y, cx, cy, 0, 0, 0, cy, m_rgbPixels.data(), &info, DIB_RGB_COLORS);
        return;
    }
	SetDIBitsToDevice(	hdc,
						x,
						y,
						cx,
						cy,
						0,
						m_Dib.bmiHeader.biHeight - cy,
						0,
						m_Dib.bmiHeader.biHeight,
						&m_Dib.DibBits[0],
						(LPBITMAPINFO)&m_Dib,
						DIB_RGB_COLORS);
}


void CFastBitmap::SetColor(UINT nIndex, COLORREF cr)
{
	if (nIndex < 256)
	{
		m_Dib.bmiColors[nIndex].rgbRed = GetRValue(cr);
		m_Dib.bmiColors[nIndex].rgbGreen = GetGValue(cr);
		m_Dib.bmiColors[nIndex].rgbBlue = GetBValue(cr);
	}
}


void CFastBitmap::SetAllColors(UINT nBaseIndex, UINT nColors, COLORREF *pcr)
{
	for (UINT i=0; i<nColors; i++)
	{
		SetColor(nBaseIndex+i, pcr[i]);
	}
}


void CFastBitmap::SetBlendColor(COLORREF cr)
{
	UINT r = GetRValue(cr);
	UINT g = GetGValue(cr);
	UINT b = GetBValue(cr);
	for (UINT i=0; i<BLEND_OFFSET; i++)
	{
		UINT m = (m_Dib.bmiColors[i].rgbRed >> 2)
				+ (m_Dib.bmiColors[i].rgbGreen >> 1)
				+ (m_Dib.bmiColors[i].rgbBlue >> 2);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbRed = static_cast<BYTE>((m + r)>>1);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbGreen = static_cast<BYTE>((m + g)>>1);
		m_Dib.bmiColors[i|BLEND_OFFSET].rgbBlue = static_cast<BYTE>((m + b)>>1);
	}
}


// Monochrome 4-bit bitmap (0=text, !0 = back)
void CFastBitmap::TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, MODPLUGDIB *lpdib)
{
	const uint8 *psrc;
	BYTE *pdest;
	UINT x1, x2;
	int srcwidth, srcinc;

	m_n4BitPalette[0] = (BYTE)m_nTextColor;
	m_n4BitPalette[15] = (BYTE)m_nBkColor;
	if (x < 0)
	{
		cx += x;
		x = 0;
	}
	if (y < 0)
	{
		cy += y;
		y = 0;
	}
	if ((x >= m_Dib.bmiHeader.biWidth) || (y >= m_Dib.bmiHeader.biHeight)) return;
	if (x+cx >= m_Dib.bmiHeader.biWidth) cx = m_Dib.bmiHeader.biWidth - x;
	if (y+cy >= m_Dib.bmiHeader.biHeight) cy = m_Dib.bmiHeader.biHeight - y;
	if (!lpdib) lpdib = m_pTextDib;
	if ((cx <= 0) || (cy <= 0) || (!lpdib)) return;
	srcwidth = (lpdib->bmiHeader.biWidth+1) >> 1;
	srcinc = srcwidth;
	if (((int)lpdib->bmiHeader.biHeight) > 0)
	{
		srcy = lpdib->bmiHeader.biHeight - 1 - srcy;
		srcinc = -srcinc;
	}
	x1 = srcx & 1;
	x2 = x1 + cx;
	pdest = &m_Dib.DibBits[((m_Dib.bmiHeader.biHeight - 1 - y) << m_nXShiftFactor) + x];
	psrc = lpdib->lpDibBits + (srcx >> 1) + (srcy * srcwidth);
	for (int iy=0; iy<cy; iy++)
	{
        auto *coverage = m_coverage.data() + (pdest - m_Dib.DibBits.data());
        std::fill_n(coverage, cx, uint16(0));
        if(lpdib->antialiased)
        {
            m_smoothUsed = true;
            for(int column = 0; column < cx; column++)
            {
                const unsigned sourceX = x1 + column;
                const unsigned source = psrc[sourceX / 2];
                const unsigned pixel = (sourceX & 1) ? source & 15 : source >> 4;
                if(lpdib->separatorStartX >= 0 && srcx + column >= lpdib->separatorStartX)
                    pdest[column] = static_cast<uint8>(m_n4BitPalette[pixel] + m_nBlendOffset);
                else
                {
                    pdest[column] = static_cast<uint8>((pixel == 15 ? m_nBkColor : m_nTextColor) + m_nBlendOffset);
                    if(pixel > 0 && pixel < 15) coverage[column] = static_cast<uint16>(((m_nBkColor + m_nBlendOffset) << 4) | (15 - pixel));
                }
            }
            pdest -= m_Dib.bmiHeader.biWidth; psrc += srcinc; continue;
        }
		uint8 *p = pdest;
		UINT ix = x1;
		if (ix&1)
		{
			UINT b = psrc[ix >> 1];
			*p++ = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
			ix++;
		}
		while (ix+1 < x2)
		{
			UINT b = psrc[ix >> 1];
			p[0] = m_n4BitPalette[b >> 4]+m_nBlendOffset;
			p[1] = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
			ix+=2;
			p+=2;
		}
		if (x2&1)
		{
			UINT b = psrc[ix >> 1];
			*p++ = m_n4BitPalette[b >> 4]+m_nBlendOffset;
		}
		pdest -= m_Dib.bmiHeader.biWidth;
		psrc += srcinc;
	}
}


void CFastBitmap::FillRect(int x, int y, int width, int height, uint8 color)
{
	const int stride = m_Dib.bmiHeader.biWidth, rows = m_Dib.bmiHeader.biHeight;
	const int right = std::min(x + width, stride), bottom = std::min(y + height, rows);
	for(int row = std::max(0, y); row < bottom; row++)
		if(right > std::max(0, x))
        {
            const size_t offset = (rows - row - 1) * stride + std::max(0, x);
			std::fill_n(m_Dib.DibBits.begin() + offset, right - std::max(0, x), static_cast<uint8>(color + m_nBlendOffset));
            std::fill_n(m_coverage.begin() + offset, right - std::max(0, x), uint16(0));
        }
}

void CFastBitmap::SetSize(int x, int y)
{
	if(x > 4)
	{
		// Compute the required shift factor for obtaining a power-of-two bitmap width
		m_nXShiftFactor = 1;
		x--;
		while(x >>= 1)
		{
			m_nXShiftFactor++;
		}
	} else
	{
		// Bitmaps rows are aligned to 4 bytes, so let this bitmap be exactly 4 pixels wide.
		m_nXShiftFactor = 2;
	}

	x = (1 << m_nXShiftFactor);
	if(m_Dib.DibBits.size() != static_cast<size_t>(y << m_nXShiftFactor))
    {
        m_Dib.DibBits.resize(y << m_nXShiftFactor);
        m_coverage.assign(m_Dib.DibBits.size(), 0);
    }
	m_Dib.bmiHeader.biWidth = x;
	m_Dib.bmiHeader.biHeight = y;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Restore / save plugin list
//

static const auto PLUGFORMAT_FILENAME = MPT_UFORMAT("Plugin{}");
static const auto PLUGFORMAT_TAGS = MPT_UFORMAT("Plugin{}.Tags");
static const auto PLUGFORMAT_TAGS_BUILTIN = MPT_UFORMAT("Plugin{}{}.Tags");
static const auto PLUGFORMAT_LIBNAME = MPT_UFORMAT("Plugin{}.LibraryName");
static const auto PLUGFORMAT_SHELLID = MPT_UFORMAT("Plugin{}.ShellPluginID");

void CTrackApp::InitializeDXPlugins()
{
	m_pPluginManager = new CVstPluginManager;
	const size_t numPlugins = GetSettings().Read<int32>(U_("VST Plugins"), U_("NumPlugins"), 0);

	bool maskCrashes = TrackerSettings::Instance().BrokenPluginsWorkaroundVSTMaskAllCrashes;

	std::vector<VSTPluginLib *> nonFoundPlugs;
	const mpt::PathString failedPlugin = GetPluginState().Read<mpt::PathString>(U_("VST Plugins"), U_("FailedPlugin"));
	ConfirmAnswer skipFailed = cnfCancel;

	CDialog pluginScanDlg;
	CWnd *textWnd = nullptr;
	DWORD64 scanStart = Util::GetTickCount64();

	// Read tags for built-in plugins
	for(auto &plug : *m_pPluginManager)
	{
		plug->tags = GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2)));
	}

	// Restructured plugin cache
	if(TrackerSettings::Instance().PreviousSettingsVersion < MPT_V("1.27.00.15"))
	{
		DeleteFile(m_szPluginCacheFileName.AsNative().c_str());
		GetPluginCache().InvalidateCache();
	}

	m_pPluginManager->reserve(numPlugins);
	auto scanFormat = MPT_CFORMAT("Scanning Plugin {} / {}...\n{}");
	for(size_t plug = 0; plug < numPlugins; plug++)
	{
		const mpt::PathString plugPath = PathInstallRelativeToAbsolute(GetSettings().Read<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_FILENAME(plug)));
		if(plugPath.empty())
			continue;

		if(!pluginScanDlg.m_hWnd && Util::GetTickCount64() >= scanStart + 2000)
		{
			// If this is taking too long, show the user what they're waiting for.
			pluginScanDlg.Create(IDD_SCANPLUGINS, gpSplashScreen);
			pluginScanDlg.ShowWindow(SW_SHOW);
			pluginScanDlg.CenterWindow(gpSplashScreen);
			textWnd = pluginScanDlg.GetDlgItem(IDC_SCANTEXT);
		} else if(pluginScanDlg.m_hWnd && Util::GetTickCount64() >= scanStart + 30)
		{
			textWnd->SetWindowText(scanFormat(plug + 1, numPlugins + 1, plugPath));
			MSG msg;
			while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}
			scanStart = Util::GetTickCount64();
		}

		if(plugPath == failedPlugin)
		{
			GetPluginState().Remove(U_("VST Plugins"), U_("FailedPlugin"));
			GetPluginState().Flush(TrackerSettings::Instance().BrokenPluginsWorkaroundSyncStartupCrashRecovery ? Caching::WriteThrough : Caching::WriteBack);
			if(skipFailed == cnfCancel)
			{
				const CString text = MPT_CFORMAT("The following plugin has previously crashed OpenMPT during initialisation:\n\n{}\n\nDo you still want to load it?")
					(failedPlugin.ToCString());
				skipFailed = Reporting::Confirm(text, false, true, &pluginScanDlg);
			}

			if(skipFailed == cnfNo)
				continue;
		}

		const uint32 shellPluginID = mpt::parse_hex<uint32>(GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_SHELLID(plug)));
		bool plugFound = true;
		for(VSTPluginLib *lib : m_pPluginManager->AddPlugin(plugPath, maskCrashes, true, &plugFound, shellPluginID))
		{
			if(lib->libraryName == P_("MIDI Input Output") && lib->pluginId1 == PLUGMAGIC('V', 's', 't', 'P') && lib->pluginId2 == PLUGMAGIC('M', 'M', 'I', 'D') && !lib->shellPluginID)
			{
				// This appears to be an old version of our MIDI I/O plugin, which is now built right into the main executable.
				m_pPluginManager->RemovePlugin(lib);
				continue;
			}
			if(!plugFound)
				nonFoundPlugs.push_back(lib);
			if(shellPluginID && lib->shellPluginID != shellPluginID)
				continue;

			lib->tags = GetSettings().Read<mpt::ustring>(U_("VST Plugins"), PLUGFORMAT_TAGS(plug));
			if(shellPluginID != 0)
			{
				if(mpt::PathString libName = GetSettings().Read<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_LIBNAME(plug)); !libName.empty())
					lib->libraryName = std::move(libName);
			}

		}
	}
	GetPluginCache().Flush();
	if(pluginScanDlg.m_hWnd)
	{
		pluginScanDlg.DestroyWindow();
	}
	if(!nonFoundPlugs.empty())
	{
		PlugNotFoundDialog(nonFoundPlugs, nullptr).DoModal();
	}
}


void CTrackApp::UninitializeDXPlugins()
{
	if(!m_pPluginManager) return;

	size_t plugIndex = 0;
	for(auto &plug : *m_pPluginManager)
	{
		if(!plug->isBuiltIn)
		{
			mpt::PathString plugPath;
			if(theApp.IsPortableMode())
				plugPath = PathAbsoluteToInstallRelative(plug->dllPath);
			else
				plugPath = plug->dllPath;
			
			const auto libName = PLUGFORMAT_LIBNAME(plugIndex), shellID = PLUGFORMAT_SHELLID(plugIndex);
			if(plug->shellPluginID != 0)
			{
				theApp.GetSettings().Write(U_("VST Plugins"), libName, plug->libraryName);
				theApp.GetSettings().Write(U_("VST Plugins"), shellID, mpt::ufmt::HEX0<8>(plug->shellPluginID));
			} else
			{
				theApp.GetSettings().Remove(U_("VST Plugins"), libName);
				theApp.GetSettings().Remove(U_("VST Plugins"), shellID);
			}

			theApp.GetSettings().Write<mpt::PathString>(U_("VST Plugins"), PLUGFORMAT_FILENAME(plugIndex), plugPath);
			theApp.GetSettings().Write(U_("VST Plugins"), PLUGFORMAT_TAGS(plugIndex), plug->tags);

			plugIndex++;
		} else
		{
			theApp.GetSettings().Write(U_("VST Plugins"), PLUGFORMAT_TAGS_BUILTIN(mpt::ufmt::HEX0<8>(plug->pluginId1), mpt::ufmt::HEX0<8>(plug->pluginId2)), plug->tags);
		}
	}
	theApp.GetSettings().Write(U_("VST Plugins"), U_("NumPlugins"), static_cast<uint32>(plugIndex));

	delete m_pPluginManager;
	m_pPluginManager = nullptr;
}


///////////////////////////////////////////////////////////////////////////////////
// Internet-related functions

bool CTrackApp::OpenURL(const char *url)
{
	if(!url) return false;
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const std::string &url)
{
	return OpenURL(mpt::PathString::FromUTF8(url));
}

bool CTrackApp::OpenURL(const CString &url)
{
	return OpenURL(mpt::ToUnicode(url));
}

bool CTrackApp::OpenURL(const mpt::ustring &url)
{
	return OpenURL(mpt::PathString::FromUnicode(url));
}

bool CTrackApp::OpenURL(const mpt::PathString &lpszURL, const mpt::tstring &param)
{
	if(!lpszURL.empty() && theApp.m_pMainWnd)
	{
		if(reinterpret_cast<INT_PTR>(ShellExecute(
			theApp.m_pMainWnd->m_hWnd,
			_T("open"),
			lpszURL.AsNative().c_str(),
			param.empty() ? nullptr : param.c_str(),
			nullptr,
			SW_SHOW)) >= 32)
		{
			return true;
		}
	}
	return false;
}

bool CTrackApp::OpenDirectory(const mpt::PathString &directory)
{
	if(mpt::native_fs{}.is_file(directory))
		return OpenURL(P_("explorer.exe"), MPT_TFORMAT("/select,\"{}\"")(directory.AsNative()));
	else
		return OpenURL(directory);
}


CString CTrackApp::GetResamplingModeName(ResamplingMode mode, int length, bool addTaps)
{
	CString result;
	switch(mode)
	{
	case SRCMODE_NEAREST:
		result = (length > 1) ? _T("No Interpolation") : _T("None") ;
		break;
	case SRCMODE_LINEAR:
		result = _T("Linear");
		break;
	case SRCMODE_CUBIC:
		result = _T("Cubic");
		break;
	case SRCMODE_SINC8:
		result = _T("Sinc");
		break;
	case SRCMODE_SINC8LP:
		result = _T("Sinc");
		break;
	default:
		MPT_ASSERT_NOTREACHED();
		break;
	}
	if(Resampling::HasAA(mode))
	{
		result += (length > 1) ? _T(" + Low-Pass") : _T(" + LP");
	}
	if(addTaps)
	{
		result += MPT_CFORMAT(" ({} tap{})")(Resampling::Length(mode), (Resampling::Length(mode) != 1) ? CString(_T("s")) : CString(_T("")));
	}
	return result;
}


mpt::ustring CTrackApp::GetFriendlyMIDIPortName(const mpt::ustring &deviceName, bool isInputPort, bool addDeviceName)
{
	auto friendlyName = GetSettings().Read<mpt::ustring>(isInputPort ? U_("MIDI Input Ports") : U_("MIDI Output Ports"), deviceName, deviceName);
	if(friendlyName.empty())
		return deviceName;
	else if(addDeviceName && friendlyName != deviceName)
		return friendlyName + UL_(" (") + deviceName + UL_(")");
	else
		return friendlyName;
}


CString CTrackApp::GetFriendlyMIDIPortName(const CString &deviceName, bool isInputPort, bool addDeviceName)
{
	return mpt::ToCString(GetFriendlyMIDIPortName(mpt::ToUnicode(deviceName), isInputPort, addDeviceName));
}


bool ValidateMacroString(CEdit &wnd, const std::string_view prevMacro, bool isParametric, bool allowVariables, bool allowMultiline)
{
	CString macroStrT;
	wnd.GetWindowText(macroStrT);
	std::string macroStr = mpt::ToCharset(mpt::Charset::ASCII, macroStrT);

	bool allowed = true, caseChange = false;
	for(char &c : macroStr)
	{
		if(c >= 'G' && c <= 'Z')  // Potentially an allowed variable; lowercase it
		{
			caseChange = true;
			c = c - 'A' + 'a';
		}

		if(c == 'k')  // Previously, 'K' was used for MIDI channel
		{
			caseChange = true;
			c = 'c';
		} else if(c >= 'a' && c <= 'c' && !allowVariables)
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if(c >= 'd' && c <= 'f')  // abc can be variables, but def can be fixed
		{
			caseChange = true;
			c = c - 'a' + 'A';
		} else if((c >= 'a' && c <= 'c') || c == 'h' || c == 'm' || c == 'n' || c == 'o' || c == 'p' || c == 's' || c == 'u' || c == 'v' || c == 'x' || c == 'y')
		{
			if(!allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == 'z')
		{
			if(!isParametric || !allowVariables)
			{
				allowed = false;
				break;
			}
		} else if(c == '\r' || c == '\n')
		{
			if(!allowMultiline)
			{
				allowed = false;
				break;
			}
		} else if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == ' '))
		{
			allowed = false;
			break;
		}
	}

	if(!allowed)
	{
		// Replace text and keep cursor position if we just typed in an invalid character
		if(prevMacro != std::string_view{macroStr})
		{
			int start, end;
			wnd.GetSel(start, end);
			wnd.SetWindowText(mpt::ToCString(mpt::Charset::ASCII, static_cast<std::string>(prevMacro)));
			wnd.SetSel(start - 1, end - 1, true);
			MessageBeep(MB_OK);
		}
		return false;
	} else
	{
		if(caseChange)
		{
			// Replace text and keep cursor position if there was a case conversion
			int start, end;
			wnd.GetSel(start, end);
			wnd.SetWindowText(mpt::ToCString(mpt::Charset::ASCII, static_cast<std::string>(macroStr)));
			wnd.SetSel(start, end, true);
		}
		return true;
	}
}


static constexpr std::pair<const mpt::uchar*, const mpt::uchar*> SampleFormats[]
{
	{ UL_("Wave Files (*.wav)"), UL_("*.wav") },
#ifdef MPT_WITH_FLAC
	{ UL_("FLAC Files (*.flac,*.oga)"), UL_("*.flac;*.oga") },
#endif  // MPT_WITH_FLAC
#if defined(MPT_WITH_OPUSFILE)
	{ UL_("Opus Files (*.opus,*.oga)"), UL_("*.opus;*.oga") },
#endif  // MPT_WITH_OPUSFILE
#if defined(MPT_WITH_VORBISFILE) || defined(MPT_WITH_STBVORBIS)
	{ UL_("Ogg Vorbis Files (*.ogg,*.oga)"), UL_("*.ogg;*.oga") },
#endif  // VORBIS
#if defined(MPT_ENABLE_MP3_SAMPLES)
	{ UL_("MPEG Files (*.mp1,*.mp2,*.mp3)"), UL_("*.mp1;*.mp2;*.mp3") },
#endif  // MPT_ENABLE_MP3_SAMPLES
	{ UL_("XI Samples (*.xi)"), UL_("*.xi") },
	{ UL_("Impulse Tracker Samples (*.its)"), UL_("*.its") },
	{ UL_("Scream Tracker Samples (*.s3i,*.smp)"), UL_("*.s3i;*.smp") },
	{ UL_("OPL Instruments (*.sb0,*.sb2,*.sbi)"), UL_("*.sb0;*.sb2;*.sbi") },
	{ UL_("GF1 Patches (*.pat)"), UL_("*.pat") },
	{ UL_("Wave64 Files (*.w64)"), UL_("*.w64") },
	{ UL_("CAF Files (*.wav)"), UL_("*.caf") },
	{ UL_("AIFF Files (*.aiff,*.8svx)"), UL_("*.aif;*.aiff;*.iff;*.8sv;*.8svx;*.svx") },
	{ UL_("Sun Audio (*.au,*.snd)"), UL_("*.au;*.snd") },
	{ UL_("SNES BRR Files (*.brr)"), UL_("*.brr") },
};


mpt::ustring ConstructSampleFormatFileFilter(bool includeRaw)
{
	mpt::ustring s = U_("All Samples (*.wav,*.flac,*.xi,*.its,*.s3i,*.sbi,...)|");
	bool first = true;
	for (const auto& [name, exts] : SampleFormats)
	{
		if (!first)
			s += U_(";");
		else
			first = false;
		s += exts;
	}
#if defined(MPT_WITH_MEDIAFOUNDATION)
	std::vector<FileType> mediaFoundationTypes = CSoundFile::GetMediaFoundationFileTypes();
	s += ToFilterOnlyString(mediaFoundationTypes, true).ToUnicode();
#endif
	if (includeRaw)
	{
		s += U_(";*.raw;*.snd;*.pcm;*.sam");
	}
	s += U_("|");
	for (const auto& [name, exts] : SampleFormats)
	{
		s += name + U_("|");
		s += exts + U_("|");
	}
#if defined(MPT_WITH_MEDIAFOUNDATION)
	s += ToFilterString(mediaFoundationTypes, FileTypeFormatShowExtensions).ToUnicode();
#endif
	if (includeRaw)
	{
		s += U_("Raw Samples (*.raw,*.snd,*.pcm,*.sam)|*.raw;*.snd;*.pcm;*.sam|");
	}
	s += U_("All Files (*.*)|*.*||");
	return s;
}




// Shared application appearance. OS settings and external plugin editors are not modified.
namespace SynMPTTheme
{
COLORREF Color(int index)
{
    switch(index)
    {
    case COLOR_WINDOW: return RGB(44,47,53);
    case COLOR_BTNFACE: case COLOR_MENU: return RGB(56,59,66);
    case COLOR_APPWORKSPACE: return RGB(35,38,43);
    case COLOR_BTNSHADOW: return RGB(31,34,39);
    case COLOR_BTNHIGHLIGHT: return RGB(80,85,94);
    case COLOR_3DDKSHADOW: return RGB(24,27,31);
    case COLOR_3DLIGHT: return RGB(68,73,81);
    case COLOR_WINDOWTEXT: case COLOR_BTNTEXT: case COLOR_MENUTEXT: return RGB(226,229,235);
    case COLOR_GRAYTEXT: return RGB(144,151,163);
    case COLOR_HIGHLIGHT: return RGB(62,99,137);
    case COLOR_HIGHLIGHTTEXT: return RGB(250,251,253);
    case COLOR_INFOBK: return RGB(64,68,76);
    case COLOR_INFOTEXT: return RGB(235,237,241);
    default: return ::GetSysColor(index);
    }
}

HBRUSH Brush(int index)
{
    static std::map<COLORREF, HBRUSH> brushes;
    const auto color = Color(index);
    const auto found = brushes.find(color);
    if(found != brushes.end()) return found->second;
    return brushes[color] = ::CreateSolidBrush(color);
}

COLORREF Mix(COLORREF background, COLORREF accent, unsigned percent)
{
    percent = std::min(percent, 100u);
    const auto component = [percent](unsigned base, unsigned value) { return (base * (100 - percent) + value * percent + 50) / 100; };
    return RGB(component(GetRValue(background), GetRValue(accent)), component(GetGValue(background), GetGValue(accent)), component(GetBValue(background), GetBValue(accent)));
}

void DrawAction(HDC dc, const RECT &bounds, const wchar_t *label, COLORREF accent, bool active, bool recordDot)
{
    const int saved = ::SaveDC(dc);
    CRect rect(bounds); rect.DeflateRect(1, 1);
    ::SetDCBrushColor(dc, active ? accent : Color(COLOR_BTNFACE));
    ::FillRect(dc, rect, GetStockBrush(DC_BRUSH));
    ::FrameRect(dc, rect, Brush(COLOR_BTNSHADOW));
    if(recordDot)
    {
        const int diameter = std::max(4, std::min(rect.Width(), rect.Height()) - 6);
        const int x = rect.CenterPoint().x, y = rect.CenterPoint().y;
        ::SelectObject(dc, GetStockObject(DC_PEN)); ::SelectObject(dc, GetStockObject(DC_BRUSH));
        ::SetDCPenColor(dc, active ? RGB(255,222,226) : RGB(237,95,112));
        ::SetDCBrushColor(dc, active ? RGB(255,237,239) : RGB(174,53,70));
        ::Ellipse(dc, x - diameter / 2, y - diameter / 2, x + (diameter + 1) / 2, y + (diameter + 1) / 2);
    } else
    {
        ::SelectObject(dc, CMainFrame::GetGUIFont());
        ::SetTextColor(dc, active ? RGB(21,28,36) : Color(COLOR_BTNTEXT)); ::SetBkMode(dc, TRANSPARENT);
        ::DrawTextW(dc, label, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    ::RestoreDC(dc, saved);
}

namespace
{
std::vector<HWND> roots;
std::wstring Class(HWND window) { wchar_t name[128]{}; ::GetClassNameW(window, name, 128); return name; }
constexpr UINT_PTR ThemeSubclassID = 0x53594E54;

struct MenuEntry { HMENU menu; std::wstring text; bool bar, separator, submenu; };
std::map<ULONG_PTR, std::unique_ptr<MenuEntry>> menuEntries;

void StyleMenu(HMENU menu, bool bar)
{
    if(!menu) return;
    MENUINFO background{}; background.cbSize = sizeof(background); background.fMask = MIM_BACKGROUND;
    background.hbrBack = Brush(COLOR_MENU); ::SetMenuInfo(menu, &background);
    for(int item = 0; item < ::GetMenuItemCount(menu); item++)
    {
        wchar_t label[512]{}; MENUITEMINFOW info{}; info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_SUBMENU | MIIM_STRING;
        info.dwTypeData = label; info.cch = 511;
        if(!::GetMenuItemInfoW(menu, item, TRUE, &info)) continue;
        if(info.hSubMenu) StyleMenu(info.hSubMenu, false);
        auto found = menuEntries.find(info.dwItemData);
        if((info.fType & MFT_OWNERDRAW) && found == menuEntries.end()) continue;
        if(info.fType & MFT_BITMAP) continue;
        MenuEntry *entry = nullptr;
        if(found != menuEntries.end())
        {
            entry = found->second.get();
            if(label[0]) entry->text = label;
        } else
        {
            auto value = std::make_unique<MenuEntry>(MenuEntry{menu, label, bar, (info.fType & MFT_SEPARATOR) != 0, info.hSubMenu != nullptr});
            entry = value.get(); menuEntries.emplace(reinterpret_cast<ULONG_PTR>(entry), std::move(value));
        }
        info.fMask = MIIM_FTYPE | MIIM_DATA; info.fType |= MFT_OWNERDRAW; info.dwItemData = reinterpret_cast<ULONG_PTR>(entry);
        ::SetMenuItemInfoW(menu, item, TRUE, &info);
    }
}

bool MenuMessage(HWND window, UINT message, WPARAM, LPARAM lParam, LRESULT &result)
{
    if(message == WM_INITMENU || message == WM_INITMENUPOPUP) return false;
    if(message != WM_MEASUREITEM && message != WM_DRAWITEM) return false;
    const auto type = message == WM_MEASUREITEM ? reinterpret_cast<MEASUREITEMSTRUCT *>(lParam)->CtlType : reinterpret_cast<DRAWITEMSTRUCT *>(lParam)->CtlType;
    if(type != ODT_MENU) return false;
    const auto data = message == WM_MEASUREITEM ? reinterpret_cast<MEASUREITEMSTRUCT *>(lParam)->itemData : reinterpret_cast<DRAWITEMSTRUCT *>(lParam)->itemData;
    const auto found = menuEntries.find(data); if(found == menuEntries.end()) return false;
    const auto &entry = *found->second;
    const int dpi = HighDPISupport::GetDpiForWindow(window), padding = MulDiv(10, dpi, 96);
    if(message == WM_MEASUREITEM)
    {
        auto *measure = reinterpret_cast<MEASUREITEMSTRUCT *>(lParam);
        const auto dc = ::GetDC(window); const auto font = ::SelectObject(dc, CMainFrame::GetGUIFont());
        SIZE size{}; ::GetTextExtentPoint32W(dc, entry.text.c_str(), static_cast<int>(entry.text.size()), &size);
        ::SelectObject(dc, font); ::ReleaseDC(window, dc);
        measure->itemWidth = size.cx + padding * (entry.bar ? 2 : 5);
        measure->itemHeight = entry.separator ? padding : std::max<LONG>(size.cy + padding, MulDiv(22, dpi, 96));
    } else
    {
        auto *draw = reinterpret_cast<DRAWITEMSTRUCT *>(lParam); const int saved = ::SaveDC(draw->hDC);
        CRect rect(draw->rcItem); const bool selected = (draw->itemState & ODS_SELECTED) != 0;
        ::FillRect(draw->hDC, rect, Brush(selected ? COLOR_HIGHLIGHT : COLOR_MENU));
        if(entry.separator)
        {
            rect.top = rect.CenterPoint().y; rect.bottom = rect.top + 1; rect.DeflateRect(padding, 0);
            ::FillRect(draw->hDC, rect, Brush(COLOR_BTNHIGHLIGHT));
        } else
        {
            ::SetBkMode(draw->hDC, TRANSPARENT); ::SelectObject(draw->hDC, CMainFrame::GetGUIFont());
            ::SetTextColor(draw->hDC, Color(draw->itemState & (ODS_DISABLED | ODS_GRAYED) ? COLOR_GRAYTEXT : COLOR_MENUTEXT));
            if(draw->itemState & ODS_CHECKED)
            { CRect mark(rect); mark.right = mark.left + padding * 2; ::DrawTextW(draw->hDC, L"\u2022", -1, mark, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
            rect.left += padding * (entry.bar ? 1 : 2); rect.right -= padding * (entry.bar ? 1 : 2);
            const auto split = entry.text.find(L'\t'); const auto label = entry.text.substr(0, split);
            ::DrawTextW(draw->hDC, label.c_str(), -1, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if(split != std::wstring::npos) ::DrawTextW(draw->hDC, entry.text.c_str() + split + 1, -1, rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            if(entry.submenu && !entry.bar) { rect.left = rect.right; rect.right += padding * 2; ::DrawTextW(draw->hDC, L"\u203a", -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
        }
        ::RestoreDC(draw->hDC, saved);
    }
    result = TRUE; return true;
}


void PaintButton(HWND window, HDC dc)
{
    RECT bounds{}; ::GetClientRect(window, &bounds); CRect rect(bounds);
    const auto style = ::GetWindowLongPtr(window, GWL_STYLE);
    const auto type = style & BS_TYPEMASK;
    const bool check = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
    const bool radio = type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
    const bool group = type == BS_GROUPBOX;
    const bool enabled = ::IsWindowEnabled(window) != FALSE;
    const auto state = ::SendMessage(window, BM_GETSTATE, 0, 0);
    const bool pressed = (state & BST_PUSHED) != 0;
    wchar_t text[512]{}; ::GetWindowTextW(window, text, 512);
    ::FillRect(dc, rect, Brush(COLOR_BTNFACE));
    const auto oldFont = ::SelectObject(dc, reinterpret_cast<HFONT>(::SendMessage(window, WM_GETFONT, 0, 0)));
    ::SetBkMode(dc, TRANSPARENT); ::SetTextColor(dc, Color(enabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT));
    if(check || radio)
    {
        const int size = std::min(rect.Height(), MulDiv(13, HighDPISupport::GetDpiForWindow(window), 96));
        CRect mark(rect.left + 1, rect.CenterPoint().y - size / 2, rect.left + 1 + size, rect.CenterPoint().y - size / 2 + size);
        ::SelectObject(dc, GetStockObject(DC_PEN)); ::SelectObject(dc, GetStockObject(DC_BRUSH));
        ::SetDCPenColor(dc, Color(COLOR_GRAYTEXT)); ::SetDCBrushColor(dc, Color(COLOR_WINDOW));
        if(radio) ::Ellipse(dc, mark.left, mark.top, mark.right, mark.bottom);
        else { ::Rectangle(dc, mark.left, mark.top, mark.right, mark.bottom); }
        if(state & (BST_CHECKED | BST_INDETERMINATE))
        {
            mark.DeflateRect(std::max(2, size / 4), std::max(2, size / 4));
            ::SetDCBrushColor(dc, RGB(117,181,239)); ::SetDCPenColor(dc, RGB(117,181,239));
            if(radio) ::Ellipse(dc, mark.left, mark.top, mark.right, mark.bottom);
            else ::Rectangle(dc, mark.left, mark.top, mark.right, mark.bottom);
        }
        rect.left += size + MulDiv(5, HighDPISupport::GetDpiForWindow(window), 96);
        ::DrawTextW(dc, text, -1, rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else if(group)
    {
        rect.top += 6; ::FrameRect(dc, rect, Brush(COLOR_BTNHIGHLIGHT));
        rect.top -= 6; rect.left += 8;
        SIZE size{}; ::GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &size);
        CRect label(rect.left, rect.top, rect.left + size.cx + 4, rect.top + size.cy);
        ::FillRect(dc, label, Brush(COLOR_BTNFACE)); ::DrawTextW(dc, text, -1, label, DT_SINGLELINE | DT_LEFT);
    } else
    {
        rect.DeflateRect(1, 1);
        ::SetDCBrushColor(dc, pressed ? Color(COLOR_HIGHLIGHT) : Color(COLOR_3DLIGHT));
        ::FillRect(dc, rect, GetStockBrush(DC_BRUSH)); ::FrameRect(dc, rect, Brush(type == BS_DEFPUSHBUTTON ? COLOR_HIGHLIGHT : COLOR_BTNSHADOW));
        rect.DeflateRect(3, 1); ::DrawTextW(dc, text, -1, rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if((state & BST_FOCUS) && !group) { rect.DeflateRect(2, 2); ::DrawFocusRect(dc, rect); }
    ::SelectObject(dc, oldFont);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    const auto cls = Class(window);
    LRESULT menuResult = 0;
    if(MenuMessage(window, message, wParam, lParam, menuResult)) return menuResult;
    if(message == WM_INITMENU || message == WM_INITMENUPOPUP)
    {
        const auto result = ::DefSubclassProc(window, message, wParam, lParam);
        StyleMenu(reinterpret_cast<HMENU>(wParam), message == WM_INITMENU);
        return result;
    }
    if(message >= WM_CTLCOLORMSGBOX && message <= WM_CTLCOLORSTATIC)
    {
        const auto dc = reinterpret_cast<HDC>(wParam);
        const bool input = message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX;
        ::SetTextColor(dc, Color(::IsWindowEnabled(reinterpret_cast<HWND>(lParam)) ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
        ::SetBkColor(dc, Color(input ? COLOR_WINDOW : COLOR_BTNFACE));
        return reinterpret_cast<LRESULT>(Brush(input ? COLOR_WINDOW : COLOR_BTNFACE));
    }
    if(message == WM_ERASEBKGND)
    {
        auto *permanent = CWnd::FromHandlePermanent(window);
        if(cls == L"#32770" || cls == L"ToolbarWindow32" || cls == L"MDIClient" || cls == L"SysTabControl32"
            || (permanent && dynamic_cast<CControlBar *>(permanent)))
        { RECT rect{}; ::GetClientRect(window, &rect); ::FillRect(reinterpret_cast<HDC>(wParam), &rect, Brush(COLOR_BTNFACE)); return TRUE; }
    }
    if(message == WM_PAINT && cls == L"Button")
    {
        const auto style = ::GetWindowLongPtr(window, GWL_STYLE);
        if((style & BS_TYPEMASK) != BS_OWNERDRAW && !(style & (BS_BITMAP | BS_ICON)))
        {
            PAINTSTRUCT paint{}; HDC dc = ::BeginPaint(window, &paint); const int saved = ::SaveDC(dc);
            PaintButton(window, dc); ::RestoreDC(dc, saved); ::EndPaint(window, &paint); return 0;
        }
    }
    if(message == WM_NOTIFY && reinterpret_cast<NMHDR *>(lParam)->code == NM_CUSTOMDRAW)
    {
        auto *draw = reinterpret_cast<NMCUSTOMDRAW *>(lParam);
        const auto childClass = Class(draw->hdr.hwndFrom);
        if(childClass == L"ToolbarWindow32")
        {
            auto *toolbar = reinterpret_cast<NMTBCUSTOMDRAW *>(lParam);
            if(draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if(draw->dwDrawStage == CDDS_ITEMPREPAINT)
            {
                toolbar->clrText = Color(COLOR_BTNTEXT); toolbar->clrBtnFace = Color(COLOR_BTNFACE);
                toolbar->clrBtnHighlight = Color(COLOR_BTNHIGHLIGHT); toolbar->clrHighlightHotTrack = Color(COLOR_HIGHLIGHT);
                return TBCDRF_USECDCOLORS;
            }
        }
    }
    if(message == WM_NCDESTROY) ::RemoveWindowSubclass(window, WindowProc, ThemeSubclassID);
    return ::DefSubclassProc(window, message, wParam, lParam);
}

BOOL CALLBACK Apply(HWND window, LPARAM)
{
    DWORD_PTR data{};
    if(::GetWindowSubclass(window, WindowProc, ThemeSubclassID, &data)) return TRUE;
    const auto cls = Class(window);
    // Limit skinning to our registered native controls, retaining custom editor painting.
    if(cls == L"Button" || cls == L"Edit" || cls == L"Static" || cls == L"ComboBox" || cls == L"ListBox"
        || cls == L"SysTreeView32" || cls == L"SysListView32" || cls == L"SysHeader32" || cls == L"SysTabControl32"
        || cls == L"ToolbarWindow32" || cls == L"msctls_statusbar32" || cls == L"msctls_trackbar32"
        || cls == L"msctls_updown32" || cls == L"#32770" || cls == L"MDIClient" || cls.rfind(L"Afx", 0) == 0)
    {
        ::SetWindowSubclass(window, WindowProc, ThemeSubclassID, 0);
        if(cls == L"Button" || cls == L"SysTabControl32" || cls == L"SysTreeView32" || cls == L"SysListView32")
            ::SetWindowTheme(window, L"", L"");
        if(cls == L"SysTreeView32")
        { TreeView_SetBkColor(window, Color(COLOR_WINDOW)); TreeView_SetTextColor(window, Color(COLOR_WINDOWTEXT)); }
        if(cls == L"SysListView32")
        { ListView_SetBkColor(window, Color(COLOR_WINDOW)); ListView_SetTextBkColor(window, Color(COLOR_WINDOW)); ListView_SetTextColor(window, Color(COLOR_WINDOWTEXT)); }
        ::InvalidateRect(window, nullptr, TRUE);
    }
    return TRUE;
}
}

void RegisterWindow(HWND window)
{
    if(!window) return;
    if(std::find(roots.begin(), roots.end(), window) == roots.end()) roots.push_back(window);
    Apply(window, 0); ::EnumChildWindows(window, Apply, 0);
    StyleMenu(::GetMenu(window), true);
    ::DrawMenuBar(window);
    // Documented DWM attribute; older Windows versions simply ignore the request.
    using SetAttribute = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    static const auto library = ::LoadLibraryW(L"dwmapi.dll");
    static const auto setAttribute = library ? reinterpret_cast<SetAttribute>(::GetProcAddress(library, "DwmSetWindowAttribute")) : nullptr;
    if(setAttribute) { const BOOL dark = TRUE; setAttribute(window, 20, &dark, sizeof(dark)); }
}

void RefreshWindows()
{
    static DWORD last = 0;
    const auto now = ::GetTickCount();
    if(now - last < 250) return;
    last = now;
    for(auto entry = menuEntries.begin(); entry != menuEntries.end();)
        if(!::IsMenu(entry->second->menu)) entry = menuEntries.erase(entry); else ++entry;
    roots.erase(std::remove_if(roots.begin(), roots.end(), [](HWND window) { DWORD_PTR data{}; return !::IsWindow(window) || !::GetWindowSubclass(window, WindowProc, ThemeSubclassID, &data); }), roots.end());
    for(auto window : roots) ::EnumChildWindows(window, Apply, 0);
}

bool HandleTransportKey(const MSG &message)
{
    if((message.message != WM_KEYDOWN && message.message != WM_KEYUP) || message.wParam != 'T'
        || (::GetKeyState(VK_CONTROL) & 0x8000) || (::GetKeyState(VK_MENU) & 0x8000) || (::GetKeyState(VK_SHIFT) & 0x8000)) return false;
    const auto cls = Class(::GetFocus());
    if(cls == L"Edit" || cls.rfind(L"RichEdit", 0) == 0 || cls.rfind(L"RICHEDIT", 0) == 0 || cls == L"ComboBox") return false;
    auto *frame = CMainFrame::GetMainFrame();
    if(!frame || !frame->IsWindowEnabled() || (::GetActiveWindow() != frame->m_hWnd && ::GetAncestor(message.hwnd, GA_ROOTOWNER) != frame->m_hWnd)) return false;
    if(message.message == WM_KEYDOWN && !(message.lParam & (1L << 30))) frame->SendMessage(WM_COMMAND, ID_TRANSPORT_BRING_HERE);
    return true;
}
}

OPENMPT_NAMESPACE_END
