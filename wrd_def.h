#pragma once
typedef struct _tagWRDClient* WRDClient;

typedef void(__cdecl* WRDSDPGenCallback)(const WRDClient client, const char* sdp, void* usr_param);

typedef void(__cdecl* WRDCandidateGatheringCallback)(const WRDClient client, const char* mid, int mline_index, const char* candidate, void* usr_param);

typedef void(__cdecl* WRDRemoteImageReceivedCallback)(const WRDClient client, const unsigned char* argb, int w, int h, void* usr_param);

typedef void(__cdecl* WRDRemoteRawDataReceivedCallback)(const WRDClient client, const unsigned char* data, unsigned int data_len, void* usr_param);

typedef void(__cdecl* WRDRemoteStringReceivedCallback)(const WRDClient client, const char* data, void* usr_param);


typedef WRDClient(__cdecl* wrdCreateViewerFunc)(void);
typedef WRDClient(__cdecl* wrdCreateMasterFunc)(void);

typedef void(__cdecl* wrdGenSDPFunc)(WRDClient client, WRDSDPGenCallback gen_done_cb, void* usr_param);

typedef void(__cdecl* wrdOnCandidateGatheringFunc)(WRDClient client, WRDCandidateGatheringCallback candidate_gathering_cb, void* usr_param);

typedef bool(__cdecl* wrdSetRemoteSDPFunc)(WRDClient client, const char* sdp);

typedef bool(__cdecl* wrdAddRemoteCandidateFunc)(WRDClient client, const char* mid, int mline_index, const char* candidate);

typedef void(__cdecl* wrdOnRemoteImageReceivedFunc)(WRDClient client, WRDRemoteImageReceivedCallback image_received_cb, void* usr_param);

typedef void(__cdecl* wrdOnRemoteRawDataReceivedFunc)(WRDClient client, WRDRemoteRawDataReceivedCallback raw_data_received_cb, void* usr_param);

typedef void(__cdecl* wrdOnRemoteStringReceivedFunc)(WRDClient client, WRDRemoteStringReceivedCallback string_received_cb, void* usr_param);

typedef void(__cdecl* wrdSendRawDataFunc)(WRDClient client, const unsigned char* data, unsigned int data_len);

typedef void(__cdecl* wrdSendStringFunc)(WRDClient client, const char* data);

typedef void(__cdecl* wrdClientDestroyFunc)(WRDClient * client);