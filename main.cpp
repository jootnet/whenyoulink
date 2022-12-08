#define _CRT_SECURE_NO_WARNINGS

#include <sstream>
#include <regex>
#include <tuple>
#include <thread>
#include <Windows.h>
#include <shellapi.h>

#include "sciter-x.h"
#include "sciter-x-window.hpp"
#include "sciter-x-threads.h"

#include "wrd_def.h"
#include "Upgrader.h"
#include "resource.h"

static std::vector<std::string> c_split(const char* in, const char* delim);
static std::vector<std::tuple<std::string, std::string, int>> keys;
static void ExtractDlls();

static sciter::om::hasset<sciter::window> pwin;
static wrdCreateMasterFunc wrdCreateMaster;
static wrdOnCandidateGatheringFunc wrdOnCandidateGathering;
static wrdOnRemoteStringReceivedFunc wrdOnRemoteStringReceived;
static wrdClientDestroyFunc wrdClientDestroy;
static wrdGenSDPFunc wrdGenSDP;
static wrdSetRemoteSDPFunc wrdSetRemoteSDP;
static wrdAddRemoteCandidateFunc wrdAddRemoteCandidate;
static wrdSendStringFunc wrdSendString;

class Webrtc : public sciter::om::asset<Webrtc> {
public:
	Webrtc(sciter::string peer_id) {
		client_ = ::wrdCreateMaster();
		::wrdOnCandidateGathering(client_, OnCandidateGathering, this);
		::wrdOnRemoteStringReceived(client_, OnRemoteStringReceive, this);
		peer_id_ = peer_id;
	}
	~Webrtc() {
		if (nullptr != client_) {
			::wrdClientDestroy(&client_);
		}
	}

	bool genSDP() {
		::wrdGenSDP(client_, OnSDPGen, this);
		return true;
	}

	bool setRemoteSDP(sciter::string sdp) {
		::wrdSetRemoteSDP(client_, aux::w2a(sdp));
		return true;
	}

	bool addRemoteCandidate(sciter::string mid, int mline_idx, sciter::string sdp) {
		::wrdAddRemoteCandidate(client_, aux::w2a(mid), mline_idx, aux::w2a(sdp));
		return true;
	}

	bool sendMessage(sciter::string msg) {
		::wrdSendString(client_, aux::w2a(msg));
		return true;
	}

	bool close() {
		::wrdClientDestroy(&client_);
		return true;
	}

	SOM_PASSPORT_BEGIN(Webrtc)
		SOM_FUNCS(
			SOM_FUNC(genSDP),
			SOM_FUNC(setRemoteSDP),
			SOM_FUNC(addRemoteCandidate),
			SOM_FUNC_EX(send, sendMessage),
			SOM_FUNC(close)
		)
		SOM_PASSPORT_END
		
private:
	sciter::string peer_id_;
	WRDClient client_;
private:
	static void OnSDPGen(const WRDClient client, const char* sdp, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		std::stringstream ss;
		ss << "{\"type\":\"sdp\", \"peer_id\":\"" << aux::w2a(webrtc->peer_id_);
		ss << "\",\"sdp\":\"";
		// 转义
		for (auto i = 0; i < 99999999; ++i) {
			char c = sdp[i];
			if (c == '\0') break;
			if (c == '\r') {
				ss << "\\r";
			}
			else if (c == '\n') {
				ss << "\\n";
			}
			else ss << c;
		}
		ss << "\"}";
		BEHAVIOR_EVENT_PARAMS evt = { 0 };
		evt.name = WSTR("sdp");
		evt.data = ss.str();
		pwin->broadcast_event(evt);
	}
	static void OnCandidateGathering(const WRDClient client, const char* mid, int mline_index, const char* candidate, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		std::stringstream ss;
		ss << "{\"type\":\"candidate\", \"peer_id\":\"" << aux::w2a(webrtc->peer_id_);
		ss << "\",\"mid\":\"" << mid;
		ss << "\", \"mline_index\":" << std::ios::dec << mline_index;
		ss << ",\"sdp\":\"" << candidate;
		ss << "\"}";
		BEHAVIOR_EVENT_PARAMS evt = {0};
		evt.name = WSTR("candidate");
		evt.data = ss.str();
		pwin->broadcast_event(evt);
	}
	static void OnRemoteStringReceive(const WRDClient client, const char* data, void* usr_param) {
		std::cout << data << std::endl;
		if (std::strncmp(data, "mousedown ", 10) == 0) {
			INPUT input = { };
			input.type = INPUT_MOUSE;
			auto tokens = c_split(data, " ");
			if (tokens.size() < 4) return;
			int x, y, button;
			try {
				x = std::stoi(tokens[1]);
				y = std::stoi(tokens[2]);
				button = std::stoi(tokens[3]);
			}
			catch (const std::invalid_argument&) {
				return;
			}
			catch (const std::out_of_range&) {
				return;
			}
			if (button == 0) {
				input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			}
			else if (button == 1) {
				input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
			}
			else if (button == 2) {
				input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
			}
			else if (button == 3) {
				input.mi.dwFlags = MOUSEEVENTF_XDOWN;
				input.mi.mouseData = XBUTTON1;
			}
			else if (button == 4) {
				input.mi.dwFlags = MOUSEEVENTF_XDOWN;
				input.mi.mouseData = XBUTTON2;
			}
			input.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
			input.mi.dx = x * (65536/::GetSystemMetrics(SM_CXSCREEN));
			input.mi.dy = y * (65536 / ::GetSystemMetrics(SM_CYSCREEN));
			::SendInput(1, &input, sizeof input);
			return;
		}
		if (std::strncmp(data, "mouseup ", 8) == 0) {
			INPUT input = { };
			input.type = INPUT_MOUSE;
			auto tokens = c_split(data, " ");
			if (tokens.size() < 4) return;
			int x, y, button;
			try {
				x = std::stoi(tokens[1]);
				y = std::stoi(tokens[2]);
				button = std::stoi(tokens[3]);
			}
			catch (const std::invalid_argument&) {
				return;
			}
			catch (const std::out_of_range&) {
				return;
			}
			if (button == 0) {
				input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			}
			else if (button == 1) {
				input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
			}
			else if (button == 2) {
				input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
			}
			else if (button == 3) {
				input.mi.dwFlags = MOUSEEVENTF_XUP;
				input.mi.mouseData = XBUTTON1;
			}
			else if (button == 4) {
				input.mi.dwFlags = MOUSEEVENTF_XUP;
				input.mi.mouseData = XBUTTON2;
			}
			input.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
			input.mi.dx = x * (65536 / ::GetSystemMetrics(SM_CXSCREEN));
			input.mi.dy = y * (65536 / ::GetSystemMetrics(SM_CYSCREEN));
			::SendInput(1, &input, sizeof input);
			return;
		}
		if (std::strncmp(data, "mousemove ", 10) == 0) {
			INPUT input = { };
			input.type = INPUT_MOUSE;
			auto tokens = c_split(data, " ");
			if (tokens.size() < 3) return;
			int x, y;
			try {
				x = std::stoi(tokens[1]);
				y = std::stoi(tokens[2]);
			}
			catch (const std::invalid_argument&) {
				return;
			}
			catch (const std::out_of_range&) {
				return;
			}
			input.mi.dwFlags = MOUSEEVENTF_MOVE;
			input.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
			input.mi.dx = x * (65536 / ::GetSystemMetrics(SM_CXSCREEN));
			input.mi.dy = y * (65536 / ::GetSystemMetrics(SM_CYSCREEN));
			::SendInput(1, &input, sizeof input);
			return;
		}
		if (std::strncmp(data, "wheel ", 6) == 0) {
			INPUT input = { };
			input.type = INPUT_MOUSE;
			auto tokens = c_split(data, " ");
			if (tokens.size() < 5) return;
			double x, y, z;
			int mode;
			try {
				x = std::stod(tokens[1]);
				y = std::stod(tokens[2]);
				z = std::stod(tokens[3]);
				mode = std::stoi(tokens[4]);
			}
			catch (const std::invalid_argument&) {
				return;
			}
			catch (const std::out_of_range&) {
				return;
			}
			if (mode != 0) return;
			if (x != 0) {
				input.mi.mouseData = x > 0 ? WHEEL_DELTA : -WHEEL_DELTA;
				input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
			}
			else if (y != 0) {
				input.mi.mouseData = y > 0 ? WHEEL_DELTA : -WHEEL_DELTA;
				input.mi.dwFlags = MOUSEEVENTF_WHEEL;
			}
			else return;
			::SendInput(1, &input, sizeof input);
			return;
		}
		if (std::strncmp(data, "keydown ", 8) == 0 || std::strncmp(data, "keyup ", 6) == 0) {
			auto tokens = c_split(data, " ");
			if (tokens.size() < 3) return;
			auto up = std::strncmp(data, "keyup ", 6) == 0;
			INPUT input = {};
			input.type = INPUT_KEYBOARD;
			if (up) {
				input.ki.dwFlags = KEYEVENTF_KEYUP;
			}
			auto find = false;
			for (auto it = keys.begin(); it != keys.end(); ++it) {
				if (std::get<0>(*it) == tokens[1]/* && std::get<1>(*it) == tokens[2]*/) { // 暂不校验key，因为存在大小写和不同类型键盘，还没考虑好怎么做兼容
					input.ki.wVk = std::get<2>(*it);
					find = true;
					break;
				}
			}
			if (find) {
				::SendInput(1, &input, sizeof input);
			}
			return;
		}

		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		std::stringstream ss;
		ss << "{\"type\":\"remote_string\", \"peer_id\":\"" << aux::w2a(webrtc->peer_id_);
		ss << "\",\"data\":\"" << data;
		ss << "\"}";
		BEHAVIOR_EVENT_PARAMS evt = { 0 };
		evt.name = WSTR("remote_string");
		evt.data = ss.str();
		pwin->broadcast_event(evt);
	}
};

class WebrtcFactory : public sciter::om::asset<WebrtcFactory> {
public:
	sciter::value wrdCreateMaster(sciter::string peer_id) {
		return sciter::value::wrap_asset(new Webrtc(peer_id));
	}

	SOM_PASSPORT_BEGIN(WebrtcFactory)
		SOM_FUNCS(
			SOM_FUNC_EX(newMaster, wrdCreateMaster)
		)
		SOM_PASSPORT_END
};

int _ = []() {
	ExtractDlls();
	return 0;
}();

#include "resources.cpp"

int uimain(std::function<int()> run) {
	HMODULE libWrdPtr = LoadLibrary(TEXT("wrd"));
	wrdCreateMaster = (wrdCreateMasterFunc) GetProcAddress(libWrdPtr, "wrdCreateMaster");
	wrdOnCandidateGathering = (wrdOnCandidateGatheringFunc)GetProcAddress(libWrdPtr, "wrdOnCandidateGathering");
	wrdOnRemoteStringReceived = (wrdOnRemoteStringReceivedFunc)GetProcAddress(libWrdPtr, "wrdOnRemoteStringReceived");
	wrdClientDestroy = (wrdClientDestroyFunc)GetProcAddress(libWrdPtr, "wrdClientDestroy");
	wrdGenSDP = (wrdGenSDPFunc)GetProcAddress(libWrdPtr, "wrdGenSDP");
	wrdSetRemoteSDP = (wrdSetRemoteSDPFunc)GetProcAddress(libWrdPtr, "wrdSetRemoteSDP");
	wrdAddRemoteCandidate = (wrdAddRemoteCandidateFunc)GetProcAddress(libWrdPtr, "wrdAddRemoteCandidate");
	wrdSendString = (wrdSendStringFunc)GetProcAddress(libWrdPtr, "wrdSendString");
	//https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	keys.push_back(std::tuple<std::string, std::string, int>("Escape", "Escape", VK_ESCAPE));
	keys.push_back(std::tuple<std::string, std::string, int>("F1", "F1", VK_F1));
	keys.push_back(std::tuple<std::string, std::string, int>("F2", "F2", VK_F2));
	keys.push_back(std::tuple<std::string, std::string, int>("F3", "F3", VK_F3));
	keys.push_back(std::tuple<std::string, std::string, int>("F4", "F4", VK_F4));
	keys.push_back(std::tuple<std::string, std::string, int>("F5", "F5", VK_F5));
	keys.push_back(std::tuple<std::string, std::string, int>("F6", "F6", VK_F6));
	keys.push_back(std::tuple<std::string, std::string, int>("F7", "F7", VK_F7));
	keys.push_back(std::tuple<std::string, std::string, int>("F8", "F8", VK_F8));
	keys.push_back(std::tuple<std::string, std::string, int>("F9", "F9", VK_F9));
	keys.push_back(std::tuple<std::string, std::string, int>("F10", "F10", VK_F10));
	keys.push_back(std::tuple<std::string, std::string, int>("F11", "F11", VK_F11));
	keys.push_back(std::tuple<std::string, std::string, int>("F12", "F12", VK_F12));
	keys.push_back(std::tuple<std::string, std::string, int>("ScrollLock", "ScrollLock", VK_SCROLL));
	keys.push_back(std::tuple<std::string, std::string, int>("Pause", "Pause", VK_PAUSE));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit1", "1", '1'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit2", "2", '2'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit3", "3", '3'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit4", "4", '4'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit5", "5", '5'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit6", "6", '6'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit7", "7", '7'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit8", "8", '8'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit9", "9", '9'));
	keys.push_back(std::tuple<std::string, std::string, int>("Digit0", "0", '0'));
	keys.push_back(std::tuple<std::string, std::string, int>("Backspace", "Backspace", VK_BACK));
	keys.push_back(std::tuple<std::string, std::string, int>("Insert", "Insert", VK_INSERT));
	keys.push_back(std::tuple<std::string, std::string, int>("Home", "Home", VK_HOME));
	keys.push_back(std::tuple<std::string, std::string, int>("PageUp", "PageUp", VK_PRIOR));
	keys.push_back(std::tuple<std::string, std::string, int>("NumLock", "NumLock", VK_NUMLOCK));
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadDivide", "/", VK_DIVIDE));
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadMultiply", "*", VK_MULTIPLY));
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadSubtract", "-", VK_SUBTRACT));
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadAdd", "+", VK_ADD));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad9", "9", VK_NUMPAD9));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad8", "8", VK_NUMPAD8));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad7", "7", VK_NUMPAD7));
	keys.push_back(std::tuple<std::string, std::string, int>("PageDown", "PageDown", VK_NEXT));
	keys.push_back(std::tuple<std::string, std::string, int>("End", "End", VK_END));
	keys.push_back(std::tuple<std::string, std::string, int>("Delete", "Delete", VK_DELETE));
	keys.push_back(std::tuple<std::string, std::string, int>("Tab", "Tab", VK_TAB));
	keys.push_back(std::tuple<std::string, std::string, int>("CapsLock", "CapsLock", VK_CAPITAL));
	keys.push_back(std::tuple<std::string, std::string, int>("Enter", "Enter", VK_RETURN));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad4", "4", VK_NUMPAD4));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad5", "5", VK_NUMPAD5));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad6", "6", VK_NUMPAD6));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad3", "3", VK_NUMPAD3));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad2", "2", VK_NUMPAD2));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad1", "1", VK_NUMPAD1));
	keys.push_back(std::tuple<std::string, std::string, int>("ArrowUp", "ArrowUp", VK_UP));
	keys.push_back(std::tuple<std::string, std::string, int>("ShiftRight", "Shift", VK_RSHIFT));
	keys.push_back(std::tuple<std::string, std::string, int>("ShiftLeft", "Shift", VK_LSHIFT));
	keys.push_back(std::tuple<std::string, std::string, int>("ControlLeft", "Control", VK_LCONTROL));
	keys.push_back(std::tuple<std::string, std::string, int>("MetaLeft", "Meta", VK_LWIN));
	keys.push_back(std::tuple<std::string, std::string, int>("AltLeft", "Alt", VK_LMENU));
	keys.push_back(std::tuple<std::string, std::string, int>("Space", " ", VK_SPACE));
	keys.push_back(std::tuple<std::string, std::string, int>("AltRight", "Alt", VK_RMENU));
	keys.push_back(std::tuple<std::string, std::string, int>("MetaRight", "Meta", VK_RWIN));
	keys.push_back(std::tuple<std::string, std::string, int>("ControlRight", "Control", VK_RCONTROL));
	keys.push_back(std::tuple<std::string, std::string, int>("ArrowLeft", "ArrowLeft", VK_LEFT));
	keys.push_back(std::tuple<std::string, std::string, int>("ArrowDown", "ArrowDown", VK_DOWN));
	keys.push_back(std::tuple<std::string, std::string, int>("ArrowRight", "ArrowRight", VK_RIGHT));
	keys.push_back(std::tuple<std::string, std::string, int>("Numpad0", "0", VK_NUMPAD0));
	keys.push_back(std::tuple<std::string, std::string, int>("AudioVolumeDown", "AudioVolumeDown", VK_VOLUME_DOWN));
	keys.push_back(std::tuple<std::string, std::string, int>("AudioVolumeUp", "AudioVolumeUp", VK_VOLUME_UP));
	keys.push_back(std::tuple<std::string, std::string, int>("AudioVolumeMute", "AudioVolumeMute", VK_VOLUME_MUTE));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyA", "A", (int)'A'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyB", "B", (int)'B'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyC", "C", (int)'C'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyD", "D", (int)'D'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyE", "E", (int)'E'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyF", "F", (int)'F'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyG", "G", (int)'G'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyH", "H", (int)'H'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyI", "I", (int)'I'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyJ", "J", (int)'J'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyK", "K", (int)'K'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyL", "L", (int)'L'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyM", "M", (int)'M'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyN", "N", (int)'N'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyO", "O", (int)'O'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyP", "P", (int)'P'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyQ", "Q", (int)'Q'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyR", "R", (int)'R'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyS", "S", (int)'S'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyT", "T", (int)'T'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyU", "U", (int)'U'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyV", "V", (int)'V'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyW", "W", (int)'W'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyX", "X", (int)'X'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyY", "Y", (int)'Y'));
	keys.push_back(std::tuple<std::string, std::string, int>("KeyZ", "Z", (int)'Z'));

	// 这些_OEM_有点不太确定
	keys.push_back(std::tuple<std::string, std::string, int>("Minus", "-", VK_OEM_MINUS));
	keys.push_back(std::tuple<std::string, std::string, int>("Equal", "=", VK_OEM_PLUS));
	keys.push_back(std::tuple<std::string, std::string, int>("Period", ".", VK_OEM_PERIOD));
	keys.push_back(std::tuple<std::string, std::string, int>("Comma", ",", VK_OEM_COMMA));
	keys.push_back(std::tuple<std::string, std::string, int>("Semicolon", ";", VK_OEM_1));
	keys.push_back(std::tuple<std::string, std::string, int>("Slash", "/", VK_OEM_2));
	keys.push_back(std::tuple<std::string, std::string, int>("Backquote", "`", VK_OEM_3));
	keys.push_back(std::tuple<std::string, std::string, int>("BracketLeft", "[", VK_OEM_4));
	keys.push_back(std::tuple<std::string, std::string, int>("Backslash", "\\", VK_OEM_5));
	keys.push_back(std::tuple<std::string, std::string, int>("BracketRight", "]", VK_OEM_6));
	keys.push_back(std::tuple<std::string, std::string, int>("Quote", "'", VK_OEM_7));
	// 小键盘的点和回车，没有找到比较像的的vk
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadDecimal", ".", VK_OEM_PERIOD));
	keys.push_back(std::tuple<std::string, std::string, int>("NumpadEnter", "Enter", VK_RETURN));
	// windows的键盘右键按钮没有找到对应的值
	//keys.push_back(std::tuple<std::string, std::string, int>("ContextMenu", "ContextMenu", VK_BACK));
#ifdef _DEBUG
	::SciterSetOption(NULL, SCITER_SET_DEBUG_MODE, TRUE);
	::WinExec("inspector.exe", SW_SHOW);
#endif
	::SciterSetGlobalAsset(new WebrtcFactory);
	::SciterSetOption(NULL, SCITER_SET_SCRIPT_RUNTIME_FEATURES,
		ALLOW_FILE_IO |
		ALLOW_SOCKET_IO |
		ALLOW_EVAL |
		ALLOW_SYSINFO);

	sciter::archive::instance().open(aux::elements_of(resources));

	pwin = new sciter::window(SW_TOOL | SW_MAIN);
	
	pwin->load(WSTR("this://app/main.htm"));
	
	pwin->expand();
	
	std::thread([] {
		if (HasNewVersion(WSTR("whenyoulink.oss-cn-chengdu.aliyuncs.com"), WSTR("/whenyoulink.exe"))) {
			BEHAVIOR_EVENT_PARAMS evt = { 0 };
			evt.name = WSTR("has_new_version");
			evt.data = WSTR("https://whenyoulink.oss-cn-chengdu.aliyuncs.com/whenyoulink.exe");
			pwin->broadcast_event(evt);
		}
	}).detach();

	return run();
}

std::vector<std::string> c_split(const char* in, const char* delim) {
	std::regex re{ delim };
	return std::vector<std::string> {
		std::cregex_token_iterator(in, in + std::strlen(in), re, -1),
			std::cregex_token_iterator()
	};
}

BOOL DoesFileOrDirExist(const WCHAR* path)
{
	WIN32_FIND_DATAW fd;
	HANDLE handle;
	handle = FindFirstFileW(path, &fd);
	if (handle == INVALID_HANDLE_VALUE)
		return FALSE;
	FindClose(handle);
	return TRUE;
}

void ExtractResource(const HINSTANCE hInstance, WORD resourceID, LPCTSTR szFilename)
{
	// Find and load the resource
	HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceID), TEXT("BINARY"));
	HGLOBAL hFileResource = LoadResource(hInstance, hResource);

	// Open and map this to a disk file
	LPVOID lpFile = LockResource(hFileResource);
	DWORD dwSize = SizeofResource(hInstance, hResource);

	// Open the file and filemap
	HANDLE hFile = CreateFile(szFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, dwSize, NULL);
	LPVOID lpAddress = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 0);

	// Write the file
	CopyMemory(lpAddress, lpFile, dwSize);

	// Un-map the file and close the handles
	UnmapViewOfFile(lpAddress);
	CloseHandle(hFileMap);
	CloseHandle(hFile);
}

void ExtractDlls() {
	WCHAR path[MAX_PATH * 3 + 2];
	size_t pathLen;
	DWORD winRes;

	{
		unsigned i;
		DWORD d;
		winRes = GetTempPathW(MAX_PATH, path);
		if (winRes == 0 || winRes > MAX_PATH)
			return;
		pathLen = wcslen(path);
		d = (GetTickCount() << 12) ^ (GetCurrentThreadId() << 14) ^ GetCurrentProcessId();

		for (i = 0;; i++, d += GetTickCount())
		{
			if (i >= 100)
			{
				return;
			}
			wcscpy(path + pathLen, L"7z");

			{
				wchar_t* s = path + wcslen(path);
				uint32_t value = d;
				unsigned k;
				for (k = 0; k < 8; k++)
				{
					unsigned t = value & 0xF;
					value >>= 4;
					s[7 - k] = (wchar_t)((t < 10) ? ('0' + t) : ('A' + (t - 10)));
				}
				s[k] = '\0';
			}

			if (DoesFileOrDirExist(path))
				continue;
			if (CreateDirectoryW(path, NULL))
			{
				wcscat(path, TEXT("\\"));
				pathLen = wcslen(path);
				break;
			}
		}
	}

	{
		WCHAR exe7zdec[MAX_PATH] = { 0 };
		wcscat(exe7zdec, path);
		wcscat(exe7zdec, TEXT("7zdec.exe"));
		WCHAR dll7z[MAX_PATH] = { 0 };
		wcscat(dll7z, path);
		wcscat(dll7z, TEXT("dll.7z"));
		ExtractResource(THIS_HINSTANCE, IDR_EXE_7ZDEC, exe7zdec);
		ExtractResource(THIS_HINSTANCE, IDR_ZIP_DLL7Z, dll7z);
	}

	SetCurrentDirectory(path);

	{
		SHELLEXECUTEINFO ei;
		//UINT32 executeRes;

		memset(&ei, 0, sizeof(ei));
		ei.cbSize = sizeof(ei);
		ei.lpFile = TEXT("7zdec.exe");
		ei.lpDirectory = path;
		ei.fMask = SEE_MASK_NOCLOSEPROCESS
			| SEE_MASK_FLAG_DDEWAIT
			 | SEE_MASK_NO_CONSOLE 
			;
		ei.lpParameters = TEXT("e dll.7z");
		ei.nShow =  SW_HIDE;
		ShellExecuteEx(&ei);
		WaitForSingleObject(ei.hProcess, INFINITE);
		//executeRes = (UINT32)(UINT_PTR)ei.hInstApp;
	}
}