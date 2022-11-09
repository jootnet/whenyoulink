
#include <thread>

#include "sciter-x.h"
#include "sciter-x-window.hpp"

#include "wrd.h"

static sciter::om::hasset<sciter::window> pwin;

class Webrtc : public sciter::om::asset<Webrtc> {
public:
	Webrtc(sciter::string peer_id) {
		client_ = wrdCreateMaster();
		peer_id_ = peer_id;
	}
	~Webrtc() {
		wrdOnCandidateGathering(client_, OnCandidateGathering, this);
		wrdOnRemoteStringReceived(client_, OnRemoteStringReceive, this);
		wrdClientDestroy(&client_);
	}

	bool genSDP() {
		wrdGenSDP(client_, OnSDPGen, this);
		return true;
	}

	bool setRemoteSDP(sciter::string sdp) {
		wrdSetRemoteSDP(client_, aux::w2a(sdp));
		return true;
	}

	bool addRemoteCandidate(sciter::string mid, int mline_idx, sciter::string sdp) {
		wrdAddRemoteCandidate(client_, aux::w2a(mid), mline_idx, aux::w2a(sdp));
		return true;
	}

	SOM_PASSPORT_BEGIN(Webrtc)
		SOM_PROPS(
			SOM_PROP_EX(onsdp, sdpGenCb_),
			SOM_PROP_EX(oncandidate, candidateGatheringCb_),
			SOM_PROP_EX(onmessage, remoteStringReceivedCb_)
		)
		SOM_FUNCS(
			SOM_FUNC(setRemoteSDP),
			SOM_FUNC(addRemoteCandidate)
		)
		SOM_PASSPORT_END

private:
	sciter::string peer_id_;
	WRDClient client_;
	sciter::string sdpGenCb_;
	sciter::string candidateGatheringCb_;
	sciter::string remoteStringReceivedCb_;
private:
	static void OnSDPGen(const WRDClient client, const char* sdp, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->sdpGenCb_.empty()) {
			pwin->call_function(aux::w2a(webrtc->sdpGenCb_), webrtc->peer_id_, sdp);
		}
	}
	static void OnCandidateGathering(const WRDClient client, const char* mid, int mline_index, const char* candidate, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->candidateGatheringCb_.empty()) {
			pwin->call_function(aux::w2a(webrtc->candidateGatheringCb_), webrtc->peer_id_, mid, mline_index, candidate);
		}
	}
	static void OnRemoteStringReceive(const WRDClient client, const char* data, void* usr_param) {
		auto webrtc = reinterpret_cast<Webrtc*>(usr_param);
		if (!webrtc->remoteStringReceivedCb_.empty()) {
			pwin->call_function(aux::w2a(webrtc->remoteStringReceivedCb_), webrtc->peer_id_, data);
		}
	}
};

class frame : public sciter::window {
public:
	frame() : window(SW_TOOL | SW_MAIN) {

	}

	sciter::value wrdCreateMaster(sciter::string peer_id) {
		return sciter::value::wrap_asset(new Webrtc(peer_id));
	}

	SOM_PASSPORT_BEGIN(frame)
		SOM_FUNCS(
			SOM_FUNC(wrdCreateMaster)
		)
		SOM_PASSPORT_END
};

#include "resources.cpp"

int uimain(std::function<int()> run) {

	sciter::archive::instance().open(aux::elements_of(resources));

	pwin = new frame();

	pwin->load(WSTR("this://app/main.htm"));

	pwin->expand();

	return run();
}
