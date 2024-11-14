#include "logger.hpp"
#include "cpp_streamer_factory.hpp"
#include "timer.hpp"

#include <iostream>
#include <uv.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>

using namespace cpp_streamer;

static Logger* s_logger = nullptr;
static const int BENCH_MAX = 10000;
static const int INC_COUNT = 10;
static int is_exit = 0;
static int should_exit = 0;

class MediasoupPulls: public StreamerReport, public TimerInterface, public CppStreamerInterface
{
public:
    MediasoupPulls(uv_loop_t* loop,
            const std::string& src_url,
            size_t bench_count):TimerInterface(loop, 500)
                            , src_url_(src_url)
                            , bench_count_(bench_count)
    {
    }
    virtual ~MediasoupPulls()
    {

    }

public:
    void Start() {
        if (start_) {
            return;
        }
        start_ = true;
        start_time_ = std::chrono::system_clock::now();
        StartTimer();
    }

    void Stop() {
        if (!start_) {
            return;
        }
        start_ = false;
        post_done_ = true;
        stop_count_ = 0;
        for (size_t i = 0; i < whep_index_; i++) {
            CppStreamerInterface* mediasoup_puller = mediasoup_puller_vec[i];
            if (mediasoup_puller) {
                mediasoup_puller->StopNetwork();
            }
        }
    }

    void Inc(size_t count) {
        if (whep_index_ >= bench_count_) {
            bench_count_ += count;
            post_done_ = false;
        }
        printf("bench_count: %lu\n", bench_count_);
    }

    void Dec(size_t count) {
        if (whep_index_ >= bench_count_) {
            for (size_t i = 0; i < count && mediasoup_puller_vec.size() > 0; i++) {
                CppStreamerInterface* mediasoup_puller = mediasoup_puller_vec.back();
                mediasoup_puller->StopNetwork();
                mediasoup_puller_vec.pop_back();
                bench_count_ -= 1;
                whep_index_ -= 1;
            }
        } else {
            bench_count_ = std::max(whep_index_, bench_count_ - count);
        }
        printf("bench_count: %lu\n", bench_count_);
    }

    void SetBeginUid(size_t index) {
        begin_uid_ = index;
    }

public://CppStreamerInterface
    virtual std::string StreamerName() override {
        return "mediasoup_pull_bench";
    }
    virtual void SetLogger(Logger* logger) override {
        logger_ = logger;
    }
    virtual int AddSinker(CppStreamerInterface* sinker) override {
        return 0;
    }
    virtual int RemoveSinker(const std::string& name) override {
        return 0;
    }
    virtual int SourceData(Media_Packet_Ptr pkt_ptr) override {
        return 0;
    }
    virtual void StartNetwork(const std::string& url, void* loop_handle) override {
    }
    virtual void AddOption(const std::string& key, const std::string& value) override {
    }
    virtual void SetReporter(StreamerReport* reporter) override {
    }


//TimerInterface
protected:
    virtual void OnTimer() override {
        StartWheps();
    }

public:
    int MakeStreamers(uv_loop_t* loop_handle) {
        loop_ = loop_handle;
        return 0;
    }

private:
    std::string GetUrl(size_t index) {
        std::string url = src_url_;
        url += "&userId=";
        url += std::to_string(begin_uid_ + index);
        return url;
    }
    void StartWheps() {
        if (post_done_) {
            return;
        }
        size_t i = 0;
        for (i = whep_index_; i < bench_count_; i++) {
            if (i - room_connected_ >= INC_COUNT) {
                LogWarnf(logger_, "等待 %d 房间建立连接...", room_connected_);
                break;
            }
            LogWarnf(logger_, "StartWheps  index:%lu", i);
            std::string url = GetUrl(i);
            try {
                if (mediasoup_puller_vec.size() <= i) {
                    CppStreamerInterface* mediasoup_puller = CppStreamerFactory::MakeStreamer("mspull");
                    mediasoup_puller->SetLogger(logger_);
                    mediasoup_puller->SetReporter(this);
                    mediasoup_puller->AddSinker(this);
                    mediasoup_puller_vec.push_back(mediasoup_puller);
                }
                mediasoup_puller_vec[i]->StartNetwork(url, loop_);
            } catch(CppStreamException& e) {
                LogErrorf(logger_, "mediasoup pull start network exception:%s", e.what());
            }
        }
        whep_index_ = i;
        if (whep_index_ >= bench_count_) {
            post_done_ = true;
        }
    }

private:
    int GetWhipIndex(const std::string& name) {
        int index = -1;
        for (CppStreamerInterface* whip : mediasoup_puller_vec) {
            index++;
            if (!whip) {
                continue;
            }
            if (whip->StreamerName() == name) {
                return index;
            }
        }
        return -1;
    }

protected:
    virtual void OnReport(const std::string& name,
            const std::string& type,
            const std::string& value) override {
        LogWarnf(logger_, "report name:%s, type:%s, value:%s",
                name.c_str(), type.c_str(), value.c_str());
        if (type == "audio_produce") {
            if (value == "ready") {
                int index = GetWhipIndex(name);
                if (index < 0) {
                    LogErrorf(logger_, "fail to find whip by name:%s", name.c_str());
                } else {
                    LogWarnf(logger_, "whip streamer is ready, index:%d, name:%s",
                            index, name.c_str());
                }
            }
        }
        if (type == "close" && start_ == false) {
            stop_count_++;
            if (stop_count_ >= whep_index_) {
                is_exit = 1;
                uv_stop(loop_);
                LogInfof(logger_, "job is done.");
            }
        }
        if (type == "room") {
            room_connected_++;
            if (value == "join") {
                joined_succ_count_++;
                LogInfof(logger_, "进房成功 (%d/%d)", joined_succ_count_, whep_index_);
            } else {
                LogInfof(logger_, "进房失败 (%d/%d)", room_connected_ - joined_succ_count_, whep_index_);
            }
        }
    }

private:
    uv_loop_t* loop_ = nullptr;
    std::string src_url_;
    size_t bench_count_ = 1;
    size_t whep_index_ = 0;
    bool post_done_ = false;
    bool start_ = false;
    size_t stop_count_ = 0;
    size_t joined_succ_count_ = 0;
    size_t room_connected_ = 0;

    std::chrono::time_point<std::chrono::system_clock> start_time_;

private:
    Logger* logger_ = nullptr;
    std::vector<CppStreamerInterface*> mediasoup_puller_vec;
    int begin_uid_ = 1000;
};

uv_signal_t signal_handle;
std::shared_ptr<MediasoupPulls> mgr_ptr;

void on_signal(uv_signal_t* handle, int signum) {
    printf("Received SIGINT (Ctrl-C), shutting down gracefully...\n");
    uv_signal_stop(handle);
    should_exit = 1;
    mgr_ptr->Stop();
}

void on_stdin_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    for (int i = 0; i < nread; i++) {
      int c = buf->base[i];
      if (c == '+') {
          mgr_ptr->Inc(INC_COUNT);
      } else if (c == '-') {
          mgr_ptr->Dec(INC_COUNT);
      }
    }

    if (buf->base)
        free(buf->base);
}
/*
 *./mediasoup_pull_bench -i "https://xxxxx.com.cn:4443?roomId=100&apid=7689e48c-09ae-48ca-8973-ad5de69de5e8&vpid=aadbbb0b-2e4e-4ed8-8bd6-22e3c50b9fc1" -l 1.log
 */
int main(int argc, char** argv) {
    char src_url_name[516];
    char log_file[516];

    int opt = 0;
    bool src_url_name_ready = false;
    bool log_file_ready = false;
    int bench_count = 0;
    int begin_uid = 10000;

    while ((opt = getopt(argc, argv, "i:l:n:b:h")) != -1) {
        switch (opt) {
            case 'i': strncpy(src_url_name, optarg, sizeof(src_url_name)); src_url_name_ready = true; break;
            case 'n':
            {
                char count_sz[80];
                strncpy(count_sz, optarg, sizeof(count_sz));
                bench_count = atoi(count_sz);
                break;
            }
            case 'l': strncpy(log_file, optarg, sizeof(log_file)); log_file_ready = true; break;
            case 'b': {
                char count_sz[80];
                strncpy(count_sz, optarg, sizeof(count_sz));
                begin_uid = atoi(count_sz);
                break;
            }
            case 'h':
            default: 
            {
                printf("Usage: %s [-i whep url]\n\
    [-n bench count]\n\
    [-b begin uid]\n\
    [-l log file name]\n",
                    argv[0]); 
                return -1;
            }
        }
    }

    if (!src_url_name_ready) {
        std::cout << "please input whep url\r\n";
        return -1;
    }

    if (bench_count <= 0) {
        std::cout << "please input whep bench count.\r\n";
        return -1;
    }
    if (bench_count > BENCH_MAX) {
        std::cout << "bench count max is " << BENCH_MAX << ".\r\n";
        return -1;
    }

    s_logger = new Logger();
    if (log_file_ready) {
        s_logger->SetFilename(std::string(log_file));
        s_logger->EnableConsole();
    }
    s_logger->SetLevel(LOGGER_INFO_LEVEL);

    CppStreamerFactory::SetLogger(s_logger);
    CppStreamerFactory::SetLibPath("./output/lib");


    LogInfof(s_logger, "mediasoup pull bench is starting, input mediasoup pull url:%s, bench count:%d",
            src_url_name, bench_count);
    uv_loop_t* loop = uv_default_loop();
    uv_signal_init(loop, &signal_handle);
    uv_signal_start(&signal_handle, on_signal, SIGINT);

    uv_tty_t tty;
    int r = uv_tty_init(loop, &tty, 0, 0); // 0 表示 stdin
    if (r) {
        fprintf(stderr, "Failed to initialize TTY: %s\n", uv_strerror(r));
    } else {
        uv_tty_set_mode(&tty, UV_TTY_MODE_NORMAL);
        uv_read_start((uv_stream_t*)&tty, [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
            buf->base = (char*)malloc(suggested_size);
            buf->len = suggested_size;
        }, on_stdin_read);
    }

    mgr_ptr = std::make_shared<MediasoupPulls>(loop, 
            src_url_name, 
            (size_t)bench_count);

    mgr_ptr->SetLogger(s_logger);
    mgr_ptr->SetBeginUid(begin_uid);

    if (mgr_ptr->MakeStreamers(loop) < 0) {
        LogErrorf(s_logger, "call mediasoup pull bench error");
        return -1;
    }
    LogInfof(s_logger, "mediasoup pull bench is starting......");
    mgr_ptr->Start();
    while (is_exit == 0) {
        uv_run(loop, UV_RUN_DEFAULT);
    }
    uv_tty_reset_mode();
    uv_close((uv_handle_t*)&tty, nullptr);
    if (should_exit) {
      LogInfof(s_logger, "已手动退出程序");
    } else {
      LogInfof(s_logger, "异常退出");
    }
    return 0;
}