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
		::wrdClientDestroy(&client_);
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

	SOM_PASSPORT_BEGIN(Webrtc)
		SOM_PROPS(
			SOM_PROP_EX(onsdp, sdpGenCb_),
			SOM_PROP_EX(oncandidate, candidateGatheringCb_),
			SOM_PROP_EX(onmessage, remoteStringReceivedCb_)
		)
		SOM_FUNCS(
			SOM_FUNC(genSDP),
			SOM_FUNC(setRemoteSDP),
			SOM_FUNC(addRemoteCandidate)
		)
		SOM_PASSPORT_END
		
private:
	sciter::string peer_id_;
	WRDClient client_;
	std::string sdpGenCb_;
	std::string candidateGatheringCb_;
	std::string remoteStringReceivedCb_;
private:
	static void OnSDPGen(const WRDClient client, const char* sdp, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->sdpGenCb_.empty()) {
			pwin->call_function(webrtc->sdpGenCb_.c_str(), webrtc->peer_id_, sdp);
		}
	}
	static void OnCandidateGathering(const WRDClient client, const char* mid, int mline_index, const char* candidate, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->candidateGatheringCb_.empty()) {
			pwin->call_function(webrtc->candidateGatheringCb_.c_str(), webrtc->peer_id_, mid, mline_index, candidate);
		}
	}
	static void OnRemoteStringReceive(const WRDClient client, const char* data, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->remoteStringReceivedCb_.empty()) {
			pwin->call_function(webrtc->remoteStringReceivedCb_.c_str(), webrtc->peer_id_, data);
		}
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
