
#include <sstream>

#include "sciter-x.h"
#include "sciter-x-window.hpp"

#include "wrd.h"

static sciter::om::hasset<sciter::window> pwin;

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
		// ЧЄТе
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

#include "resources.cpp"

int uimain(std::function<int()> run) {
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

	return run();
}
