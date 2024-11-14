#ifndef MS_PULL_HPP
#define MS_PULL_HPP
#include "logger.hpp"
#include "http_client.hpp"
#include "peerconnection.hpp"
#include "cpp_streamer_interface.hpp"
#include "json.hpp"
#
extern "C" {
void* make_mspull_streamer();
void destroy_mspull_streamer(void* streamer);
}

namespace cpp_streamer
{
typedef enum
{
    BROADCASTER_INIT,
    BROADCASTER_ENTER_ROOM,
    BROADCASTER_REQUEST,//create broadcaster 
    BROADCASTER_TRANSPORT,//create transport
    BROADCASTER_TRANSPORT_CONNECT,//create transport connect
    BROADCASTER_VIDEO_CONSUME,//video consume
    BROADCASTER_AUDIO_CONSUME,//audio consume
    BROADCASTER_EXIT_ROOM,
    BROADCASTER_DONE
} BROADCASTER_STATE;

void OnConsumeDelay(uv_timer_t* handle);

class MsPull : public CppStreamerInterface , public HttpClientCallbackI, public PCStateReportI, public MediaCallbackI
{
    friend void OnConsumeDelay(uv_timer_t* handle);
public:
    MsPull();
    virtual ~MsPull();

public:
    virtual std::string StreamerName() override;
    virtual void SetLogger(Logger* logger) override;
    virtual int AddSinker(CppStreamerInterface* sinker) override;
    virtual int RemoveSinker(const std::string& name) override;
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override;
    virtual void StartNetwork(const std::string& url, void* loop_handle) override;
    virtual void AddOption(const std::string& key, const std::string& value) override;
    virtual void SetReporter(StreamerReport* reporter) override;
    virtual void StopNetwork() override;
protected:
    virtual void OnHttpRead(int ret, std::shared_ptr<HttpClientResponse> resp_ptr) override;

public:
    virtual void OnState(const std::string& type, const std::string& value) override;

public:
    virtual void OnReceiveMediaPacket(Media_Packet_Ptr pkt_ptr) override;

private:
    void ParseVideoConsume(const std::string& data);
    void ParseAudioConsume(const std::string& data);

private:
    void ReleaseHttpClient(HttpClient*& hc);
    bool GetHostInfoByUrl(const std::string& url, 
            std::string& host, 
            uint16_t& port, 
            std::string& roomId, 
            std::string& userId,
            std::string& produceUserId, 
            std::string& vProduceId,
            std::string& aProduceId);
    void EnterRoomRequest();
    void ExitRoomRequest();
    void BroadCasterRequest();
    void TransportRequest();
    void TransportConnectRequest();
    void VideoConsumeRequest();
    void AudioConsumeRequest();
    void HandleBroadCasterResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleTransportResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleTransportConnectResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleVideoConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleAudioConsumeResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleEnterRoomResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
    void HandleExitRoomResponse(std::shared_ptr<HttpClientResponse> resp_ptr);
private:
    void Report(const std::string& type, const std::string& value);

private:
    Logger* logger_ = nullptr;
    std::string name_;

private:
    HttpClient* hc_enter_           = nullptr;
    HttpClient* hc_exit_           = nullptr;
    HttpClient* hc_req_           = nullptr;
    HttpClient* hc_transport_     = nullptr;
    HttpClient* hc_trans_connect_ = nullptr;
    HttpClient* hc_video_consume_ = nullptr;
    HttpClient* hc_audio_consume_ = nullptr;
    uv_loop_t* loop_    = nullptr;
    PeerConnection* pc_ = nullptr;
    uv_timer_t consume_delay_timer_ = {};

private:
    std::string host_;
    uint16_t port_;
    std::string roomId_;
    std::string userId_;
    std::string peerId_; // 自定义代理id
    std::string producerUid_; // 要拉流的用户id
    BROADCASTER_STATE state_ = BROADCASTER_INIT;
    int64_t start_ms_ = -1;

private:
    std::string transport_id_;
    std::string video_produce_id_;
    std::string audio_produce_id_;
    std::string video_consume_id_;
    std::string audio_consume_id_;

private:
    std::string ice_pwd_;
    std::string ice_usr_frag_;
    std::string candidate_ip_;
    int candidate_port_;
    std::string candidate_proto_;
    std::string alg_value_;
};

}

#endif

