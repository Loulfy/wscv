#include <iostream>
#include <mongoose/mongoose.h>
#include <opencv2/opencv.hpp>

#include <wscv.pb.h>

using namespace std;

static sig_atomic_t s_signal_received = 0;
static const char *s_http_port = "9090";
static struct mg_serve_http_opts s_http_server_opts;

static bool show = true;

static void signal_handler(int sig_num)
{
    signal(sig_num, signal_handler);  // Reinstantiate signal handler
    s_signal_received = sig_num;
}

static void compute(cv::Mat& frame, wscv::Response& res)
{
    cv::flip(frame, frame, 0);
    
    //cv::cvtColor(frame, frame, cv::COLOR_BGR2HLS);
    //cv::inRange(frame, cv::Scalar(0, 0.1 * 255, 0.05 * 255), cv::Scalar(30 , 0.8 * 255, 0.6 * 255), frame);
    //cv::blur(frame, frame, cv::Size(10, 10));
    //cv::threshold(frame, frame, 200, 255, cv::THRESH_BINARY);

    //if(show) cv::imshow("camera", frame);
    //if((cv::waitKey(30) & 0xff) == 27) show = false;

    cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
    cv::CascadeClassifier face_cascade(DATA_DIR"haarcascade_frontalface_default.xml");

    std::vector<cv::Rect> faces;
    face_cascade.detectMultiScale(frame, faces);

    if(faces.empty()) res.set_status("failure");
    else res.set_status("success");

}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    switch (ev)
    {
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
        {
            cout << "Bonjour Unity" << endl;
            break;
        }
        case MG_EV_WEBSOCKET_FRAME:
        {
            struct websocket_message *wm = (struct websocket_message *) ev_data;

            if(wm->flags & WEBSOCKET_OP_BINARY)
            {
                wscv::Request req;

                req.ParseFromArray(wm->data, wm->size);
                cout << "Protobuf frame : " << req.width() << "x" << req.height() << " = " << req.content().size() << " bytes" << endl;

                cv::Mat frame(req.height(), req.width(), CV_8UC3, (unsigned char*)req.content().data());
                wscv::Response res;

                compute(frame, res);

                auto msg = res.SerializeAsString();

                mg_send_websocket_frame(nc, WEBSOCKET_OP_BINARY, msg.c_str(), msg.size());
            }

            break;
        }
        case MG_EV_HTTP_REQUEST:
        {
            mg_serve_http(nc, (struct http_message *) ev_data, s_http_server_opts);
            break;
        }
        case MG_EV_CLOSE:
        {
            break;
        }
    }
}

int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    struct mg_mgr mgr;
    struct mg_connection *nc;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, s_http_port, ev_handler);
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = WWW_DIR;  // Serve current directory
    s_http_server_opts.enable_directory_listing = "yes";

    printf("Started on port %s\n", s_http_port);
    while (s_signal_received == 0)
    {
        mg_mgr_poll(&mgr, 200);
    }
    mg_mgr_free(&mgr);
    google::protobuf::ShutdownProtobufLibrary();

    return EXIT_SUCCESS;
}
